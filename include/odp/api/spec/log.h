#ifndef ODP_API_SPEC_LOG_H_
#define ODP_API_SPEC_LOG_H_

/* add by ltl */
#ifdef __cplusplus
extern "C" {
#endif
int odp_dpdk_dump_log_types(char* buf, size_t size, char** pos, size_t ps, size_t unit_sz);

int odp_dpdk_set_log(const char* logtype, int logtype_len, int level);

int odp_dpdk_reset_log(void);

#ifdef __cplusplus
}
#endif

#endif

