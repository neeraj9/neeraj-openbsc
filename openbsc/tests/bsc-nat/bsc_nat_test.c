/*
 * BSC NAT Message filtering
 *
 * (C) 2010 by Holger Hans Peter Freyther <zecke@selfish.org>
 * (C) 2010 by On-Waves
 *
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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <openbsc/debug.h>
#include <openbsc/gsm_data.h>
#include <openbsc/bsc_nat.h>
#include <openbsc/bsc_nat_sccp.h>

#include <osmocom/core/application.h>
#include <osmocom/core/talloc.h>

#include <osmocom/sccp/sccp.h>
#include <osmocom/gsm/protocol/gsm_08_08.h>

#include <stdio.h>

/* test messages for ipa */
static uint8_t ipa_id[] = {
	0x00, 0x01, 0xfe, 0x06,
};

/* SCCP messages are below */
static uint8_t gsm_reset[] = {
	0x00, 0x12, 0xfd,
	0x09, 0x00, 0x03, 0x05, 0x07, 0x02, 0x42, 0xfe,
	0x02, 0x42, 0xfe, 0x06, 0x00, 0x04, 0x30, 0x04,
	0x01, 0x20,
};

static const uint8_t gsm_reset_ack[] = {
	0x00, 0x13, 0xfd,
	0x09, 0x00, 0x03, 0x07, 0x0b, 0x04, 0x43, 0x01,
	0x00, 0xfe, 0x04, 0x43, 0x5c, 0x00, 0xfe, 0x03,
	0x00, 0x01, 0x31,
};

static const uint8_t gsm_paging[] = {
	0x00, 0x20, 0xfd,
	0x09, 0x00, 0x03, 0x07, 0x0b, 0x04, 0x43, 0x01,
	0x00, 0xfe, 0x04, 0x43, 0x5c, 0x00, 0xfe, 0x10,
	0x00, 0x0e, 0x52, 0x08, 0x08, 0x29, 0x47, 0x10,
	0x02, 0x01, 0x31, 0x97, 0x61, 0x1a, 0x01, 0x06,
};

/* BSC -> MSC connection open */
static const uint8_t bssmap_cr[] = {
	0x00, 0x2c, 0xfd,
	0x01, 0x01, 0x02, 0x03, 0x02, 0x02, 0x04, 0x02,
	0x42, 0xfe, 0x0f, 0x1f, 0x00, 0x1d, 0x57, 0x05,
	0x08, 0x00, 0x72, 0xf4, 0x80, 0x20, 0x12, 0xc3,
	0x50, 0x17, 0x10, 0x05, 0x24, 0x11, 0x03, 0x33,
	0x19, 0xa2, 0x08, 0x29, 0x47, 0x10, 0x02, 0x01,
	0x31, 0x97, 0x61, 0x00
};

/* MSC -> BSC connection confirm */
static const uint8_t bssmap_cc[] = {
	0x00, 0x0a, 0xfd,
	0x02, 0x01, 0x02, 0x03, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00,
};

/* MSC -> BSC released */
static const uint8_t bssmap_released[] = {
	0x00, 0x0e, 0xfd,
	0x04, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03, 0x00, 0x01, 0x0f,
	0x02, 0x23, 0x42, 0x00,
};

/* BSC -> MSC released */
static const uint8_t bssmap_release_complete[] = {
	0x00, 0x07, 0xfd,
	0x05, 0x01, 0x02, 0x03, 0x00, 0x00, 0x03
};

/* both directions IT timer */
static const uint8_t connnection_it[] = {
	0x00, 0x0b, 0xfd,
	0x10, 0x01, 0x02, 0x03, 0x01, 0x02, 0x03,
	0x00, 0x00, 0x00, 0x00,
};

/* error in both directions */
static const uint8_t proto_error[] = {
	0x00, 0x05, 0xfd,
	0x0f, 0x22, 0x33, 0x44, 0x00,
};

/* MGCP wrap... */
static const uint8_t mgcp_msg[] = {
	0x00, 0x03, 0xfc,
	0x20, 0x20, 0x20,
};

/* location updating request */
static const uint8_t bss_lu[] = {
	0x00, 0x2e, 0xfd,
	0x01, 0x91, 0x45, 0x14, 0x02, 0x02, 0x04, 0x02,
	0x42, 0xfe, 0x0f, 0x21, 0x00, 0x1f, 0x57, 0x05,
	0x08, 0x00, 0x72, 0xf4, 0x80, 0x20, 0x14, 0xc3,
	0x50, 0x17, 0x12, 0x05, 0x08, 0x70, 0x72, 0xf4,
	0x80, 0xff, 0xfe, 0x30, 0x08, 0x29, 0x44, 0x50,
	0x12, 0x03, 0x24, 0x01, 0x95, 0x00
};

