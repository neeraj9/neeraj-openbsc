#ifndef _GPRS_BSSGP_H
#define _GPRS_BSSGP_H

#include <stdint.h>

/* Section 5.4.1 */
#define BVCI_SIGNALLING	0x0000
#define BVCI_PTM	0x0001

/* Section 11.3.26 / Table 11.27 */
enum bssgp_pdu_type {
	/* PDUs between RL and BSSGP SAPs */
	BSSGP_PDUT_DL_UNITDATA		= 0x00,
	BSSGP_PDUT_UL_UNITDATA		= 0x01,
	BSSGP_PDUT_RA_CAPABILITY	= 0x02,
	BSSGP_PDUT_PTM_UNITDATA		= 0x03,
	/* PDUs between GMM SAPs */
	BSSGP_PDUT_PAGING_PS		= 0x06,
	BSSGP_PDUT_PAGING_CS		= 0x07,
	BSSGP_PDUT_RA_CAPA_UDPATE	= 0x08,
	BSSGP_PDUT_RA_CAPA_UPDATE_ACK	= 0x09,
	BSSGP_PDUT_RADIO_STATUS		= 0x0a,
	BSSGP_PDUT_SUSPEND		= 0x0b,
	BSSGP_PDUT_SUSPEND_ACK		= 0x0c,
	BSSGP_PDUT_SUSPEND_NACK		= 0x0d,
	BSSGP_PDUT_RESUME		= 0x0e,
	BSSGP_PDUT_RESUME_ACK		= 0x0f,
	BSSGP_PDUT_RESUME_NACK		= 0x10,
	/* PDus between NM SAPs */
	BSSGP_PDUT_BVC_BLOCK		= 0x20,
	BSSGP_PDUT_BVC_BLOCK_ACK	= 0x21,
	BSSGP_PDUT_BVC_RESET		= 0x22,
	BSSGP_PDUT_BVC_RESET_ACK	= 0x23,
	BSSGP_PDUT_BVC_UNBLOCK		= 0x24,
	BSSGP_PDUT_BVC_UNBLOCK_ACK	= 0x25,
	BSSGP_PDUT_FLOW_CONTROL_BVC	= 0x26,
	BSSGP_PDUT_FLOW_CONTROL_BVC_ACK	= 0x27,
	BSSGP_PDUT_FLOW_CONTROL_MS	= 0x28,
	BSSGP_PDUT_FLOW_CONTROL_MS_ACK	= 0x29,
	BSSGP_PDUT_FLUSH_LL		= 0x2a,
	BSSGP_PDUT_FLUSH_LL_ACK		= 0x2b,
	BSSGP_PDUT_LLC_DISCARD		= 0x2c,
	BSSGP_PDUT_SGSN_INVOKE_TRACE	= 0x40,
	BSSGP_PDUT_STATUS		= 0x41,
	/* PDUs between PFM SAP's */
	BSSGP_PDUT_DOWNLOAD_BSS_PFC	= 0x50,
	BSSGP_PDUT_CREATE_BSS_PFC	= 0x51,
	BSSGP_PDUT_CREATE_BSS_PFC_ACK	= 0x52,
	BSSGP_PDUT_CREATE_BSS_PFC_NACK	= 0x53,
	BSSGP_PDUT_MODIFY_BSS_PFC	= 0x54,
	BSSGP_PDUT_MODIFY_BSS_PFC_ACK	= 0x55,
	BSSGP_PDUT_DELETE_BSS_PFC	= 0x56,
	BSSGP_PDUT_DELETE_BSS_PFC_ACK	= 0x57,
};

/* Section 10.2.1 and 10.2.2 */
struct bssgp_ud_hdr {
	uint8_t pdu_type;
	uint32_t tlli;
	uint8_t qos_profile[3];
	uint8_t data[0];	/* TLV's */
} __attribute__((packed));

struct bssgp_normal_hdr {
	uint8_t pdu_type;
	uint8_t data[0];	/* TLV's */
};

