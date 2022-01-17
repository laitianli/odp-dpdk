/* add by ltl */
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>

#include <odp/visibility_begin.h>
#include <odp/api/log.h>
#include <rte_log.h>
#include <odp_debug_internal.h>
static int g_log_level_org = 0;

static const char *
loglevel_to_string(uint32_t level)
{
	switch (level) {
	case 0: return "disabled";
	case RTE_LOG_EMERG: return "emerg";
	case RTE_LOG_ALERT: return "alert";
	case RTE_LOG_CRIT: return "critical";
	case RTE_LOG_ERR: return "error";
	case RTE_LOG_WARNING: return "warning";
	case RTE_LOG_NOTICE: return "notice";
	case RTE_LOG_INFO: return "info";
	case RTE_LOG_DEBUG: return "debug";
	default: return "unknown";
	}
}

int odp_dpdk_dump_log_types(char* buf, size_t size, char** pos, size_t ps, size_t unit_sz)
{
    if (!buf || size <= 0)
        return -1;
    if (!pos || ps <= 0)
        return -1;
    struct rte_logs* log = rte_get_log_obj();
   	size_t i = 0, j = 0;
    size_t ret = 0, tmp_ret = 0;
    char tmp_buf[200] = {0};
    pos[j++] = buf;
	ret += snprintf(buf + ret, size - ret, "->global log level: %s\r\n",
            loglevel_to_string(log->level));
    ret += snprintf(buf + ret, size - ret, "%-3s %-35s %s\r\n", "-id-","-logtype-", "-level-");
	for (i = 0; i < log->dynamic_types_len; i++) {
		if (log->dynamic_types[i].name == NULL)
			continue;
        tmp_ret = sprintf(tmp_buf, "%-5zu %-35s %s\r\n",
			i, log->dynamic_types[i].name,
			loglevel_to_string(log->dynamic_types[i].loglevel));
         if (size - ret <= tmp_ret + 1) {
            printf("\033[31m [Error] [%s:%d] buffer size less, break!!! \n\033[0m", __func__, __LINE__);
            break;
        }
        if (ret >= unit_sz * j) {
            if (j >= ps) {
                return ret;
            }
            buf[ret++] = '\0';
            pos[j++] = buf + ret;
        }
		ret += snprintf(buf + ret, size - ret, "%s", tmp_buf);
	}
    return ret;
}

int odp_dpdk_set_log(const char* logtype, int logtype_len, int level)
{
    size_t i = 0;
    struct rte_logs* log = rte_get_log_obj();
    if (level < 0 || (unsigned int)level > RTE_LOG_DEBUG)
        return 0;
    log->level = level;
    if (!strncmp(logtype, "all", logtype_len)) {
        if (level == 0)
            return 1;
        for (i = 0; i < log->dynamic_types_len; i++) {
    		if (log->dynamic_types[i].name == NULL)
    			continue;
            log->dynamic_types[i].loglevel = level;
        }
        return 1;
    }
    else {
        for (i = 0; i < log->dynamic_types_len; i++) {
    		if (!log->dynamic_types[i].name ||
                strncmp(log->dynamic_types[i].name, logtype, logtype_len))
    			continue;
            log->dynamic_types[i].loglevel = level;
            return 1;
        }
    }
    return 0;
}

int odp_dpdk_reset_log(void)
{
    size_t i;
    struct rte_logs* log = rte_get_log_obj();
    struct rte_logs* log_org = rte_get_log_org_obj();
    log->type = log_org->type;
    log->level = log_org->level;
    log->dynamic_types_len = log_org->dynamic_types_len;
    for (i = 0; i < log_org->dynamic_types_len; i++) {
        if (log_org->dynamic_types[i].name == NULL)
    	    continue;
        log->dynamic_types[i].loglevel = log_org->dynamic_types[i].loglevel;
        log->dynamic_types[i].name = log_org->dynamic_types[i].name;
    }
    return 1;
}

int odp_get_log(void)
{
    return odp_global_ro.log_level;
}

int odp_reset_log(void)
{
    odp_global_ro.log_level = g_log_level_org;
    return 1;
}

int odp_set_log(int level)
{
    static int first_tag = 1;
    if (first_tag == 1) {
        first_tag = 0;
        g_log_level_org = odp_global_ro.log_level;
    }
    odp_global_ro.log_level = level;
    return 1;
}