/* paging response */
static const uint8_t pag_resp[] = {
	0x00, 0x2c, 0xfd, 0x01, 0xe5, 0x68,
	0x14, 0x02, 0x02, 0x04, 0x02, 0x42, 0xfe, 0x0f,
	0x1f, 0x00, 0x1d, 0x57, 0x05, 0x08, 0x00, 0x72,
	0xf4, 0x80, 0x20, 0x16, 0xc3, 0x50, 0x17, 0x10,
	0x06, 0x27, 0x01, 0x03, 0x30, 0x18, 0x96, 0x08,
	0x29, 0x26, 0x30, 0x32, 0x11, 0x42, 0x01, 0x19,
	0x00
};

struct filter_result {
	const uint8_t *data;
	const uint16_t length;
	const int dir;
	const int result;
};

static const struct filter_result results[] = {
	{
		.data = ipa_id,
		.length = ARRAY_SIZE(ipa_id),
		.dir = DIR_MSC,
		.result = 1,
	},
	{
		.data = gsm_reset,
		.length = ARRAY_SIZE(gsm_reset),
		.dir = DIR_MSC,
		.result = 1,
	},
	{
		.data = gsm_reset_ack,
		.length = ARRAY_SIZE(gsm_reset_ack),
		.dir = DIR_BSC,
		.result = 1,
	},
	{
		.data = gsm_paging,
		.length = ARRAY_SIZE(gsm_paging),
		.dir = DIR_BSC,
		.result = 0,
	},
	{
		.data = bssmap_cr,
		.length = ARRAY_SIZE(bssmap_cr),
		.dir = DIR_MSC,
		.result = 0,
	},
	{
		.data = bssmap_cc,
		.length = ARRAY_SIZE(bssmap_cc),
		.dir = DIR_BSC,
		.result = 0,
	},
	{
		.data = bssmap_released,
		.length = ARRAY_SIZE(bssmap_released),
		.dir = DIR_MSC,
		.result = 0,
	},
	{
		.data = bssmap_release_complete,
		.length = ARRAY_SIZE(bssmap_release_complete),
		.dir = DIR_BSC,
		.result = 0,
	},
	{
		.data = mgcp_msg,
		.length = ARRAY_SIZE(mgcp_msg),
		.dir = DIR_MSC,
		.result = 0,
	},
	{
		.data = connnection_it,
		.length = ARRAY_SIZE(connnection_it),
		.dir = DIR_BSC,
		.result = 0,
	},
	{
		.data = connnection_it,
		.length = ARRAY_SIZE(connnection_it),
		.dir = DIR_MSC,
		.result = 0,
	},
	{
		.data = proto_error,
		.length = ARRAY_SIZE(proto_error),
		.dir = DIR_BSC,
		.result = 0,
	},
	{
		.data = proto_error,
		.length = ARRAY_SIZE(proto_error),
		.dir = DIR_MSC,
		.result = 0,
	},

};

static void test_filter(void)
{
	int i;


	/* start testinh with proper messages */
	fprintf(stderr, "Testing BSS Filtering.\n");
	for (i = 0; i < ARRAY_SIZE(results); ++i) {
		int result;
		struct bsc_nat_parsed *parsed;
		struct msgb *msg = msgb_alloc(4096, "test-message");

		fprintf(stderr, "Going to test item: %d\n", i);
		memcpy(msg->data, results[i].data, results[i].length);
		msg->l2h = msgb_put(msg, results[i].length);

		parsed = bsc_nat_parse(msg);
		if (!parsed) {
			fprintf(stderr, "FAIL: Failed to parse the message\n");
			continue;
		}

		result = bsc_nat_filter_ipa(results[i].dir, msg, parsed);
		if (result != results[i].result) {
			fprintf(stderr, "FAIL: Not the expected result got: %d wanted: %d\n",
				result, results[i].result);
		}

		msgb_free(msg);
	}
}

#include "bsc_data.c"

static void copy_to_msg(struct msgb *msg, const uint8_t *data, unsigned int length)
{
	msgb_reset(msg);
	msg->l2h = msgb_put(msg, length);
	memcpy(msg->l2h, data, msgb_l2len(msg));
}

#define VERIFY(con_found, con, msg, ver, str) \
	if (!con_found || con_found->bsc != con) { \
		fprintf(stderr, "Failed to find the con: %p\n", con_found); \
		abort(); \
	} \
	if (memcmp(msg->data, ver, sizeof(ver)) != 0) { \
		fprintf(stderr, "Failed to patch the %s msg.\n", str); \
		abort(); \
	}

