/* Copyright (c) 2018, Linaro Limited
 * Copyright (c) 2019, Nokia
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include <unistd.h>
#include <getopt.h>

#include <odp_api.h>

#define MAX_FILENAME 128

typedef struct timer_ctx_t {
    odp_timer_t timer;
    odp_event_t event;
    uint64_t    nsec;

} timer_ctx_t;

typedef struct {
    uint64_t nsec_before_sum;
    uint64_t nsec_before_min;
    uint64_t nsec_before_max;

    uint64_t nsec_after_sum;
    uint64_t nsec_after_min;
    uint64_t nsec_after_max;

    uint64_t num_before;
    uint64_t num_exact;
    uint64_t num_after;

    uint64_t num_tooearly;

} test_stat_t;

typedef struct test_log_t {
    uint64_t tmo_ns;
    int64_t  diff_ns;

} test_log_t;

typedef struct test_global_t {
    struct {
        unsigned long long int period_ns;
        unsigned long long int res_ns;
        unsigned long long int offset_ns;
        unsigned long long int num;
        unsigned long long int burst;
        unsigned long long int burst_gap;
        int mode;
        int init;
        int output;
        int early_retry;
    } opt;

    test_stat_t stat;

    odp_queue_t      queue;
    odp_timer_pool_t timer_pool;
    odp_pool_t       timeout_pool;
    timer_ctx_t     *timer_ctx;
    uint64_t         period_ns;
    uint64_t         tot_timers;
    uint64_t         alloc_timers;
    uint64_t         start_tick;
    uint64_t         start_ns;
    uint64_t         period_tick;
    odp_shm_t        log_shm;
    test_log_t    *log;
    FILE            *file;
    char         filename[MAX_FILENAME + 1];

} test_global_t;

static void print_usage(void)
{
    printf("\n"
           "Timer accuracy test application.\n"
           "\n"
           "OPTIONS:\n"
           "  -p, --period <nsec>     Timeout period in nsec. Default: 200 msec\n"
           "  -r, --resolution <nsec> Timeout resolution in nsec. Default: period / 10\n"
           "  -f, --first <nsec>      First timer offset in nsec. Default: 300 msec\n"
           "  -n, --num <number>      Number of timeout periods. Default: 50\n"
           "  -b, --burst <number>    Number of timers per a timeout period. Default: 1\n"
           "  -g, --burst_gap <nsec>  Gap (in nsec) between timers within a burst. Default: 0\n"
           "  -m, --mode <number>     Test mode select (default: 0):\n"
           "                            0: Set all timers at init phase.\n"
           "                            1: Set first burst of timers at init. Restart timers during test with absolute time.\n"
           "                            2: Set first burst of timers at init. Restart timers during test with relative time.\n"
           "  -o, --output <file>     Output file for measurement logs\n"
           "  -e, --early_retry <num> When timer restart fails due to ODP_TIMER_TOOEARLY, retry this many times\n"
           "                          with expiration time incremented by the period. Default: 0\n"
           "  -i, --init              Set global init parameters. Default: init params not set.\n"
           "  -h, --help              Display help and exit.\n\n");
}

static int parse_options(int argc, char *argv[], test_global_t *test_global)
{
    int opt, long_index;
    const struct option longopts[] = {
        {"period",       required_argument, NULL, 'p'},
        {"resolution",   required_argument, NULL, 'r'},
        {"first",        required_argument, NULL, 'f'},
        {"num",          required_argument, NULL, 'n'},
        {"burst",        required_argument, NULL, 'b'},
        {"burst_gap",    required_argument, NULL, 'g'},
        {"mode",         required_argument, NULL, 'm'},
        {"output",       required_argument, NULL, 'o'},
        {"early_retry",  required_argument, NULL, 'e'},
        {"init",         no_argument,       NULL, 'i'},
        {"help",         no_argument,       NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    const char *shortopts =  "+p:r:f:n:b:g:m:o:e:ih";
    int ret = 0;

    test_global->opt.period_ns = 200 * ODP_TIME_MSEC_IN_NS;
    test_global->opt.res_ns    = 0;
    test_global->opt.offset_ns = 300 * ODP_TIME_MSEC_IN_NS;
    test_global->opt.num       = 50;
    test_global->opt.burst     = 1;
    test_global->opt.burst_gap = 0;
    test_global->opt.mode      = 0;
    test_global->opt.init      = 0;
    test_global->opt.output    = 0;
    test_global->opt.early_retry = 0;

    while (1) {
        opt = getopt_long(argc, argv, shortopts, longopts, &long_index);

        if (opt == -1)
            break;    /* No more options */

        switch (opt) {
        case 'p':
            test_global->opt.period_ns = strtoull(optarg, NULL, 0);
            break;
        case 'r':
            test_global->opt.res_ns = strtoull(optarg, NULL, 0);
            break;
        case 'f':
            test_global->opt.offset_ns = strtoull(optarg, NULL, 0);
            break;
        case 'n':
            test_global->opt.num = strtoull(optarg, NULL, 0);
            break;
        case 'b':
            test_global->opt.burst = strtoull(optarg, NULL, 0);
            break;
        case 'g':
            test_global->opt.burst_gap = strtoull(optarg, NULL, 0);
            break;
        case 'm':
            test_global->opt.mode = atoi(optarg);
            break;
        case 'o':
            test_global->opt.output = 1;
            /* filename is NULL terminated in anycase */
            strncpy(test_global->filename, optarg, MAX_FILENAME);
            break;
        case 'e':
            test_global->opt.early_retry = atoi(optarg);
            break;
        case 'i':
            test_global->opt.init = 1;
            break;
        case 'h':
            print_usage();
            ret = -1;
            break;
        default:
            print_usage();
            ret = -1;
            break;
        }
    }

    if (test_global->opt.res_ns == 0)
        test_global->opt.res_ns = test_global->opt.period_ns / 10;

    test_global->tot_timers = test_global->opt.num * test_global->opt.burst;

    if (test_global->opt.mode)
        test_global->alloc_timers = test_global->opt.burst;
    else
        test_global->alloc_timers = test_global->tot_timers;

    return ret;
}

