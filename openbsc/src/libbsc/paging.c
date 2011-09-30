/* Paging helper and manager.... */
/* (C) 2009 by Holger Hans Peter Freyther <zecke@selfish.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Relevant specs:
 *     12.21:
 *       - 9.4.12 for CCCH Local Threshold
 *
 *     05.58:
 *       - 8.5.2 CCCH Load indication
 *       - 9.3.15 Paging Load
 *
 * Approach:
 *       - Send paging command to subscriber
 *       - On Channel Request we will remember the reason
 *       - After the ACK we will request the identity
 *	 - Then we will send assign the gsm_subscriber and
 *	 - and call a callback
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <osmocom/core/talloc.h>
#include <osmocom/gsm/gsm48.h>
#include <osmocom/gsm/gsm0502.h>

#include <openbsc/paging.h>
#include <openbsc/debug.h>
#include <openbsc/signal.h>
#include <openbsc/abis_rsl.h>
#include <openbsc/gsm_data.h>
#include <openbsc/chan_alloc.h>
#include <openbsc/bsc_api.h>

void *tall_paging_ctx;

#define PAGING_TIMER 0, 500000

/*
 * Kill one paging request update the internal list...
 */
static void paging_remove_request(struct gsm_bts_paging_state *paging_bts,
				struct gsm_paging_request *to_be_deleted)
{
	osmo_timer_del(&to_be_deleted->T3113);
	llist_del(&to_be_deleted->entry);
	subscr_put(to_be_deleted->subscr);
	talloc_free(to_be_deleted);
}

static void page_ms(struct gsm_paging_request *request)
{
	uint8_t mi[128];
	unsigned int mi_len;
	unsigned int page_group;
	struct gsm_bts *bts = request->bts;

	LOGP(DPAG, LOGL_INFO, "Going to send paging commands: imsi: '%s' tmsi: '0x%x'\n",
		request->subscr->imsi, request->subscr->tmsi);

	if (request->subscr->tmsi == GSM_RESERVED_TMSI)
		mi_len = gsm48_generate_mid_from_imsi(mi, request->subscr->imsi);
	else
		mi_len = gsm48_generate_mid_from_tmsi(mi, request->subscr->tmsi);

	page_group = gsm0502_calc_paging_group(&bts->si_common.chan_desc,
						str_to_imsi(request->subscr->imsi));
	gsm0808_page(bts, page_group, mi_len, mi, request->chan_type);
}

static void paging_schedule_if_needed(struct gsm_bts_paging_state *paging_bts)
{
	if (llist_empty(&paging_bts->pending_requests))
		return;

	if (!osmo_timer_pending(&paging_bts->work_timer))
		osmo_timer_schedule(&paging_bts->work_timer, PAGING_TIMER);
}


static void paging_handle_pending_requests(struct gsm_bts_paging_state *paging_bts);
static void paging_give_credit(void *data)
{
	struct gsm_bts_paging_state *paging_bts = data;

	LOGP(DPAG, LOGL_NOTICE, "No slots available on bts nr %d\n", paging_bts->bts->nr);
	paging_bts->available_slots = 20;
	paging_handle_pending_requests(paging_bts);
}

static int can_send_pag_req(struct gsm_bts *bts, int rsl_type)
{
	struct pchan_load pl;
	int count;

	memset(&pl, 0, sizeof(pl));
	bts_chan_load(&pl, bts);

	switch (rsl_type) {
	case RSL_CHANNEED_TCH_F:
	case RSL_CHANNEED_TCH_ForH:
		goto count_tch;
		break;
	case RSL_CHANNEED_SDCCH:
		goto count_sdcch;
		break;
	case RSL_CHANNEED_ANY:
	default:
		if (bts->network->pag_any_tch)
			goto count_tch;
		else
			goto count_sdcch;
		break;
	}

	return 0;

	/* could available SDCCH */
count_sdcch:
	count = 0;
	count += pl.pchan[GSM_PCHAN_SDCCH8_SACCH8C].total
			- pl.pchan[GSM_PCHAN_SDCCH8_SACCH8C].used;
	count += pl.pchan[GSM_PCHAN_CCCH_SDCCH4].total
			- pl.pchan[GSM_PCHAN_CCCH_SDCCH4].used;
	return bts->paging.free_chans_need > count;

count_tch:
	count = 0;
	count += pl.pchan[GSM_PCHAN_TCH_F].total
			- pl.pchan[GSM_PCHAN_TCH_F].used;
	if (bts->network->neci)
		count += pl.pchan[GSM_PCHAN_TCH_H].total
				- pl.pchan[GSM_PCHAN_TCH_H].used;
	return bts->paging.free_chans_need > count;
}