/* test conn tracking once */
static void test_contrack()
{
	struct bsc_nat *nat;
	struct bsc_connection *con;
	struct sccp_connections *con_found;
	struct sccp_connections *rc_con;
	struct bsc_nat_parsed *parsed;
	struct msgb *msg;

	fprintf(stderr, "Testing connection tracking.\n");
	nat = bsc_nat_alloc();
	con = bsc_connection_alloc(nat);
	con->cfg = bsc_config_alloc(nat, "foo");
	bsc_config_add_lac(con->cfg, 23);
	bsc_config_add_lac(con->cfg, 49);
	bsc_config_add_lac(con->cfg, 42);
	bsc_config_del_lac(con->cfg, 49);
	bsc_config_add_lac(con->cfg, 1111);
	msg = msgb_alloc(4096, "test");

	/* 1.) create a connection */
	copy_to_msg(msg, bsc_cr, sizeof(bsc_cr));
	parsed = bsc_nat_parse(msg);
	con_found = patch_sccp_src_ref_to_msc(msg, parsed, con);
	if (con_found != NULL) {
		fprintf(stderr, "Con should not exist %p\n", con_found);
		abort();
	}
	rc_con = create_sccp_src_ref(con, parsed);
	if (!rc_con) {
		fprintf(stderr, "Failed to create a ref\n");
		abort();
	}
	con_found = patch_sccp_src_ref_to_msc(msg, parsed, con);
	if (!con_found || con_found->bsc != con) {
		fprintf(stderr, "Failed to find the con: %p\n", con_found);
		abort();
	}
	if (con_found != rc_con) {
		fprintf(stderr, "Failed to find the right connection.\n");
		abort();
	}
	if (memcmp(msg->data, bsc_cr_patched, sizeof(bsc_cr_patched)) != 0) {
		fprintf(stderr, "Failed to patch the BSC CR msg.\n");
		abort();
	}
	talloc_free(parsed);

	/* 2.) get the cc */
	copy_to_msg(msg, msc_cc, sizeof(msc_cc));
	parsed = bsc_nat_parse(msg);
	con_found = patch_sccp_src_ref_to_bsc(msg, parsed, nat);
	VERIFY(con_found, con, msg, msc_cc_patched, "MSC CC");
	if (update_sccp_src_ref(con_found, parsed) != 0) {
		fprintf(stderr, "Failed to update the SCCP con.\n");
		abort();
	}

	/* 3.) send some data */
	copy_to_msg(msg, bsc_dtap, sizeof(bsc_dtap));
	parsed = bsc_nat_parse(msg);
	con_found = patch_sccp_src_ref_to_msc(msg, parsed, con);
	VERIFY(con_found, con, msg, bsc_dtap_patched, "BSC DTAP");

	/* 4.) receive some data */
	copy_to_msg(msg, msc_dtap, sizeof(msc_dtap));
	parsed = bsc_nat_parse(msg);
	con_found = patch_sccp_src_ref_to_bsc(msg, parsed, nat);
	VERIFY(con_found, con, msg, msc_dtap_patched, "MSC DTAP");

	/* 5.) close the connection */
	copy_to_msg(msg, msc_rlsd, sizeof(msc_rlsd));
	parsed = bsc_nat_parse(msg);
	con_found = patch_sccp_src_ref_to_bsc(msg, parsed, nat);
	VERIFY(con_found, con, msg, msc_rlsd_patched, "MSC RLSD");

	/* 6.) confirm the connection close */
	copy_to_msg(msg, bsc_rlc, sizeof(bsc_rlc));
	parsed = bsc_nat_parse(msg);
	con_found = patch_sccp_src_ref_to_msc(msg, parsed, con);
	if (!con_found || con_found->bsc != con) {
		fprintf(stderr, "Failed to find the con: %p\n", con_found);
		abort();
	}
	if (memcmp(msg->data, bsc_rlc_patched, sizeof(bsc_rlc_patched)) != 0) {
		fprintf(stderr, "Failed to patch the BSC CR msg.\n");
		abort();
	}
	remove_sccp_src_ref(con, msg, parsed);
	talloc_free(parsed);

	copy_to_msg(msg, bsc_rlc, sizeof(bsc_rlc));
	parsed = bsc_nat_parse(msg);
	con_found = patch_sccp_src_ref_to_msc(msg, parsed, con);

	/* verify that it is gone */
	if (con_found != NULL) {
		fprintf(stderr, "Con should be gone. %p\n", con_found);
		abort();
	}
	talloc_free(parsed);


	bsc_config_free(con->cfg);
	talloc_free(nat);
	msgb_free(msg);
}