static int start_timers(test_global_t *test_global)
{
    odp_pool_t pool;
    odp_pool_param_t pool_param;
    odp_timer_pool_t timer_pool;
    odp_timer_pool_param_t timer_param;
    odp_timer_capability_t timer_capa;
    odp_timer_t timer;
    odp_queue_t queue;
    odp_queue_param_t queue_param;
    uint64_t tick, start_tick;
    uint64_t period_ns, res_ns, start_ns, nsec, res_capa, offset_ns;
    odp_event_t event;
    odp_timeout_t timeout;
    odp_timer_set_t ret;
    odp_time_t time;
    uint64_t i, j, idx, num_tmo, burst, burst_gap;
    uint64_t tot_timers, alloc_timers;
    int mode;

    mode = test_global->opt.mode;
    alloc_timers = test_global->alloc_timers;
    tot_timers = test_global->tot_timers;
    num_tmo = test_global->opt.num;
    burst = test_global->opt.burst;
    burst_gap = test_global->opt.burst_gap;
    period_ns = test_global->opt.period_ns;
    test_global->period_ns = period_ns;

    /* Always init globals for destroy calls */
    test_global->queue = ODP_QUEUE_INVALID;
    test_global->timer_pool = ODP_TIMER_POOL_INVALID;
    test_global->timeout_pool = ODP_POOL_INVALID;

    for (i = 0; i < alloc_timers; i++) {
        test_global->timer_ctx[i].timer = ODP_TIMER_INVALID;
        test_global->timer_ctx[i].event = ODP_EVENT_INVALID;
    }

    odp_queue_param_init(&queue_param);
    queue_param.type        = ODP_QUEUE_TYPE_SCHED;
    queue_param.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
    queue_param.sched.sync  = ODP_SCHED_SYNC_ATOMIC;
    queue_param.sched.group = ODP_SCHED_GROUP_ALL;

    queue = odp_queue_create("timeout_queue", &queue_param);
    if (queue == ODP_QUEUE_INVALID) {
        printf("Queue create failed.\n");
        return -1;
    }

    test_global->queue = queue;

    odp_pool_param_init(&pool_param);
    pool_param.type    = ODP_POOL_TIMEOUT;
    pool_param.tmo.num = alloc_timers;

    pool = odp_pool_create("timeout pool", &pool_param);

    if (pool == ODP_POOL_INVALID) {
        printf("Timeout pool create failed.\n");
        return -1;
    }

    test_global->timeout_pool = pool;

    if (odp_timer_capability(ODP_CLOCK_CPU, &timer_capa)) {
        printf("Timer capa failed\n");
        return -1;
    }

    if (timer_capa.max_timers &&
        test_global->alloc_timers > timer_capa.max_timers) {
        printf("Error: Too many timers: %" PRIu64 ".\n"
               "       Max timers: %u\n", test_global->alloc_timers,
               timer_capa.max_timers);
        return -1;
    }

    res_capa = timer_capa.highest_res_ns;

    offset_ns = test_global->opt.offset_ns;
    res_ns = test_global->opt.res_ns;

    if (res_ns < res_capa) {
        printf("Resolution %" PRIu64 " nsec too high. "
               "Highest resolution %" PRIu64 " nsec. "
               "Default resolution is period / 10.\n\n",
               res_ns, res_capa);
        return -1;
    }

    memset(&timer_param, 0, sizeof(odp_timer_pool_param_t));

    timer_param.res_ns     = res_ns;
    if (mode)
        timer_param.min_tmo = period_ns / 10;
    else
        timer_param.min_tmo = offset_ns / 2;

    timer_param.max_tmo    = offset_ns + ((num_tmo + 1) * period_ns);
    timer_param.num_timers = alloc_timers;
    timer_param.clk_src    = ODP_CLOCK_CPU;

    printf("\nTest parameters:\n");
    printf("  resolution capa: %" PRIu64 " nsec\n", res_capa);
    printf("  max timers capa: %" PRIu32 "\n", timer_capa.max_timers);
    printf("  mode:            %i\n", mode);
    printf("  restart retries: %i\n", test_global->opt.early_retry);
    if (test_global->opt.output)
        printf("  log file:        %s\n", test_global->filename);
    printf("  start offset:    %" PRIu64 " nsec\n", offset_ns);
    printf("  period:          %" PRIu64 " nsec\n", period_ns);
    printf("  resolution:      %" PRIu64 " nsec\n", timer_param.res_ns);
    printf("  min timeout:     %" PRIu64 " nsec\n", timer_param.min_tmo);
    printf("  max timeout:     %" PRIu64 " nsec\n", timer_param.max_tmo);
    printf("  num timeout:     %" PRIu64 "\n", num_tmo);
    printf("  burst size:      %" PRIu64 "\n", burst);
    printf("  burst gap:       %" PRIu64 "\n", burst_gap);
    printf("  total timers:    %" PRIu64 "\n", tot_timers);
    printf("  alloc timers:    %" PRIu64 "\n", alloc_timers);
    printf("  test run time:   %.2f sec\n\n",
           (offset_ns + (num_tmo * period_ns)) / 1000000000.0);

    timer_pool = odp_timer_pool_create("timer_accuracy", &timer_param);

    if (timer_pool == ODP_TIMER_POOL_INVALID) {
        printf("Timer pool create failed\n");
        return -1;
    }

    odp_timer_pool_start();

    /* Spend some time so that current tick would not be zero */
    odp_time_wait_ns(100 * ODP_TIME_MSEC_IN_NS);

    test_global->timer_pool = timer_pool;

    for (i = 0; i < alloc_timers; i++) {
        timer_ctx_t *ctx = &test_global->timer_ctx[i];

        timer = odp_timer_alloc(timer_pool, queue, ctx);

        if (timer == ODP_TIMER_INVALID) {
            printf("Timer alloc failed.\n");
            return -1;
        }

        ctx->timer = timer;

        timeout = odp_timeout_alloc(pool);
        if (timeout == ODP_TIMEOUT_INVALID) {
            printf("Timeout alloc failed\n");
            return -1;
        }

        ctx->event = odp_timeout_to_event(timeout);
    }

    /* Run scheduler few times to ensure that (software) timer is active */
    for (i = 0; i < 1000; i++) {
        event = odp_schedule(NULL, ODP_SCHED_NO_WAIT);

        if (event != ODP_EVENT_INVALID) {
            printf("Spurious event received\n");
            odp_event_free(event);
            return -1;
        }
    }

    idx = 0;
    start_tick = odp_timer_current_tick(timer_pool);
    time       = odp_time_local();
    start_ns   = odp_time_to_ns(time);

    test_global->start_tick = start_tick;
    test_global->start_ns = start_ns;
    test_global->period_tick = odp_timer_ns_to_tick(timer_pool, period_ns);

    /* When mode is 1, set only one burst of timers initially */
    if (mode)
        num_tmo = 1;

    for (i = 0; i < num_tmo; i++) {
        for (j = 0; j < burst; j++) {
            nsec = offset_ns + (i * period_ns) + (j * burst_gap);
            tick = start_tick + odp_timer_ns_to_tick(timer_pool,
                                 nsec);

            timer_ctx_t *ctx = &test_global->timer_ctx[idx];

            timer = ctx->timer;
            event = ctx->event;
            ctx->nsec = start_ns + nsec;

            ret = odp_timer_set_abs(timer, tick, &event);

            if (ret != ODP_TIMER_SUCCESS) {
                printf("Timer[%" PRIu64 "] set failed: %i\n",
                       idx, ret);
                return -1;
            }

            idx++;
        }
    }

    return 0;
}

