/* A hackish minimal BSC (+MSC +HLR) implementation */

/* (C) 2008-2010 by Harald Welte <laforge@gnumonks.org>
 * (C) 2009 by Holger Hans Peter Freyther <zecke@selfish.org>
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

#include <openbsc/gsm_data.h>
#include <osmocom/gsm/gsm_utils.h>
#include <openbsc/gsm_04_08.h>
#include <openbsc/abis_rsl.h>
#include <openbsc/abis_nm.h>
#include <openbsc/debug.h>
#include <openbsc/misdn.h>
#include <osmocom/vty/telnet_interface.h>
#include <openbsc/system_information.h>
#include <openbsc/paging.h>
#include <openbsc/signal.h>
#include <openbsc/chan_alloc.h>
#include <osmocom/core/talloc.h>
#include <openbsc/ipaccess.h>

/* global pointer to the gsm network data structure */
extern struct gsm_network *bsc_gsmnet;
extern int hsl_setup(struct gsm_network *gsmnet);

/* Callback function for NACK on the OML NM */
static int oml_msg_nack(struct nm_nack_signal_data *nack)
{
	int i;

	if (nack->mt == NM_MT_SET_BTS_ATTR_NACK) {

		LOGP(DNM, LOGL_FATAL, "Failed to set BTS attributes. That is fatal. "
				"Was the bts type and frequency properly specified?\n");
		exit(-1);
	} else {
		LOGP(DNM, LOGL_ERROR, "Got a NACK going to drop the OML links.\n");
		for (i = 0; i < bsc_gsmnet->num_bts; ++i) {
			struct gsm_bts *bts = gsm_bts_num(bsc_gsmnet, i);
			if (is_ipaccess_bts(bts))
				ipaccess_drop_oml(bts);
		}
	}

	return 0;
}

/* Callback function to be called every time we receive a signal from NM */
static int nm_sig_cb(unsigned int subsys, unsigned int signal,
		     void *handler_data, void *signal_data)
{
	struct nm_nack_signal_data *nack;

	switch (signal) {
	case S_NM_NACK:
		nack = signal_data;
		return oml_msg_nack(nack);
	default:
		break;
	}
	return 0;
}

int bsc_shutdown_net(struct gsm_network *net)
{
	struct gsm_bts *bts;

	llist_for_each_entry(bts, &net->bts_list, list) {
		LOGP(DNM, LOGL_NOTICE, "shutting down OML for BTS %u\n", bts->nr);
		osmo_signal_dispatch(SS_GLOBAL, S_GLOBAL_BTS_CLOSE_OM, bts);
	}

	return 0;
}

static int generate_and_rsl_si(struct gsm_bts_trx *trx, enum osmo_sysinfo_type i)
{
	struct gsm_bts *bts = trx->bts;
	int si_len, rc, j;

	/* Only generate SI if this SI is not in "static" (user-defined) mode */
	if (!(bts->si_mode_static & (1 << i))) {
		rc = gsm_generate_si(bts, i);
		if (rc < 0)
			return rc;
		si_len = rc;
	}

	DEBUGP(DRR, "SI%s: %s\n", get_value_string(osmo_sitype_strs, i),
		osmo_hexdump(GSM_BTS_SI(bts, i), GSM_MACBLOCK_LEN));

	switch (i) {
	case SYSINFO_TYPE_5:
	case SYSINFO_TYPE_5bis:
	case SYSINFO_TYPE_5ter:
	case SYSINFO_TYPE_6:
		if (trx->bts->type == GSM_BTS_TYPE_HSL_FEMTO) {
			/* HSL has mistaken SACCH INFO MODIFY for SACCH FILLING,
			 * so we need a special workaround here */
			/* This assumes a combined BCCH and TCH on TS1...7 */
			for (j = 0; j < 4; j++)
				rsl_sacch_info_modify(&trx->ts[0].lchan[j],
						      osmo_sitype2rsl(i),
						      GSM_BTS_SI(bts, i), si_len);
			for (j = 1; j < 8; j++) {
				rsl_sacch_info_modify(&trx->ts[j].lchan[0],
						      osmo_sitype2rsl(i),
						      GSM_BTS_SI(bts, i), si_len);
				rsl_sacch_info_modify(&trx->ts[j].lchan[1],
						      osmo_sitype2rsl(i),
						      GSM_BTS_SI(bts, i), si_len);
			}
		} else
			rc = rsl_sacch_filling(trx, osmo_sitype2rsl(i),
					       GSM_BTS_SI(bts, i), rc);
		break;
	default:
		rc = rsl_bcch_info(trx, osmo_sitype2rsl(i),
				   GSM_BTS_SI(bts, i), rc);
		break;
	}

	return rc;
}