static void test_paging(void)
{
	struct bsc_nat *nat;
	struct bsc_connection *con;
	struct bsc_config *cfg;

	fprintf(stderr, "Testing paging by lac.\n");

	nat = bsc_nat_alloc();
	con = bsc_connection_alloc(nat);
	cfg = bsc_config_alloc(nat, "unknown");
	con->cfg = cfg;
	bsc_config_add_lac(cfg, 23);
	con->authenticated = 1;
	llist_add(&con->list_entry, &nat->bsc_connections);

	/* Test it by not finding it */
	if (bsc_config_handles_lac(cfg, 8213) != 0) {
		fprintf(stderr, "Should not be handled.\n");
		abort();
	}

	/* Test by finding it */
	bsc_config_del_lac(cfg, 23);
	bsc_config_add_lac(cfg, 8213);
	if (bsc_config_handles_lac(cfg, 8213) == 0) {
		fprintf(stderr, "Should have found it.\n");
		abort();
	}
}

static void test_mgcp_allocations(void)
{
#if 0
	struct bsc_connection *bsc;
	struct bsc_nat *nat;
	struct sccp_connections con;
	int i, j, multiplex;

	fprintf(stderr, "Testing MGCP.\n");
	memset(&con, 0, sizeof(con));

	nat = bsc_nat_alloc();
	nat->bsc_endpoints = talloc_zero_array(nat,
					       struct bsc_endpoint,
					       65);
	nat->mgcp_cfg = mgcp_config_alloc();
	nat->mgcp_cfg->trunk.number_endpoints = 64;

	bsc = bsc_connection_alloc(nat);
	bsc->cfg = bsc_config_alloc(nat, "foo");
	bsc->cfg->max_endpoints = 60;
	bsc_config_add_lac(bsc->cfg, 2323);
	bsc->last_endpoint = 0x22;
	con.bsc = bsc;

	bsc_init_endps_if_needed(bsc);

	i  = 1;
	do {
		if (bsc_assign_endpoint(bsc, &con) != 0) {
			fprintf(stderr, "failed to allocate... on iteration %d\n", i);
			break;
		}
		++i;
	} while(1);

	multiplex = bsc_mgcp_nr_multiplexes(bsc->cfg->max_endpoints);
	for (i = 0; i < multiplex; ++i) {
		for (j = 0; j < 32; ++j)
			printf("%d", bsc->_endpoint_status[i*32 + j]);
		printf(": %d of %d\n", i*32 + 32, 32 * 8);
	}
#endif
}

static void test_mgcp_ass_tracking(void)
{
	struct bsc_connection *bsc;
	struct bsc_nat *nat;
	struct sccp_connections con;
	struct bsc_nat_parsed *parsed;
	struct msgb *msg;

	fprintf(stderr, "Testing MGCP.\n");
	memset(&con, 0, sizeof(con));

	nat = bsc_nat_alloc();
	nat->bsc_endpoints = talloc_zero_array(nat,
					       struct bsc_endpoint,
					       33);
	nat->mgcp_cfg = mgcp_config_alloc();
	nat->mgcp_cfg->trunk.number_endpoints = 64;

	bsc = bsc_connection_alloc(nat);
	bsc->cfg = bsc_config_alloc(nat, "foo");
	bsc_config_add_lac(bsc->cfg, 2323);
	bsc->last_endpoint = 0x1e;
	con.bsc = bsc;

	msg = msgb_alloc(4096, "foo");
	copy_to_msg(msg, ass_cmd, sizeof(ass_cmd));
	parsed = bsc_nat_parse(msg);

	if (msg->l2h[16] != 0 ||
	    msg->l2h[17] != 0x1) {
		fprintf(stderr, "Input is not as expected.. %s 0x%x\n",
			osmo_hexdump(msg->l2h, msgb_l2len(msg)),
			msg->l2h[17]);
		abort();
	}

	if (bsc_mgcp_assign_patch(&con, msg) != 0) {
		fprintf(stderr, "Failed to handle assignment.\n");
		abort();
	}

	if (con.msc_endp != 1) {
		fprintf(stderr, "Timeslot should be 1.\n");
		abort();
	}

	if (con.bsc_endp != 0x1) {
		fprintf(stderr, "Assigned timeslot should have been 1.\n");
		abort();
	}
	if (con.bsc->_endpoint_status[0x1] != 1) {
		fprintf(stderr, "The status on the BSC is wrong.\n");
		abort();
	}

	int multiplex, timeslot;
	mgcp_endpoint_to_timeslot(0x1, &multiplex, &timeslot);

	uint16_t cic = htons(timeslot & 0x1f);
	if (memcmp(&cic, &msg->l2h[16], sizeof(cic)) != 0) {
		fprintf(stderr, "Message was not patched properly\n");
		fprintf(stderr, "data cic: 0x%x %s\n", cic, osmo_hexdump(msg->l2h, msgb_l2len(msg)));
		abort();
	}

	talloc_free(parsed);

	bsc_mgcp_dlcx(&con);
	if (con.bsc_endp != -1 || con.msc_endp != -1 ||
	    con.bsc->_endpoint_status[1] != 0 || con.bsc->last_endpoint != 0x1) {
		fprintf(stderr, "Clearing should remove the mapping.\n");
		abort();
	}

	bsc_config_free(bsc->cfg);
	talloc_free(nat);
}