static int destroy_timers(test_global_t *test_global)
{
    uint64_t i, alloc_timers;
    odp_timer_t timer;
    odp_event_t ev;
    int ret = 0;

    alloc_timers = test_global->alloc_timers;

    for (i = 0; i < alloc_timers; i++) {
        timer = test_global->timer_ctx[i].timer;

        if (timer == ODP_TIMER_INVALID)
            break;

        ev = odp_timer_free(timer);

        if (ev != ODP_EVENT_INVALID)
            odp_event_free(ev);
    }

    if (test_global->timer_pool != ODP_TIMER_POOL_INVALID)
        odp_timer_pool_destroy(test_global->timer_pool);

    if (test_global->timeout_pool != ODP_POOL_INVALID) {
        if (odp_pool_destroy(test_global->timeout_pool)) {
            printf("Pool destroy failed.\n");
            ret = -1;
        }
    }

    if (test_global->queue != ODP_QUEUE_INVALID) {
        if (odp_queue_destroy(test_global->queue)) {
            printf("Queue destroy failed.\n");
            ret = -1;
        }
    }

    return ret;
}

static void print_stat(test_global_t *test_global)
{
    uint64_t i;
    uint64_t tot_timers = test_global->tot_timers;
    uint64_t res_ns = test_global->opt.res_ns;
    test_stat_t *stat = &test_global->stat;
    test_log_t *log = test_global->log;
    double ave_after = 0.0;
    double ave_before = 0.0;

    if (stat->num_after)
        ave_after = (double)stat->nsec_after_sum / stat->num_after;
    else
        stat->nsec_after_min = 0;

    if (stat->num_before)
        ave_before = (double)stat->nsec_before_sum / stat->num_before;
    else
        stat->nsec_before_min = 0;

    if (log) {
        FILE *file = test_global->file;

        fprintf(file, "   Timer      tmo(ns)   diff(ns)\n");

        for (i = 0; i < tot_timers; i++) {
            fprintf(file, "%8" PRIu64 " %12" PRIu64 " %10"
                PRIi64 "\n", i, log[i].tmo_ns, log[i].diff_ns);
        }

        fprintf(file, "\n");
    }

    printf("\n Test results:\n");
    printf("  num after:  %12" PRIu64 "  /  %.2f%%\n",
           stat->num_after, 100.0 * stat->num_after / tot_timers);
    printf("  num before: %12" PRIu64 "  /  %.2f%%\n",
           stat->num_before, 100.0 * stat->num_before / tot_timers);
    printf("  num exact:  %12" PRIu64 "  /  %.2f%%\n",
           stat->num_exact, 100.0 * stat->num_exact / tot_timers);
    printf("  num retry:  %12" PRIu64 "  /  %.2f%%\n",
           stat->num_tooearly, 100.0 * stat->num_tooearly / tot_timers);
    printf("  error after (nsec):\n");
    printf("         min: %12" PRIu64 "  /  %.3fx resolution\n",
           stat->nsec_after_min, (double)stat->nsec_after_min / res_ns);
    printf("         max: %12" PRIu64 "  /  %.3fx resolution\n",
           stat->nsec_after_max, (double)stat->nsec_after_max / res_ns);
    printf("         ave: %12.0f  /  %.3fx resolution\n",
           ave_after, ave_after / res_ns);
    printf("  error before (nsec):\n");
    printf("         min: %12" PRIu64 "  /  %.3fx resolution\n",
           stat->nsec_before_min, (double)stat->nsec_before_min / res_ns);
    printf("         max: %12" PRIu64 "  /  %.3fx resolution\n",
           stat->nsec_before_max, (double)stat->nsec_before_max / res_ns);
    printf("         ave: %12.0f  /  %.3fx resolution\n",
           ave_before, ave_before / res_ns);
    printf("\n");
}

