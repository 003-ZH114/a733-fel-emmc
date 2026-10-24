#ifndef A733_EMMC_INIT_H
#define A733_EMMC_INIT_H

/*
 * Initialize SMHC2 and the eMMC card for bare-metal FEL use.
 *
 * Returns 0 on success, or a negative error code:
 *   -1  clock update failed
 *   -2  CMD1 (SEND_OP_COND) timeout
 *   -3  CMD6 bus-width switch failed
 *
 * If cid is non-NULL it receives the four CID response words.
 */
int emmc_init(unsigned int *cid);

#endif /* A733_EMMC_INIT_H */