/* test the code to find a given connection */
static void test_mgcp_find(void)
{
	struct bsc_nat *nat;
	struct bsc_connection *con;
	struct sccp_connections *sccp_con;

	fprintf(stderr, "Testing finding of a BSC Connection\n");

	nat = bsc_nat_alloc();
	con = bsc_connection_alloc(nat);
	llist_add(&con->list_entry, &nat->bsc_connections);

	sccp_con = talloc_zero(con, struct sccp_connections);
	sccp_con->msc_endp = 12;
	sccp_con->bsc_endp = 12;
	sccp_con->bsc = con;
	llist_add(&sccp_con->list_entry, &nat->sccp_connections);

	if (bsc_mgcp_find_con(nat, 11) != NULL) {
		fprintf(stderr, "Found the wrong connection.\n");
		abort();
	}

	if (bsc_mgcp_find_con(nat, 12) != sccp_con) {
		fprintf(stderr, "Didn't find the connection\n");
		abort();
	}

	/* free everything */
	talloc_free(nat);
}

static void test_mgcp_rewrite(void)
{
	int i;
	struct msgb *output;
	fprintf(stderr, "Test rewriting MGCP messages.\n");

	for (i = 0; i < ARRAY_SIZE(mgcp_messages); ++i) {
		const char *orig = mgcp_messages[i].orig;
		const char *patc = mgcp_messages[i].patch;
		const char *ip = mgcp_messages[i].ip;
		const int port = mgcp_messages[i].port;

		char *input = strdup(orig);

		output = bsc_mgcp_rewrite(input, strlen(input), 0x1e, ip, port);
		if (msgb_l2len(output) != strlen(patc)) {
			fprintf(stderr, "Wrong sizes for test: %d  %d != %d != %d\n", i, msgb_l2len(output), strlen(patc), strlen(orig));
			fprintf(stderr, "String '%s' vs '%s'\n", (const char *) output->l2h, patc);
			abort();
		}

		if (memcmp(output->l2h, patc, msgb_l2len(output)) != 0) {
			fprintf(stderr, "Broken on %d msg: '%s'\n", i, (const char *) output->l2h);
			abort();
		}

		msgb_free(output);
		free(input);
	}
}

static void test_mgcp_parse(void)
{
	int code, ci;
	char transaction[60];

	fprintf(stderr, "Test MGCP response parsing.\n");

	if (bsc_mgcp_parse_response(crcx_resp, &code, transaction) != 0) {
		fprintf(stderr, "Failed to parse CRCX resp.\n");
		abort();
	}

	if (code != 200) {
		fprintf(stderr, "Failed to parse the CODE properly. Got: %d\n", code);
		abort();
	}

	if (strcmp(transaction, "23265295") != 0) {
		fprintf(stderr, "Failed to parse transaction id: '%s'\n", transaction);
		abort();
	}

	ci = bsc_mgcp_extract_ci(crcx_resp);
	if (ci != 1) {
		fprintf(stderr, "Failed to parse the CI. Got: %d\n", ci);
		abort();
	}
}

struct cr_filter {
	const uint8_t *data;
	int length;
	int result;
	int contype;

	const char *bsc_imsi_allow;
	const char *bsc_imsi_deny;
	const char *nat_imsi_deny;
};

