#define SKN_BAR_IMPLEMENTATION
#include "skn_bar.h"

#include <stdio.h>
#include <unistd.h>   /* usleep */

static void busy_work(int ms)
{
    usleep((unsigned)ms * 1000);
}

int main(void)
{
    /* 1. Single bar — 100 steps, default style */
    printf("--- single bar, 100 steps ---\n");
    SknBar *bar = sbar_init(100, 40, stdout);
    for (int i = 0; i <= 100; i++) {
        sbar_update(bar, i);
        busy_work(15);
    }
    sbar_finish(bar);
    sbar_free(bar);

    /* 2. sbar_step — non-unit delta */
    printf("--- sbar_step, delta=5 ---\n");
    bar = sbar_init(100, 40, stdout);
    for (int i = 0; i < 20; i++) {
        sbar_step(bar, 5);
        busy_work(50);
    }
    sbar_finish(bar);
    sbar_free(bar);

    /* 3. Custom characters */
    printf("--- custom chars ('=' / ' ') ---\n");
    bar = sbar_init(60, 40, stdout);
    sbar_set_chars(bar, "=", " ");
    for (int i = 0; i <= 60; i++) {
        sbar_update(bar, i);
        busy_work(15);
    }
    sbar_finish(bar);
    sbar_free(bar);

    /* 4. Narrow bar — verifies fraction arithmetic at small width */
    printf("--- narrow bar, width=10, total=7 ---\n");
    bar = sbar_init(7, 10, stdout);
    for (int i = 0; i <= 7; i++) {
        sbar_update(bar, i);
        busy_work(150);
    }
    sbar_finish(bar);
    sbar_free(bar);

    /* 5. Bar set — SKN_LOG_PLAIN, 3 workers, mixed totals and styles */
    printf("--- bar set (PLAIN): 3 workers ---\n");
    SknBarSet *set = sbars_init(3, stdout);
    sbars_config(set, 0, 100, 30);
    sbars_config(set, 1, 200, 30);
    sbars_config(set, 2,  50, 30);
    sbars_set_chars(set, 1, "=", "-");
    sbars_set_chars(set, 2, "*", ".");
    sbars_start(set);

    for (int tick = 1; tick <= 200; tick++) {
        sbars_update(set, 0, tick <= 100 ? tick : 100);
        sbars_update(set, 1, tick);
        sbars_update(set, 2, tick <=  50 ? tick :  50);
        if (tick ==  50) sbars_log(set, SKN_LOG_INFO, "worker 2 finished\n");
        if (tick == 100) sbars_log(set, SKN_LOG_INFO, "worker 0 finished\n");
        busy_work(10);
    }
    sbars_finish(set);
    sbars_free(set);

    /* 6. Bar set — SKN_LOG_TIMED mode */
    printf("--- bar set (TIMED): 2 workers ---\n");
    set = sbars_init(2, stdout);
    sbars_set_mode(set, SKN_LOG_TIMED);
    sbars_config(set, 0, 50, 30);
    sbars_config(set, 1, 50, 30);
    sbars_start(set);

    for (int tick = 1; tick <= 50; tick++) {
        sbars_update(set, 0, tick);
        sbars_update(set, 1, 50 - tick);
        if (tick == 25)
            sbars_log(set, SKN_LOG_INFO, "halfway mark reached\n");
        busy_work(30);
    }
    sbars_finish(set);
    sbars_free(set);

    /* 7. Bar set — SKN_LOG_LEVEL mode with timer */
    printf("--- bar set (LEVEL + timer): 2 workers ---\n");
    set = sbars_init(2, stdout);
    sbars_set_mode(set, SKN_LOG_LEVEL);
    sbars_config(set, 0, 80, 30);
    sbars_config(set, 1, 80, 30);
    sbars_start(set);
    sbars_start_timer(set);

    for (int tick = 1; tick <= 80; tick++) {
        sbars_update(set, 0, tick);
        sbars_update(set, 1, 80 - tick);
        if (tick == 40)
            sbars_log(set, SKN_LOG_WARN, "worker 1 falling behind\n");
        busy_work(20);
    }
    sbars_end_timer(set, SKN_LOG_INFO, "batch complete in %t\n");
    sbars_finish(set);
    sbars_free(set);

    return 0;
}
