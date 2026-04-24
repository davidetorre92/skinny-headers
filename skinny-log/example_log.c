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
    slog_print(plain, "=== plain mode ===\n");
    slog_print(plain, "no timestamp here, just the message\n");
    slog_print(plain, "value: %d, ratio: %.3f\n", 42, 3.14);
    slog_free(plain);

    /* --- mode 2: timestamped --- */
    SknLog *log = slog_init(SKN_LOG_TIMED, stdout);
    slog_print(log, "=== timed mode ===\n");
    slog_print(log, "every line gets a timestamp\n");

    /* --- timer: fast work (microseconds) --- */
    slog_start_timer(log);
    busy_work(1000);
    slog_end_timer(log, "fast work done, took %t\n");

    /* --- timer: slower work (milliseconds) --- */
    slog_start_timer(log);
    busy_work(100000000);
    slog_end_timer(log, "slow work done, took %t\n");

    /* --- %% escape --- */
    slog_end_timer(log, "100%% complete, last elapsed still: %t\n");

    /* --- log to stderr --- */
    SknLog *err = slog_init(SKN_LOG_TIMED, stderr);
    slog_print(err, "this goes to stderr\n");
    slog_free(err);

    slog_free(log);
    return 0;
}