static struct cr_filter cr_filter[] = {
	{
		.data = bssmap_cr,
		.length = sizeof(bssmap_cr),
		.result = 1,
		.contype = NAT_CON_TYPE_CM_SERV_REQ,
	},
	{
		.data = bss_lu,
		.length = sizeof(bss_lu),
		.result = 1,
		.contype = NAT_CON_TYPE_LU,
	},
	{
		.data = pag_resp,
		.length = sizeof(pag_resp),
		.result = 1,
		.contype = NAT_CON_TYPE_PAG_RESP,
	},
	{
		/* nat deny is before blank/null BSC */
		.data = bss_lu,
		.length = sizeof(bss_lu),
		.result = -3,
		.nat_imsi_deny = "[0-9]*",
		.contype = NAT_CON_TYPE_LU,
	},
	{
		/* BSC allow is before NAT deny */
		.data = bss_lu,
		.length = sizeof(bss_lu),
		.result = 1,
		.nat_imsi_deny = "[0-9]*",
		.bsc_imsi_allow = "2440[0-9]*",
		.contype = NAT_CON_TYPE_LU,
	},
	{
		/* BSC allow is before NAT deny */
		.data = bss_lu,
		.length = sizeof(bss_lu),
		.result = 1,
		.bsc_imsi_allow = "[0-9]*",
		.nat_imsi_deny = "[0-9]*",
		.contype = NAT_CON_TYPE_LU,
	},
	{
		/* filter as deny is first */
		.data = bss_lu,
		.length = sizeof(bss_lu),
		.result = 1,
		.bsc_imsi_deny = "[0-9]*",
		.bsc_imsi_allow = "[0-9]*",
		.nat_imsi_deny = "[0-9]*",
		.contype = NAT_CON_TYPE_LU,
	},
	{
		/* deny by nat rule */
		.data = bss_lu,
		.length = sizeof(bss_lu),
		.result = -3,
		.bsc_imsi_deny = "000[0-9]*",
		.nat_imsi_deny = "[0-9]*",
		.contype = NAT_CON_TYPE_LU,
	},
	{
		/* deny by bsc rule */
		.data = bss_lu,
		.length = sizeof(bss_lu),
		.result = -2,
		.bsc_imsi_deny = "[0-9]*",
		.contype = NAT_CON_TYPE_LU,
	},

};

static void test_cr_filter()
{
	int i, res, contype;
	struct msgb *msg = msgb_alloc(4096, "test_cr_filter");
	struct bsc_nat_parsed *parsed;
	struct bsc_nat_acc_lst *nat_lst, *bsc_lst;
	struct bsc_nat_acc_lst_entry *nat_entry, *bsc_entry;

	struct bsc_nat *nat = bsc_nat_alloc();
	struct bsc_connection *bsc = bsc_connection_alloc(nat);
	bsc->cfg = bsc_config_alloc(nat, "foo");
	bsc_config_add_lac(bsc->cfg, 1234);
	bsc->cfg->acc_lst_name = "bsc";
	nat->acc_lst_name = "nat";

	nat_lst = bsc_nat_acc_lst_get(nat, "nat");
	bsc_lst = bsc_nat_acc_lst_get(nat, "bsc");

	bsc_entry = bsc_nat_acc_lst_entry_create(bsc_lst);
	nat_entry = bsc_nat_acc_lst_entry_create(nat_lst);

	for (i = 0; i < ARRAY_SIZE(cr_filter); ++i) {
		char *imsi;
		msgb_reset(msg);
		copy_to_msg(msg, cr_filter[i].data, cr_filter[i].length);

		nat_lst = bsc_nat_acc_lst_get(nat, "nat");
		bsc_lst = bsc_nat_acc_lst_get(nat, "bsc");

		if (bsc_parse_reg(nat_entry, &nat_entry->imsi_deny_re, &nat_entry->imsi_deny,
			      cr_filter[i].nat_imsi_deny ? 1 : 0,
			      &cr_filter[i].nat_imsi_deny) != 0)
			abort();
		if (bsc_parse_reg(bsc_entry, &bsc_entry->imsi_allow_re, &bsc_entry->imsi_allow,
			      cr_filter[i].bsc_imsi_allow ? 1 : 0,
			      &cr_filter[i].bsc_imsi_allow) != 0)
			abort();
		if (bsc_parse_reg(bsc_entry, &bsc_entry->imsi_deny_re, &bsc_entry->imsi_deny,
			      cr_filter[i].bsc_imsi_deny ? 1 : 0,
			      &cr_filter[i].bsc_imsi_deny) != 0)
			abort();

		parsed = bsc_nat_parse(msg);
		if (!parsed) {
			fprintf(stderr, "FAIL: Failed to parse the message\n");
			abort();
		}

		res = bsc_nat_filter_sccp_cr(bsc, msg, parsed, &contype, &imsi);
		if (res != cr_filter[i].result) {
			fprintf(stderr, "FAIL: Wrong result %d for test %d.\n", res, i);
			abort();
		}

		if (contype != cr_filter[i].contype) {
			fprintf(stderr, "FAIL: Wrong contype %d for test %d.\n", res, contype);
			abort();
		}

		talloc_steal(parsed, imsi);
		talloc_free(parsed);
	}

	msgb_free(msg);
}