enum bssgp_iei_type {
	BSSGP_IE_ALIGNMENT		= 0x00,
	BSSGP_IE_BMAX_DEFAULT_MS	= 0x01,
	BSSGP_IE_BSS_AREA_ID		= 0x02,
	BSSGP_IE_BUCKET_LEAK_RATE	= 0x03,
	BSSGP_IE_BVCI			= 0x04,
	BSSGP_IE_BVC_BUCKET_SIZE	= 0x05,
	BSSGP_IE_BVC_MEASUREMENT	= 0x06,
	BSSGP_IE_CAUSE			= 0x07,
	BSSGP_IE_CELL_ID		= 0x08,
	BSSGP_IE_CHAN_NEEDED		= 0x09,
	BSSGP_IE_DRX_PARAMS		= 0x0a,
	BSSGP_IE_EMLPP_PRIO		= 0x0b,
	BSSGP_IE_FLUSH_ACTION		= 0x0c,
	BSSGP_IE_IMSI			= 0x0d,
	BSSGP_IE_LLC_PDU		= 0x0e,
	BSSGP_IE_LLC_FRAMES_DISCARDED	= 0x0f,
	BSSGP_IE_LOCATION_AREA		= 0x10,
	BSSGP_IE_MOBILE_ID		= 0x11,
	BSSGP_IE_MS_BUCKET_SIZE		= 0x12,
	BSSGP_IE_MS_RADIO_ACCESS_CAP	= 0x13,
	BSSGP_IE_OMC_ID			= 0x14,
	BSSGP_IE_PDU_IN_ERROR		= 0x15,
	BSSGP_IE_PDU_LIFETIME		= 0x16,
	BSSGP_IE_PRIORITY		= 0x17,
	BSSGP_IE_QOS_PROFILE		= 0x18,
	BSSGP_IE_RADIO_CAUSE		= 0x19,
	BSSGP_IE_RA_CAP_UPD_CAUSE	= 0x1a,
	BSSGP_IE_ROUTEING_AREA		= 0x1b,
	BSSGP_IE_R_DEFAULT_MS		= 0x1c,
	BSSGP_IE_SUSPEND_REF_NR		= 0x1d,
	BSSGP_IE_TAG			= 0x1e,
	BSSGP_IE_TLLI			= 0x1f,
	BSSGP_IE_TMSI			= 0x20,
	BSSGP_IE_TRACE_REFERENC		= 0x21,
	BSSGP_IE_TRACE_TYPE		= 0x22,
	BSSGP_IE_TRANSACTION_ID		= 0x23,
	BSSGP_IE_TRIGGER_ID		= 0x24,
	BSSGP_IE_NUM_OCT_AFF		= 0x25,
	BSSGP_IE_LSA_ID_LIST		= 0x26,
	BSSGP_IE_LSA_INFORMATION	= 0x27,
	BSSGP_IE_PACKET_FLOW_ID		= 0x28,
	BSSGP_IE_PACKET_FLOW_TIMER	= 0x29,
	BSSGP_IE_AGG_BSS_QOS_PROFILE	= 0x3a,
	BSSGP_IE_FEATURE_BITMAP		= 0x3b,
	BSSGP_IE_BUCKET_FULL_RATIO	= 0x3c,
	BSSGP_IE_SERVICE_UTRAN_CCO	= 0x3d,
};

/* Section 11.3.8 / Table 11.10: Cause coding */
enum gprs_bssgp_cause {
	BSSGP_CAUSE_PROC_OVERLOAD	= 0x00,
	BSSGP_CAUSE_EQUIP_FAIL		= 0x01,
	BSSGP_CAUSE_TRASIT_NET_FAIL	= 0x02,
	BSSGP_CAUSE_CAPA_GREATER_0KPBS	= 0x03,
	BSSGP_CAUSE_UNKNOWN_MS		= 0x04,
	BSSGP_CAUSE_UNKNOWN_BVCI	= 0x05,
	BSSGP_CAUSE_CELL_TRAF_CONG	= 0x06,
	BSSGP_CAUSE_SGSN_CONG		= 0x07,
	BSSGP_CAUSE_OML_INTERV		= 0x08,
	BSSGP_CAUSE_BVCI_BLOCKED	= 0x09,
	BSSGP_CAUSE_PFC_CREATE_FAIL	= 0x0a,
	BSSGP_CAUSE_SEM_INCORR_PDU	= 0x20,
	BSSGP_CAUSE_INV_MAND_INF	= 0x21,
	BSSGP_CAUSE_MISSING_MAND_IE	= 0x22,
	BSSGP_CAUSE_MISSING_COND_IE	= 0x23,
	BSSGP_CAUSE_UNEXP_COND_IE	= 0x24,
	BSSGP_CAUSE_COND_IE_ERR		= 0x25,
	BSSGP_CAUSE_PDU_INCOMP_STATE	= 0x26,
	BSSGP_CAUSE_PROTO_ERR_UNSPEC	= 0x27,
	BSSGP_CAUSE_PDU_INCOMP_FEAT	= 0x28,
};

/* Our implementation */

/* gprs_bssgp_util.c */
extern struct gprs_ns_inst *bssgp_nsi;
struct msgb *bssgp_msgb_alloc(void);
const char *bssgp_cause_str(enum gprs_bssgp_cause cause);
/* Transmit a simple response such as BLOCK/UNBLOCK/RESET ACK/NACK */
int bssgp_tx_simple_bvci(uint8_t pdu_type, uint16_t nsei,
			 uint16_t bvci, uint16_t ns_bvci);
