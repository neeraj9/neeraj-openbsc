/* OpenBSC Debugging/Logging support code */

/* (C) 2008-2010 by Harald Welte <laforge@gnumonks.org>
 * (C) 2008 by Holger Hans Peter Freyther <zecke@selfish.org>
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>

#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include <openbsc/gsm_data.h>
#include <openbsc/gsm_subscriber.h>
#include <openbsc/debug.h>

/* default categories */
static const struct log_info_cat default_categories[] = {
	[DRLL] = {
		.name = "DRLL",
		.description = "A-bis Radio Link Layer (RLL)",
		.color = "\033[1;31m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DCC] = {
		.name = "DCC",
		.description = "Layer3 Call Control (CC)",
		.color = "\033[1;32m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DMM] = {
		.name = "DMM",
		.description = "Layer3 Mobility Management (MM)",
		.color = "\033[1;33m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DRR] = {
		.name = "DRR",
		.description = "Layer3 Radio Resource (RR)",
		.color = "\033[1;34m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DRSL] = {
		.name = "DRSL",
		.description = "A-bis Radio Siganlling Link (RSL)",
		.color = "\033[1;35m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DNM] =	{
		.name = "DNM",
		.description = "A-bis Network Management / O&M (NM/OML)",
		.color = "\033[1;36m",
		.enabled = 1, .loglevel = LOGL_INFO,
	},
	[DMNCC] = {
		.name = "DMNCC",
		.description = "MNCC API for Call Control application",
		.color = "\033[1;39m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DSMS] = {
		.name = "DSMS",
		.description = "Layer3 Short Message Service (SMS)",
		.color = "\033[1;37m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DPAG]	= {
		.name = "DPAG",
		.description = "Paging Subsystem",
		.color = "\033[1;38m",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DMEAS] = {
		.name = "DMEAS",
		.description = "Radio Measurement Processing",
		.enabled = 0, .loglevel = LOGL_NOTICE,
	},
	[DSCCP] = {
		.name = "DSCCP",
		.description = "SCCP Protocol",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DMSC] = {
		.name = "DMSC",
		.description = "Mobile Switching Center",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DMGCP] = {
		.name = "DMGCP",
		.description = "Media Gateway Control Protocol",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DHO] = {
		.name = "DHO",
		.description = "Hand-Over",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DDB] = {
		.name = "DDB",
		.description = "Database Layer",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DREF] = {
		.name = "DREF",
		.description = "Reference Counting",
		.enabled = 0, .loglevel = LOGL_NOTICE,
	},
	[DGPRS] = {
		.name = "DGPRS",
		.description = "GPRS Packet Service",
		.enabled = 1, .loglevel = LOGL_DEBUG,
	},
	[DNS] = {
		.name = "DNS",
		.description = "GPRS Network Service (NS)",
		.enabled = 1, .loglevel = LOGL_INFO,
	},
	[DBSSGP] = {
		.name = "DBSSGP",
		.description = "GPRS BSS Gateway Protocol (BSSGP)",
		.enabled = 1, .loglevel = LOGL_DEBUG,
	},
	[DLLC] = {
		.name = "DLLC",
		.description = "GPRS Logical Link Control Protocol (LLC)",
		.enabled = 1, .loglevel = LOGL_DEBUG,
	},
	[DSNDCP] = {
		.name = "DSNDCP",
		.description = "GPRS Sub-Network Dependent Control Protocol (SNDCP)",
		.enabled = 1, .loglevel = LOGL_DEBUG,
	},
	[DNAT] = {
		.name = "DNAT",
		.description = "GSM 08.08 NAT/Multipkexer",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DCTRL] = {
		.name = "DCTRL",
		.description = "Control interface",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
};

enum log_filter {
	_FLT_ALL = LOG_FILTER_ALL,	/* libosmocore */
	FLT_IMSI = 1,
	FLT_NSVC = 2,
	FLT_BVC  = 3,
};

static int filter_fn(const struct log_context *ctx,
		     struct log_target *tar)
{
	struct gsm_subscriber *subscr = ctx->ctx[BSC_CTX_SUBSCR];
	const struct gprs_nsvc *nsvc = ctx->ctx[BSC_CTX_NSVC];
	const struct gprs_nsvc *bvc = ctx->ctx[BSC_CTX_BVC];

	if ((tar->filter_map & (1 << FLT_IMSI)) != 0
	    && subscr && strcmp(subscr->imsi, tar->filter_data[FLT_IMSI]) == 0)
		return 1;

	/* Filter on the NS Virtual Connection */
	if ((tar->filter_map & (1 << FLT_NSVC)) != 0
	    && nsvc && (nsvc == tar->filter_data[FLT_NSVC]))
		return 1;

	/* Filter on the NS Virtual Connection */
	if ((tar->filter_map & (1 << FLT_BVC)) != 0
	    && bvc && (bvc == tar->filter_data[FLT_BVC]))
		return 1;

	return 0;
}

const struct log_info log_info = {
	.filter_fn = filter_fn,
	.cat = default_categories,
	.num_cat = ARRAY_SIZE(default_categories),
};

void log_set_imsi_filter(struct log_target *target, const char *imsi)
{
	if (imsi) {
		target->filter_map |= (1 << FLT_IMSI);
		target->filter_data[FLT_IMSI] = talloc_strdup(target, imsi);
	} else if (target->filter_data[FLT_IMSI]) {
		target->filter_map &= ~(1 << FLT_IMSI);
		talloc_free(target->filter_data[FLT_IMSI]);
		target->filter_data[FLT_IMSI] = NULL;
	}
}

void log_set_nsvc_filter(struct log_target *target, struct gprs_nsvc *nsvc)
{
	if (nsvc) {
		target->filter_map |= (1 << FLT_NSVC);
		target->filter_data[FLT_NSVC] = nsvc;
	} else if (target->filter_data[FLT_NSVC]) {
		target->filter_map = ~(1 << FLT_NSVC);
		target->filter_data[FLT_NSVC] = NULL;
	}
}

void log_set_bvc_filter(struct log_target *target, struct bssgp_bvc_ctx *bctx)
{
	if (bctx) {
		target->filter_map |= (1 << FLT_BVC);
		target->filter_data[FLT_BVC] = bctx;
	} else if (target->filter_data[FLT_NSVC]) {
		target->filter_map = ~(1 << FLT_BVC);
		target->filter_data[FLT_BVC] = NULL;
	}
}
