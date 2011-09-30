#ifndef _BSS_H_
#define _BSS_H_

struct gsm_network;
struct msgb;

/* start and stop network */
extern int bsc_bootstrap_network(int (*mncc_recv)(struct gsm_network *, struct msgb *), const char *cfg_file);
extern int bsc_shutdown_net(struct gsm_network *net);

/* register all supported BTS */
extern int bts_init(void);
extern int bts_model_bs11_init(void);
extern int bts_model_rbs2k_init(void);
extern int bts_model_nanobts_init(void);
extern int bts_model_hslfemto_init(void);
extern int bts_model_nokia_site_init(void);
#endif