/* Chapter 10.4.14: Status */
int bssgp_tx_status(uint8_t cause, uint16_t *bvci, struct msgb *orig_msg);

/* gprs_bssgp.c */

/* According to Section 8.2 */
struct bssgp_flow_control {
	uint32_t bucket_size_max;
	uint32_t bucket_leak_rate;

	uint32_t bucket_counter;
	struct timeval time_last_pdu;

	/* the built-in queue */
	uint32_t max_queue_depth;
	uint32_t queue_depth;
	struct llist_head queue;
	/* callback to be called at output of flow control */
	int (*out_cb)(struct bssgp_flow_control *fc, struct msgb *msg,
			uint32_t llc_pdu_len, void *priv);
};

#define BVC_S_BLOCKED	0x0001

/* The per-BTS context that we keep on the SGSN side of the BSSGP link */
struct bssgp_bvc_ctx {
	struct llist_head list;

	/* parsed RA ID and Cell ID of the remote BTS */
	struct gprs_ra_id ra_id;
	uint16_t cell_id;

	/* NSEI and BVCI of underlying Gb link.  Together they
	 * uniquely identify a link to a BTS (5.4.4) */
	uint16_t bvci;
	uint16_t nsei;

	uint32_t state;

	struct rate_ctr_group *ctrg;

	struct bssgp_flow_control fc;
	uint32_t bmax_default_ms;
	uint32_t r_default_ms;

	/* we might want to add this as a shortcut later, avoiding the NSVC
	 * lookup for every packet, similar to a routing cache */
	//struct gprs_nsvc *nsvc;
};
extern struct llist_head bssgp_bvc_ctxts;
/* Find a BTS Context based on parsed RA ID and Cell ID */
struct bssgp_bvc_ctx *btsctx_by_raid_cid(const struct gprs_ra_id *raid, uint16_t cid);
/* Find a BTS context based on BVCI+NSEI tuple */
struct bssgp_bvc_ctx *btsctx_by_bvci_nsei(uint16_t bvci, uint16_t nsei);

#include <osmocore/tlv.h>

/* BSSGP-UL-UNITDATA.ind */
int gprs_bssgp_rcvmsg(struct msgb *msg);

/* BSSGP-DL-UNITDATA.req */
struct sgsn_mm_ctx;
int gprs_bssgp_tx_dl_ud(struct msgb *msg, struct sgsn_mm_ctx *mmctx);

uint16_t bssgp_parse_cell_id(struct gprs_ra_id *raid, const uint8_t *buf);

/* Wrapper around TLV parser to parse BSSGP IEs */
static inline int bssgp_tlv_parse(struct tlv_parsed *tp, uint8_t *buf, int len)
{
	return tlv_parse(tp, &tvlv_att_def, buf, len, 0, 0);
}

enum bssgp_paging_mode {
	BSSGP_PAGING_PS,
	BSSGP_PAGING_CS,
};

enum bssgp_paging_scope {
	BSSGP_PAGING_BSS_AREA,		/* all cells in BSS */
	BSSGP_PAGING_LOCATION_AREA,	/* all cells in LA */
	BSSGP_PAGING_ROUTEING_AREA,	/* all cells in RA */
	BSSGP_PAGING_BVCI,		/* one cell */
};

struct bssgp_paging_info {
	enum bssgp_paging_mode mode;
	enum bssgp_paging_scope scope;
	struct gprs_ra_id raid;
	uint16_t bvci;
	const char *imsi;
	uint32_t *ptmsi;
	uint16_t drx_params;
	uint8_t qos[3];
};

/* Send a single GMM-PAGING.req to a given NSEI/NS-BVCI */
int gprs_bssgp_tx_paging(uint16_t nsei, uint16_t ns_bvci,
			 struct bssgp_paging_info *pinfo);

/* input function of the flow control implementation, called first
 * for the MM flow control, and then as the MM flow control output
 * callback in order to perform BVC flow control */
int bssgp_fc_in(struct bssgp_flow_control *fc, struct msgb *msg,
		uint32_t llc_pdu_len, void *priv);

/* Initialize the Flow Control parameters for a new MS according to
 * default values for the BVC specified by BVCI and NSEI */
int bssgp_fc_ms_init(struct bssgp_flow_control *fc_ms, uint16_t bvci,
		     uint16_t nsei);

/* gprs_bssgp_vty.c */
int gprs_bssgp_vty_init(void);

#endif /* _GPRS_BSSGP_H */