static void run_test(test_global_t *test_global)
{
    uint64_t burst, num, num_tmo, next_tmo;
    uint64_t i, tot_timers;
    odp_event_t ev;
    odp_time_t time;
    uint64_t time_ns, diff_ns, period_ns;
    odp_timeout_t tmo;
    uint64_t tmo_ns;
    timer_ctx_t *ctx;
    test_stat_t *stat = &test_global->stat;
    test_log_t *log = test_global->log;
    int mode = test_global->opt.mode;

    num      = 0;
    next_tmo = 1;
    num_tmo  = test_global->opt.num;
    burst    = test_global->opt.burst;
    tot_timers = test_global->tot_timers;
    period_ns  = test_global->period_ns;

    for (i = 0; i < tot_timers; i++) {
        ev = odp_schedule(NULL, ODP_SCHED_WAIT);

        time = odp_time_local();
        time_ns = odp_time_to_ns(time);
        tmo = odp_timeout_from_event(ev);
        ctx = odp_timeout_user_ptr(tmo);
        tmo_ns = ctx->nsec;

        if (time_ns > tmo_ns) {
            diff_ns = time_ns - tmo_ns;
            stat->num_after++;
            stat->nsec_after_sum += diff_ns;
            if (diff_ns < stat->nsec_after_min)
                stat->nsec_after_min = diff_ns;
            if (diff_ns > stat->nsec_after_max)
                stat->nsec_after_max = diff_ns;
            if (log) {
                log[i].tmo_ns  = tmo_ns;
                log[i].diff_ns = diff_ns;
            }

        } else if (time_ns < tmo_ns) {
            diff_ns = tmo_ns - time_ns;
            stat->num_before++;
            stat->nsec_before_sum += diff_ns;
            if (diff_ns < stat->nsec_before_min)
                stat->nsec_before_min = diff_ns;
            if (diff_ns > stat->nsec_before_max)
                stat->nsec_before_max = diff_ns;
            if (log) {
                log[i].tmo_ns  = tmo_ns;
                log[i].diff_ns = -diff_ns;
            }
        } else {
            stat->num_exact++;
        }

        if (mode && next_tmo < num_tmo) {
            /* Reset timer for next period */
            odp_timer_t tim;
            uint64_t nsec, tick;
            odp_timer_set_t ret;
            unsigned int j;
            odp_timer_pool_t tp = test_global->timer_pool;
            unsigned int retries = test_global->opt.early_retry;
            uint64_t start_ns = test_global->start_ns;

            tim = ctx->timer;

            /* Depending on the option, retry when expiration
             * time is too early */
            for (j = 0; j < retries + 1; j++) {
                if (mode == 1) {
                    /* Absolute time */
                    ctx->nsec += period_ns;
                    nsec = ctx->nsec - start_ns;
                    tick = test_global->start_tick +
                           odp_timer_ns_to_tick(tp, nsec);

                    ret = odp_timer_set_abs(tim, tick, &ev);
                } else {
                    /* Relative time */
                    tick = test_global->period_tick;
                    time = odp_time_local();
                    time_ns = odp_time_to_ns(time);
                    ctx->nsec = time_ns + period_ns;

                    ret = odp_timer_set_rel(tim, tick, &ev);
                }

                if (ret == ODP_TIMER_TOOEARLY)
                    stat->num_tooearly++;
                else
                    break;
            }

            if (ret != ODP_TIMER_SUCCESS) {
                printf("Timer set failed: %i. Timeout nsec "
                       "%" PRIu64 "\n", ret, ctx->nsec);
                return;
            }
        } else {
            odp_event_free(ev);
        }

        num++;

        if (num == burst) {
            next_tmo++;
            num = 0;
        }

    }

    /* Free current scheduler context. There should be no more events. */
    while ((ev = odp_schedule(NULL, ODP_SCHED_NO_WAIT))
           != ODP_EVENT_INVALID) {
        printf("Dropping extra event\n");
        odp_event_free(ev);
    }
}

