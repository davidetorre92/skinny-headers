#define SKN_LOG_IMPLEMENTATION
#include "skn_log.h"

static void busy_work(int n)
{
    volatile long long x = 0;
    for (int i = 0; i < n; i++) x += i;
    (void)x;
}

int main(void)
{
    /* --- mode 1: plain --- */
    SknLog *plain = slog_init(SKN_LOG_PLAIN, stdout);
    slog_print(plain, SKN_LOG_INFO, "=== plain mode ===\n");
    slog_print(plain, SKN_LOG_INFO, "no timestamp here, just the message\n");
    slog_print(plain, SKN_LOG_INFO, "value: %d, ratio: %.3f\n", 42, 3.14);
    slog_free(plain);

    /* --- mode 2: timestamped --- */
    SknLog *log = slog_init(SKN_LOG_TIMED, stdout);
    slog_print(log, SKN_LOG_INFO, "=== timed mode ===\n");
    slog_print(log, SKN_LOG_INFO, "every line gets a timestamp\n");

    /* --- timer: fast work (microseconds) --- */
    slog_start_timer(log);
    busy_work(1000);
    slog_end_timer(log, SKN_LOG_INFO, "fast work done, took %t\n");

    /* --- timer: slower work (milliseconds) --- */
    slog_start_timer(log);
    busy_work(100000000);
    slog_end_timer(log, SKN_LOG_INFO, "slow work done, took %t\n");

    /* --- %% escape --- */
    slog_end_timer(log, SKN_LOG_INFO, "100%% complete, last elapsed still: %t\n");
    slog_free(log);

    /* --- mode 3: level-tagged --- */
    SknLog *lvl = slog_init(SKN_LOG_LEVEL, stdout);
    slog_print(lvl, SKN_LOG_INFO, "=== level mode ===\n");
    slog_print(lvl, SKN_LOG_INFO, "everything is fine\n");
    slog_print(lvl, SKN_LOG_WARN, "disk usage above 80%%\n");
    slog_print(lvl, SKN_LOG_ERR,  "connection refused on port %d\n", 8080);

    slog_start_timer(lvl);
    busy_work(100000000);
    slog_end_timer(lvl, SKN_LOG_INFO, "batch processed in %t\n");
    slog_free(lvl);

    /* --- log to stderr --- */
    SknLog *err = slog_init(SKN_LOG_LEVEL, stderr);
    slog_print(err, SKN_LOG_ERR, "this error goes to stderr\n");
    slog_free(err);

    return 0;
}