/* set all system information types */
static int set_system_infos(struct gsm_bts_trx *trx)
{
	int i, rc;
	struct gsm_bts *bts = trx->bts;

	bts->si_common.cell_sel_par.ms_txpwr_max_ccch =
			ms_pwr_ctl_lvl(bts->band, bts->ms_max_power);
	bts->si_common.cell_sel_par.neci = bts->network->neci;

	/* First, we determine which of the SI messages we actually need */

	if (trx == bts->c0) {
		/* 1...4 are always present on a C0 TRX */
		for (i = SYSINFO_TYPE_1; i <= SYSINFO_TYPE_4; i++)
			bts->si_valid |= (1 << i);

		/* 13 is always present on a C0 TRX of a GPRS BTS */
		if (bts->gprs.mode != BTS_GPRS_NONE)
			bts->si_valid |= (1 << SYSINFO_TYPE_13);
	}

	/* 5 and 6 are always present on every TRX */
	bts->si_valid |= (1 << SYSINFO_TYPE_5);
	bts->si_valid |= (1 << SYSINFO_TYPE_6);

	/* Second, we generate and send the selected SI via RSL */
	for (i = SYSINFO_TYPE_1; i < _MAX_SYSINFO_TYPE; i++) {
		if (!(bts->si_valid & (1 << i)))
			continue;

		rc = generate_and_rsl_si(trx, i);
		if (rc < 0)
			goto err_out;
	}

	return 0;
err_out:
	LOGP(DRR, LOGL_ERROR, "Cannot generate SI %u for BTS %u, most likely "
		"a problem with neighbor cell list generation\n",
		i, bts->nr);
	return rc;
}

/* Produce a MA as specified in 10.5.2.21 */
static int generate_ma_for_ts(struct gsm_bts_trx_ts *ts)
{
	/* we have three bitvecs: the per-timeslot ARFCNs, the cell chan ARFCNs
	 * and the MA */
	struct bitvec *cell_chan = &ts->trx->bts->si_common.cell_alloc;
	struct bitvec *ts_arfcn = &ts->hopping.arfcns;
	struct bitvec *ma = &ts->hopping.ma;
	unsigned int num_cell_arfcns, bitnum, n_chan;
	int i;

	/* re-set the MA to all-zero */
	ma->cur_bit = 0;
	ts->hopping.ma_len = 0;
	memset(ma->data, 0, ma->data_len);

	if (!ts->hopping.enabled)
		return 0;

	/* count the number of ARFCNs in the cell channel allocation */
	num_cell_arfcns = 0;
	for (i = 1; i < 1024; i++) {
		if (bitvec_get_bit_pos(cell_chan, i))
			num_cell_arfcns++;
	}

	/* pad it to octet-aligned number of bits */
	ts->hopping.ma_len = num_cell_arfcns / 8;
	if (num_cell_arfcns % 8)
		ts->hopping.ma_len++;

	n_chan = 0;
	for (i = 1; i < 1024; i++) {
		if (!bitvec_get_bit_pos(cell_chan, i))
			continue;
		/* set the corresponding bit in the MA */
		bitnum = (ts->hopping.ma_len * 8) - 1 - n_chan;
		if (bitvec_get_bit_pos(ts_arfcn, i))
			bitvec_set_bit_pos(ma, bitnum, 1);
		else
			bitvec_set_bit_pos(ma, bitnum, 0);
		n_chan++;
	}

	/* ARFCN 0 is special: It is coded last in the bitmask */
	if (bitvec_get_bit_pos(cell_chan, 0)) {
		n_chan++;
		/* set the corresponding bit in the MA */
		bitnum = (ts->hopping.ma_len * 8) - 1 - n_chan;
		if (bitvec_get_bit_pos(ts_arfcn, 0))
			bitvec_set_bit_pos(ma, bitnum, 1);
		else
			bitvec_set_bit_pos(ma, bitnum, 0);
	}

	return 0;
}