static void test_dt_filter()
{
	int i;
	struct msgb *msg = msgb_alloc(4096, "test_dt_filter");
	struct bsc_nat_parsed *parsed;

	struct bsc_nat *nat = bsc_nat_alloc();
	struct bsc_connection *bsc = bsc_connection_alloc(nat);
	struct sccp_connections *con = talloc_zero(0, struct sccp_connections);

	bsc->cfg = bsc_config_alloc(nat, "foo");
	bsc_config_add_lac(bsc->cfg, 23);
	con->bsc = bsc;

	msgb_reset(msg);
	copy_to_msg(msg, id_resp, ARRAY_SIZE(id_resp));

	parsed = bsc_nat_parse(msg);
	if (!parsed) {
		fprintf(stderr, "FAIL: Could not parse ID resp\n");
		abort();
	}

	if (parsed->bssap != BSSAP_MSG_DTAP) {
		fprintf(stderr, "FAIL: It should be dtap\n");
		abort();
	}

	/* gsm_type is actually the size of the dtap */
	if (parsed->gsm_type < msgb_l3len(msg) - 3) {
		fprintf(stderr, "FAIL: Not enough space for the content\n");
		abort();
	}

	if (bsc_nat_filter_dt(bsc, msg, con, parsed) != 1) {
		fprintf(stderr, "FAIL: Should have passed..\n");
		abort();
	}

	/* just some basic length checking... */
	for (i = ARRAY_SIZE(id_resp); i >= 0; --i) {
		msgb_reset(msg);
		copy_to_msg(msg, id_resp, ARRAY_SIZE(id_resp));

		parsed = bsc_nat_parse(msg);
		if (!parsed)
			continue;

		con->imsi_checked = 0;
		bsc_nat_filter_dt(bsc, msg, con, parsed);
	}
}

static void test_setup_rewrite()
{
	struct msgb *msg = msgb_alloc(4096, "test_dt_filter");
	struct msgb *out;
	struct bsc_nat_parsed *parsed;
	const char *imsi = "27408000001234";

	struct bsc_nat *nat = bsc_nat_alloc();

	/* a fake list */
	struct osmo_config_list entries;
	struct osmo_config_entry entry;

	INIT_LLIST_HEAD(&entries.entry);
	entry.mcc = "274";
	entry.mnc = "08";
	entry.option = "^0([1-9])";
	entry.text = "0049";
	llist_add_tail(&entry.list, &entries.entry);
	bsc_nat_num_rewr_entry_adapt(nat, &nat->num_rewr, &entries);

	/* verify that nothing changed */
	msgb_reset(msg);
	copy_to_msg(msg, cc_setup_international, ARRAY_SIZE(cc_setup_international));
	parsed = bsc_nat_parse(msg);
	if (!parsed) {
		fprintf(stderr, "FAIL: Could not parse ID resp\n");
		abort();
	}

	out = bsc_nat_rewrite_msg(nat, msg, parsed, imsi);
	if (msg != out) {
		fprintf(stderr, "FAIL: The message should not have been changed\n");
		abort();
	}

	if (out->len != ARRAY_SIZE(cc_setup_international)) {
		fprintf(stderr, "FAIL: Length of message changed\n");
		abort();
	}

	if (memcmp(out->data, cc_setup_international, out->len) != 0) {
		fprintf(stderr, "FAIL: Content modified..\n");
		abort();
	}
	talloc_free(parsed);

	/* verify that something in the message changes */
	msgb_reset(msg);
	copy_to_msg(msg, cc_setup_national, ARRAY_SIZE(cc_setup_national));
	parsed = bsc_nat_parse(msg);
	if (!parsed) {
		fprintf(stderr, "FAIL: Could not parse ID resp\n");
		abort();
	}

	out = bsc_nat_rewrite_msg(nat, msg, parsed, imsi);
	if (!out) {
		fprintf(stderr, "FAIL: A new message should be created.\n");
		abort();
	}

	if (msg == out) {
		fprintf(stderr, "FAIL: The message should have changed\n");
		abort();
	}

	if (out->len != ARRAY_SIZE(cc_setup_national_patched)) {
		fprintf(stderr, "FAIL: Length is wrong.\n");
		abort();
	}

	if (memcmp(cc_setup_national_patched, out->data, out->len) != 0) {
		fprintf(stderr, "FAIL: Data is wrong.\n");
		fprintf(stderr, "Data was: %s\n", osmo_hexdump(out->data, out->len));
		abort();
	}

	msgb_free(out);

	/* Make sure that a wildcard is matching */
	entry.mnc = "*";
	bsc_nat_num_rewr_entry_adapt(nat, &nat->num_rewr, &entries);
	msg = msgb_alloc(4096, "test_dt_filter");
	copy_to_msg(msg, cc_setup_national, ARRAY_SIZE(cc_setup_national));
	parsed = bsc_nat_parse(msg);
	if (!parsed) {
		fprintf(stderr, "FAIL: Could not parse ID resp\n");
		abort();
	}

	out = bsc_nat_rewrite_msg(nat, msg, parsed, imsi);
	if (!out) {
		fprintf(stderr, "FAIL: A new message should be created.\n");
		abort();
	}

	if (msg == out) {
		fprintf(stderr, "FAIL: The message should have changed\n");
		abort();
	}

	if (out->len != ARRAY_SIZE(cc_setup_national_patched)) {
		fprintf(stderr, "FAIL: Length is wrong.\n");
		abort();
	}

	if (memcmp(cc_setup_national_patched, out->data, out->len) != 0) {
		fprintf(stderr, "FAIL: Data is wrong.\n");
		fprintf(stderr, "Data was: %s\n", osmo_hexdump(out->data, out->len));
		abort();
	}

	msgb_free(out);

	/* Make sure that a wildcard is matching */
	entry.mnc = "09";
	bsc_nat_num_rewr_entry_adapt(nat, &nat->num_rewr, &entries);
	msg = msgb_alloc(4096, "test_dt_filter");
	copy_to_msg(msg, cc_setup_national, ARRAY_SIZE(cc_setup_national));
	parsed = bsc_nat_parse(msg);
	if (!parsed) {
		fprintf(stderr, "FAIL: Could not parse ID resp\n");
		abort();
	}

	out = bsc_nat_rewrite_msg(nat, msg, parsed, imsi);
	if (out != msg) {
		fprintf(stderr, "FAIL: The message should be unchanged.\n");
		abort();
	}

	if (out->len != ARRAY_SIZE(cc_setup_national)) {
		fprintf(stderr, "FAIL: Foo\n");
		abort();
	}

	if (memcmp(out->data, cc_setup_national, ARRAY_SIZE(cc_setup_national)) != 0) {
		fprintf(stderr, "FAIL: The message should really be unchanged.\n");
		abort();
	}

	msgb_free(out);
}