/*
 * This is kicked by the periodic PAGING LOAD Indicator
 * coming from abis_rsl.c
 *
 * We attempt to iterate once over the list of items but
 * only upto available_slots.
 */
static void paging_handle_pending_requests(struct gsm_bts_paging_state *paging_bts)
{
	struct gsm_paging_request *request = NULL;

	/*
	 * Determine if the pending_requests list is empty and
	 * return then.
	 */
	if (llist_empty(&paging_bts->pending_requests)) {
		/* since the list is empty, no need to reschedule the timer */
		return;
	}

	/*
	 * In case the BTS does not provide us with load indication and we
	 * ran out of slots, call an autofill routine. It might be that the
	 * BTS did not like our paging messages and then we have counted down
	 * to zero and we do not get any messages.
	 */
	if (paging_bts->available_slots == 0) {
		paging_bts->credit_timer.cb = paging_give_credit;
		paging_bts->credit_timer.data = paging_bts;
		osmo_timer_schedule(&paging_bts->credit_timer, 5, 0);
		return;
	}

	request = llist_entry(paging_bts->pending_requests.next,
			      struct gsm_paging_request, entry);

	/* we need to determine the number of free channels */
	if (paging_bts->free_chans_need != -1) {
		if (can_send_pag_req(request->bts, request->chan_type) != 0)
			goto skip_paging;
	}

	/* handle the paging request now */
	page_ms(request);
	paging_bts->available_slots--;
	request->attempts++;

	/* take the current and add it to the back */
	llist_del(&request->entry);
	llist_add_tail(&request->entry, &paging_bts->pending_requests);

skip_paging:
	osmo_timer_schedule(&paging_bts->work_timer, PAGING_TIMER);
}

static void paging_worker(void *data)
{
	struct gsm_bts_paging_state *paging_bts = data;

	paging_handle_pending_requests(paging_bts);
}

static void paging_init_if_needed(struct gsm_bts *bts)
{
	if (bts->paging.bts)
		return;

	bts->paging.bts = bts;
	INIT_LLIST_HEAD(&bts->paging.pending_requests);
	bts->paging.work_timer.cb = paging_worker;
	bts->paging.work_timer.data = &bts->paging;

	/* Large number, until we get a proper message */
	bts->paging.available_slots = 20;
}

static int paging_pending_request(struct gsm_bts_paging_state *bts,
				struct gsm_subscriber *subscr) {
	struct gsm_paging_request *req;

	llist_for_each_entry(req, &bts->pending_requests, entry) {
		if (subscr == req->subscr)
			return 1;
	}

	return 0;	
}

static void paging_T3113_expired(void *data)
{
	struct gsm_paging_request *req = (struct gsm_paging_request *)data;
	void *cbfn_param;
	gsm_cbfn *cbfn;
	int msg;

	LOGP(DPAG, LOGL_INFO, "T3113 expired for request %p (%s)\n",
		req, req->subscr->imsi);

	/* must be destroyed before calling cbfn, to prevent double free */
	osmo_counter_inc(req->bts->network->stats.paging.expired);
	cbfn_param = req->cbfn_param;
	cbfn = req->cbfn;

	/* did we ever manage to page the subscriber */
	msg = req->attempts > 0 ? GSM_PAGING_EXPIRED : GSM_PAGING_BUSY;

	/* destroy it now. Do not access req afterwards */
	paging_remove_request(&req->bts->paging, req);

	if (cbfn)
		cbfn(GSM_HOOK_RR_PAGING, msg, NULL, NULL,
			  cbfn_param);
}