static void bootstrap_rsl(struct gsm_bts_trx *trx)
{
	unsigned int i;

	LOGP(DRSL, LOGL_NOTICE, "bootstrapping RSL for BTS/TRX (%u/%u) "
		"on ARFCN %u using MCC=%u MNC=%u LAC=%u CID=%u BSIC=%u TSC=%u\n",
		trx->bts->nr, trx->nr, trx->arfcn, bsc_gsmnet->country_code,
		bsc_gsmnet->network_code, trx->bts->location_area_code,
		trx->bts->cell_identity, trx->bts->bsic, trx->bts->tsc);
	set_system_infos(trx);

	for (i = 0; i < ARRAY_SIZE(trx->ts); i++)
		generate_ma_for_ts(&trx->ts[i]);
}

/* Callback function to be called every time we receive a signal from INPUT */
static int inp_sig_cb(unsigned int subsys, unsigned int signal,
		      void *handler_data, void *signal_data)
{
	struct input_signal_data *isd = signal_data;
	struct gsm_bts_trx *trx = isd->trx;
	int ts_no, lchan_no;

	if (subsys != SS_INPUT)
		return -EINVAL;

	switch (signal) {
	case S_INP_TEI_UP:
		if (isd->link_type == E1INP_SIGN_RSL)
			bootstrap_rsl(trx);
		break;
	case S_INP_TEI_DN:
		LOGP(DMI, LOGL_ERROR, "Lost some E1 TEI link: %d %p\n", isd->link_type, trx);

		if (isd->link_type == E1INP_SIGN_OML)
			osmo_counter_inc(trx->bts->network->stats.bts.oml_fail);
		else if (isd->link_type == E1INP_SIGN_RSL)
			osmo_counter_inc(trx->bts->network->stats.bts.rsl_fail);

		/*
		 * free all allocated channels. change the nm_state so the
		 * trx and trx_ts becomes unusable and chan_alloc.c can not
		 * allocate from it.
		 */
		for (ts_no = 0; ts_no < ARRAY_SIZE(trx->ts); ++ts_no) {
			struct gsm_bts_trx_ts *ts = &trx->ts[ts_no];

			for (lchan_no = 0; lchan_no < ARRAY_SIZE(ts->lchan); ++lchan_no) {
				if (ts->lchan[lchan_no].state != LCHAN_S_NONE)
					lchan_free(&ts->lchan[lchan_no]);
				lchan_reset(&ts->lchan[lchan_no]);
			}
		}

		gsm_bts_mo_reset(trx->bts);

		abis_nm_clear_queue(trx->bts);
		break;
	default:
		break;
	}

	return 0;
}