int main(int argc, char *argv[])
{
    odp_instance_t instance;
    odp_init_t init;
    test_global_t test_global;
    odp_init_t *init_ptr = NULL;
    int ret = 0;

    memset(&test_global, 0, sizeof(test_global_t));
    test_global.stat.nsec_before_min = UINT64_MAX;
    test_global.stat.nsec_after_min  = UINT64_MAX;

    if (parse_options(argc, argv, &test_global))
        return -1;

    if (test_global.opt.output) {
        test_global.file = fopen(test_global.filename, "w");
        if (test_global.file == NULL) {
            printf("Failed to open file: %s\n",
                   test_global.filename);
            return -1;
        }
    }

    /* List features not to be used (may optimize performance) */
    odp_init_param_init(&init);
    init.not_used.feat.cls      = 1;
    init.not_used.feat.compress = 1;
    init.not_used.feat.crypto   = 1;
    init.not_used.feat.ipsec    = 1;
    init.not_used.feat.tm       = 1;

    if (test_global.opt.init)
        init_ptr = &init;

    /* Init ODP before calling anything else */
    if (odp_init_global(&instance, init_ptr, NULL)) {
        printf("Global init failed.\n");
        return -1;
    }

    /* Init this thread */
    if (odp_init_local(instance, ODP_THREAD_WORKER)) {
        printf("Local init failed.\n");
        return -1;
    }

    odp_sys_info_print();

    /* Configure scheduler */
    odp_schedule_config(NULL);

    test_global.timer_ctx = calloc(test_global.alloc_timers,
                       sizeof(timer_ctx_t));

    if (test_global.timer_ctx == NULL) {
        printf("Timer context table calloc failed.\n");
        goto quit;
    }

    if (test_global.opt.output) {
        odp_shm_t shm;
        void *addr;
        uint64_t size = test_global.tot_timers * sizeof(test_log_t);

        shm = odp_shm_reserve("timer_accuracy_log", size,
                      sizeof(test_log_t), 0);

        if (shm == ODP_SHM_INVALID) {
            printf("Test log alloc failed.\n");
            goto quit;
        }

        addr = odp_shm_addr(shm);
        memset(addr, 0, size);
        test_global.log = addr;
        test_global.log_shm = shm;
    }

    if (start_timers(&test_global))
        goto quit;

    run_test(&test_global);

    print_stat(&test_global);

quit:
    if (test_global.file)
        fclose(test_global.file);

    if (destroy_timers(&test_global))
        ret = -1;

    if (test_global.timer_ctx)
        free(test_global.timer_ctx);

    if (test_global.log)
        odp_shm_free(test_global.log_shm);

    if (odp_term_local()) {
        printf("Term local failed.\n");
        ret = -1;
    }

    if (odp_term_global(instance)) {
        printf("Term global failed.\n");
        ret = -1;
    }

    return ret;
}