static void test_smsc_rewrite()
{
	struct msgb *msg = msgb_alloc(4096, "SMSC rewrite"), *out;
	struct bsc_nat_parsed *parsed;
	const char *imsi = "515039900406700";

	struct bsc_nat *nat = bsc_nat_alloc();

	/* a fake list */
	struct osmo_config_list smsc_entries, dest_entries;
	struct osmo_config_entry smsc_entry, dest_entry;

	INIT_LLIST_HEAD(&smsc_entries.entry);
	INIT_LLIST_HEAD(&dest_entries.entry);
	smsc_entry.mcc = "^515039";
	smsc_entry.option = "639180000105()";
	smsc_entry.text   = "6666666666667";
	llist_add_tail(&smsc_entry.list, &smsc_entries.entry);
	dest_entry.mcc = "515";
	dest_entry.mnc = "03";
	dest_entry.option = "^0049";
	dest_entry.text   = "";
	llist_add_tail(&dest_entry.list, &dest_entries.entry);

	bsc_nat_num_rewr_entry_adapt(nat, &nat->smsc_rewr, &smsc_entries);
	bsc_nat_num_rewr_entry_adapt(nat, &nat->tpdest_match, &dest_entries);

	copy_to_msg(msg, smsc_rewrite, ARRAY_SIZE(smsc_rewrite));
	parsed = bsc_nat_parse(msg);
	if (!parsed) {
		fprintf(stderr, "FAIL: Could not parse SMS\n");
		abort();
	}

	out = bsc_nat_rewrite_msg(nat, msg, parsed, imsi);
	if (out == msg) {
		fprintf(stderr, "FAIL: This should have changed.\n");
		abort();
	}

	if (out->len != ARRAY_SIZE(smsc_rewrite_patched)) {
		fprintf(stderr, "FAIL: The size should match.\n");
		abort();
	}

	if (memcmp(out->data, smsc_rewrite_patched, out->len) != 0) {
		fprintf(stderr, "FAIL: the data should be changed.\n");
		abort();
	}
}

int main(int argc, char **argv)
{
	sccp_set_log_area(DSCCP);
	osmo_init_logging(&log_info);

	test_filter();
	test_contrack();
	test_paging();
	test_mgcp_ass_tracking();
	test_mgcp_find();
	test_mgcp_rewrite();
	test_mgcp_parse();
	test_cr_filter();
	test_dt_filter();
	test_setup_rewrite();
	test_smsc_rewrite();
	test_mgcp_allocations();
	return 0;
}