static int bootstrap_bts(struct gsm_bts *bts)
{
	int i, n;

	if (bts->model->start && !bts->model->started) {
		int ret = bts->model->start(bts->network);
		if (ret < 0)
			return ret;

		bts->model->started = true;
	}

	/* FIXME: What about secondary TRX of a BTS?  What about a BTS that has TRX
	 * in different bands? Why is 'band' a parameter of the BTS and not of the TRX? */
	switch (bts->band) {
	case GSM_BAND_1800:
		if (bts->c0->arfcn < 512 || bts->c0->arfcn > 885) {
			LOGP(DNM, LOGL_ERROR, "GSM1800 channel must be between 512-885.\n");
			return -EINVAL;
		}
		break;
	case GSM_BAND_1900:
		if (bts->c0->arfcn < 512 || bts->c0->arfcn > 810) {
			LOGP(DNM, LOGL_ERROR, "GSM1900 channel must be between 512-810.\n");
			return -EINVAL;
		}
		break;
	case GSM_BAND_900:
		if (bts->c0->arfcn < 1 ||
		   (bts->c0->arfcn > 124 && bts->c0->arfcn < 955) ||
		    bts->c0->arfcn > 1023)  {
			LOGP(DNM, LOGL_ERROR, "GSM900 channel must be between 1-124, 955-1023.\n");
			return -EINVAL;
		}
		break;
	case GSM_BAND_850:
		if (bts->c0->arfcn < 128 || bts->c0->arfcn > 251) {
			LOGP(DNM, LOGL_ERROR, "GSM850 channel must be between 128-251.\n");
			return -EINVAL;
		}
		break;
	default:
		LOGP(DNM, LOGL_ERROR, "Unsupported frequency band.\n");
		return -EINVAL;
	}

	if (bts->network->auth_policy == GSM_AUTH_POLICY_ACCEPT_ALL &&
	    !bts->si_common.rach_control.cell_bar)
		LOGP(DNM, LOGL_ERROR, "\nWARNING: You are running an 'accept-all' "
			"network on a BTS that is not barred.  This "
			"configuration is likely to interfere with production "
			"GSM networks and should only be used in a RF "
			"shielded environment such as a faraday cage!\n\n");

	/* Control Channel Description */
	bts->si_common.chan_desc.att = 1;
	bts->si_common.chan_desc.bs_pa_mfrms = RSL_BS_PA_MFRMS_5;
	bts->si_common.chan_desc.bs_ag_blks_res = 1;

	/* T3212 is set from vty/config */

	/* Set ccch config by looking at ts config */
	for (n=0, i=0; i<8; i++)
		n += bts->c0->ts[i].pchan == GSM_PCHAN_CCCH ? 1 : 0;

	switch (n) {
	case 0:
		bts->si_common.chan_desc.ccch_conf = RSL_BCCH_CCCH_CONF_1_C;
		break;
	case 1:
		bts->si_common.chan_desc.ccch_conf = RSL_BCCH_CCCH_CONF_1_NC;
		break;
	case 2:
		bts->si_common.chan_desc.ccch_conf = RSL_BCCH_CCCH_CONF_2_NC;
		break;
	case 3:
		bts->si_common.chan_desc.ccch_conf = RSL_BCCH_CCCH_CONF_3_NC;
		break;
	case 4:
		bts->si_common.chan_desc.ccch_conf = RSL_BCCH_CCCH_CONF_4_NC;
		break;
	default:
		LOGP(DNM, LOGL_ERROR, "Unsupported CCCH timeslot configuration\n");
		return -EINVAL;
	}

	/* some defaults for our system information */
	bts->si_common.cell_options.radio_link_timeout = 7; /* 12 */

	/* allow/disallow DTXu */
	if (bts->network->dtx_enabled)
		bts->si_common.cell_options.dtx = 0;
	else
		bts->si_common.cell_options.dtx = 2;

	bts->si_common.cell_options.pwrc = 0; /* PWRC not set */

	bts->si_common.cell_sel_par.acs = 0;

	bts->si_common.ncc_permitted = 0xff;

	return 0;
}

int bsc_bootstrap_network(int (*mncc_recv)(struct gsm_network *, struct msgb *),
			  const char *config_file)
{
	struct telnet_connection dummy_conn;
	struct gsm_bts *bts;
	int rc;

	/* initialize our data structures */
	bsc_gsmnet = gsm_network_init(1, 1, mncc_recv);
	if (!bsc_gsmnet)
		return -ENOMEM;

	bsc_gsmnet->name_long = talloc_strdup(bsc_gsmnet, "OpenBSC");
	bsc_gsmnet->name_short = talloc_strdup(bsc_gsmnet, "OpenBSC");

	/* our vty command code expects vty->priv to point to a telnet_connection */
	dummy_conn.priv = bsc_gsmnet;
	rc = vty_read_config_file(config_file, &dummy_conn);
	if (rc < 0) {
		LOGP(DNM, LOGL_FATAL, "Failed to parse the config file: '%s'\n", config_file);
		return rc;
	}

	rc = telnet_init(tall_bsc_ctx, bsc_gsmnet, 4242);
	if (rc < 0)
		return rc;

	osmo_signal_register_handler(SS_NM, nm_sig_cb, NULL);
	osmo_signal_register_handler(SS_INPUT, inp_sig_cb, NULL);

	llist_for_each_entry(bts, &bsc_gsmnet->bts_list, list) {
		rc = bootstrap_bts(bts);
		if (rc < 0) {
			LOGP(DNM, LOGL_FATAL, "Error bootstrapping BTS\n");
			return rc;
		}
		switch (bts->type) {
		case GSM_BTS_TYPE_NANOBTS:
		case GSM_BTS_TYPE_HSL_FEMTO:
			break;
		default:
			rc = e1_reconfig_bts(bts);
			break;
		}
		if (rc < 0) {
			LOGP(DNM, LOGL_FATAL, "Error enabling E1 input driver\n");
			return rc;
		}
	}
	return 0;
}