static int _paging_request(struct gsm_bts *bts, struct gsm_subscriber *subscr,
			    int type, gsm_cbfn *cbfn, void *data)
{
	struct gsm_bts_paging_state *bts_entry = &bts->paging;
	struct gsm_paging_request *req;

	if (paging_pending_request(bts_entry, subscr)) {
		LOGP(DPAG, LOGL_INFO, "Paging request already pending for %s\n", subscr->imsi);
		return -EEXIST;
	}

	LOGP(DPAG, LOGL_DEBUG, "Start paging of subscriber %llu on bts %d.\n",
		subscr->id, bts->nr);
	req = talloc_zero(tall_paging_ctx, struct gsm_paging_request);
	req->subscr = subscr_get(subscr);
	req->bts = bts;
	req->chan_type = type;
	req->cbfn = cbfn;
	req->cbfn_param = data;
	req->T3113.cb = paging_T3113_expired;
	req->T3113.data = req;
	osmo_timer_schedule(&req->T3113, bts->network->T3113, 0);
	llist_add_tail(&req->entry, &bts_entry->pending_requests);
	paging_schedule_if_needed(bts_entry);

	return 0;
}

int paging_request(struct gsm_network *network, struct gsm_subscriber *subscr,
		   int type, gsm_cbfn *cbfn, void *data)
{
	struct gsm_bts *bts = NULL;
	int num_pages = 0;

	osmo_counter_inc(network->stats.paging.attempted);

	/* start paging subscriber on all BTS within Location Area */
	do {
		int rc;

		bts = gsm_bts_by_lac(network, subscr->lac, bts);
		if (!bts)
			break;

		/* skip all currently inactive TRX */
		if (!trx_is_usable(bts->c0))
			continue;

		/* maybe it is the first time we use it */
		paging_init_if_needed(bts);

		num_pages++;

		/* Trigger paging, pass any error to caller */
		rc = _paging_request(bts, subscr, type, cbfn, data);
		if (rc < 0)
			return rc;
	} while (1);

	if (num_pages == 0)
		osmo_counter_inc(network->stats.paging.detached);

	return num_pages;
}


/* we consciously ignore the type of the request here */
static void _paging_request_stop(struct gsm_bts *bts, struct gsm_subscriber *subscr,
				 struct gsm_subscriber_connection *conn,
				 struct msgb *msg)
{
	struct gsm_bts_paging_state *bts_entry = &bts->paging;
	struct gsm_paging_request *req, *req2;

	paging_init_if_needed(bts);

	llist_for_each_entry_safe(req, req2, &bts_entry->pending_requests,
				 entry) {
		if (req->subscr == subscr) {
			if (conn && req->cbfn) {
				LOGP(DPAG, LOGL_DEBUG, "Stop paging on bts %d, calling cbfn.\n", bts->nr);
				req->cbfn(GSM_HOOK_RR_PAGING, GSM_PAGING_SUCCEEDED,
					  msg, conn, req->cbfn_param);
			} else
				LOGP(DPAG, LOGL_DEBUG, "Stop paging on bts %d silently.\n", bts->nr);
			paging_remove_request(&bts->paging, req);
			break;
		}
	}
}

/* Stop paging on all other bts' */
void paging_request_stop(struct gsm_bts *_bts, struct gsm_subscriber *subscr,
			 struct gsm_subscriber_connection *conn,
			 struct msgb *msg)
{
	struct gsm_bts *bts = NULL;

	if (_bts)
		_paging_request_stop(_bts, subscr, conn, msg);

	do {
		/*
		 * FIXME: Don't use the lac of the subscriber...
		 * as it might have magically changed the lac.. use the
		 * location area of the _bts as reconfiguration of the
		 * network is probably happening less often.
		 */
		bts = gsm_bts_by_lac(subscr->net, subscr->lac, bts);
		if (!bts)
			break;

		/* Stop paging */
		if (bts != _bts)
			_paging_request_stop(bts, subscr, NULL, NULL);
	} while (1);
}

void paging_update_buffer_space(struct gsm_bts *bts, uint16_t free_slots)
{
	paging_init_if_needed(bts);

	osmo_timer_del(&bts->paging.credit_timer);
	bts->paging.available_slots = free_slots;
	paging_schedule_if_needed(&bts->paging);
}

unsigned int paging_pending_requests_nr(struct gsm_bts *bts)
{
	unsigned int requests = 0;
	struct gsm_paging_request *req;

	paging_init_if_needed(bts);

	llist_for_each_entry(req, &bts->paging.pending_requests, entry)
		++requests;

	return requests;
}

/**
 * Find any paging data for the given subscriber at the given BTS.
 */
void *paging_get_data(struct gsm_bts *bts, struct gsm_subscriber *subscr)
{
	struct gsm_paging_request *req;

	llist_for_each_entry(req, &bts->paging.pending_requests, entry)
		if (req->subscr == subscr)
			return req->cbfn_param;

	return NULL;
}
