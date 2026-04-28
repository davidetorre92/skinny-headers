/*  skn_bar.h — header-only terminal progress bar
 *
 *  SINGLE BAR
 *    Renders in-place using \r; no ANSI codes needed.
 *    Format: [################..........] 62%  62/100
 *    Call sbar_finish() to emit a trailing \n.
 *
 *  BAR SET
 *    Manages N bars simultaneously using ANSI cursor codes.
 *    Supports the same output modes and timer as skn_log.h.
 *    All output to the shared stream must go through sbars_log() or
 *    sbars_end_timer() while bars are active — direct fprintf or
 *    slog_print to the same stream will garble the display.
 *    POSIX only (uses ANSI escape sequences and clock_gettime).
 *
 *  MODES (set with sbars_set_mode)
 *    SKN_LOG_PLAIN (1) : message only (default)
 *    SKN_LOG_TIMED (2) : [YYYY-MM-DD HH:mm:ss] prefix
 *    SKN_LOG_LEVEL (3) : [YYYY-MM-DD HH:mm:ss] INFO/WARNING/ERROR -
 *
 *  TIMER
 *    sbars_start_timer / sbars_end_timer bracket timed sections.
 *    In the sbars_end_timer message, %t is replaced with elapsed time
 *    auto-formatted in the most readable unit (µs, ms, s, min, h, days).
 *    %% outputs a literal %.
 *
 *  USAGE
 *    #define SKN_BAR_IMPLEMENTATION
 *    #include "skn_bar.h"
 *
 *  SINGLE BAR EXAMPLE
 *    SknBar *bar = sbar_init(100, 40, stdout);
 *    for (int i = 0; i <= 100; i++) {
 *        sbar_update(bar, i);
 *        do_work();
 *    }
 *    sbar_finish(bar);
 *    sbar_free(bar);
 *
 *  BAR SET EXAMPLE
 *    SknBarSet *set = sbars_init(3, stdout);
 *    sbars_set_mode(set, SKN_LOG_LEVEL);
 *    sbars_config(set, 0, 200, 40);
 *    sbars_start(set);
 *    sbars_start_timer(set);
 *    sbars_update(set, 0, 42);
 *    sbars_log(set, SKN_LOG_INFO, "worker 0 reached milestone\n");
 *    sbars_end_timer(set, SKN_LOG_INFO, "batch done in %t\n");
 *    sbars_finish(set);
 *    sbars_free(set);
 */

#ifndef SKN_BAR_H
#define SKN_BAR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * Log mode and level types (shared with skn_log.h when both are included;
 * defined here only if skn_log.h has not already defined them).
 * ---------------------------------------------------------------------- */

#ifndef SKN_LOG_H
typedef enum {
    SKN_LOG_PLAIN = 1,
    SKN_LOG_TIMED = 2,
    SKN_LOG_LEVEL = 3
} SknLogMode;

typedef enum {
    SKN_LOG_INFO = 0,
    SKN_LOG_WARN = 1,
    SKN_LOG_ERR  = 2
} SknLogLevel;
#endif /* SKN_LOG_H */

/* -------------------------------------------------------------------------
 * Public types
 * ---------------------------------------------------------------------- */

typedef struct {
    FILE *stream;
    int   total;    /* 100% mark; always >= 1 */
    int   current;
    int   width;    /* character cells inside the brackets; always >= 1 */
    char  fill;     /* completed portion character, default '#' */
    char  empty;    /* remaining portion character, default '.' */
} SknBar;

typedef struct {
    FILE           *stream;
    int             n;          /* number of bars */
    SknBar         *bars;       /* flat array of n SknBar structs */
    SknLogMode      mode;       /* output mode for sbars_log / sbars_end_timer */
    struct timespec t_start;    /* set by sbars_start_timer */
} SknBarSet;

/* -------------------------------------------------------------------------
 * Public API — single bar
 * ---------------------------------------------------------------------- */

/* Allocate and initialise a progress bar writing to stream.
 * total and width are clamped to >= 1. Returns NULL on allocation failure. */
SknBar *sbar_init(int total, int width, FILE *stream);

/* Free the bar. Does not close the underlying stream. */
void sbar_free(SknBar *bar);

/* Set fill and empty characters (defaults '#' and '.'). */
void sbar_set_chars(SknBar *bar, char fill, char empty);

/* Redraw the bar at absolute position value (clamped to [0, total]). */
void sbar_update(SknBar *bar, int value);

/* Advance current by delta, then redraw (result clamped to [0, total]). */
void sbar_step(SknBar *bar, int delta);

/* Force a final redraw at 100%, emit '\n', flush. */
void sbar_finish(SknBar *bar);

/* -------------------------------------------------------------------------
 * Public API — bar set (N bars, ANSI multi-line)
 * ---------------------------------------------------------------------- */

/* Allocate a set of n bars all writing to stream.
 * Default per bar: total=100, width=40, fill='#', empty='.'.
 * Default mode: SKN_LOG_PLAIN. */
SknBarSet *sbars_init(int n, FILE *stream);

/* Free the bar set. Does not close the underlying stream. */
void sbars_free(SknBarSet *set);

/* Set the output mode used by sbars_log and sbars_end_timer. */
void sbars_set_mode(SknBarSet *set, SknLogMode mode);

/* Override total and width for bar i. Must be called before sbars_start(). */
void sbars_config(SknBarSet *set, int i, int total, int width);

/* Override fill and empty characters for bar i. */
void sbars_set_chars(SknBarSet *set, int i, char fill, char empty);

/* Draw all n bars at 0% and position the cursor on the anchor line
 * (one line below the last bar). Must be called once before any
 * sbars_update, sbars_step, sbars_log, or sbars_end_timer. */
void sbars_start(SknBarSet *set);

/* Redraw bar i at absolute value (clamped to [0, total]). */
void sbars_update(SknBarSet *set, int i, int value);

/* Advance bar i by delta, then redraw (result clamped to [0, total]). */
void sbars_step(SknBarSet *set, int i, int delta);

/* Record the start of a timed section. */
void sbars_start_timer(SknBarSet *set);

/* Clear the bar region, print a printf-style message with optional timestamp
 * and level prefix (depending on mode), then redraw all bars below it.
 * The message must end with '\n'.
 * level is ignored in SKN_LOG_PLAIN and SKN_LOG_TIMED modes. */
void sbars_log(SknBarSet *set, SknLogLevel level, const char *fmt, ...);

/* Clear the bar region, print msg with %t replaced by elapsed time since
 * sbars_start_timer, then redraw all bars. %% outputs a literal %.
 * Respects the current mode for timestamp and level prefix.
 * The message must end with '\n'. */
void sbars_end_timer(SknBarSet *set, SknLogLevel level, const char *msg);

/* Force all bars to 100%, emit a final '\n' below the bar region, flush. */
void sbars_finish(SknBarSet *set);

/* -------------------------------------------------------------------------
 * Implementation
 * ---------------------------------------------------------------------- */

#ifdef SKN_BAR_IMPLEMENTATION

/* --- single-bar helpers ------------------------------------------------- */

static void bar__render(const SknBar *bar, int clamped, char *buf)
{
    int filled = (int)((double)clamped / bar->total * bar->width);
    int pct    = (int)((double)clamped / bar->total * 100.0);
    int empty  = bar->width - filled;
    int pos    = 0;

    buf[pos++] = '\r';
    buf[pos++] = '[';
    for (int i = 0; i < filled; i++) buf[pos++] = bar->fill;
    for (int i = 0; i < empty;  i++) buf[pos++] = bar->empty;
    buf[pos++] = ']';
    pos += sprintf(buf + pos, " %3d%%  %d/%d", pct, clamped, bar->total);
    buf[pos] = '\0';
}

SknBar *sbar_init(int total, int width, FILE *stream)
{
    SknBar *bar = (SknBar *)malloc(sizeof(SknBar));
    if (!bar) return NULL;
    bar->stream  = stream;
    bar->total   = total < 1 ? 1 : total;
    bar->width   = width < 1 ? 1 : width;
    bar->current = 0;
    bar->fill    = '#';
    bar->empty   = '.';
    return bar;
}

void sbar_free(SknBar *bar) { free(bar); }

void sbar_set_chars(SknBar *bar, char fill, char empty)
{
    bar->fill  = fill;
    bar->empty = empty;
}

void sbar_update(SknBar *bar, int value)
{
    if (value < 0)          value = 0;
    if (value > bar->total) value = bar->total;
    bar->current = value;
    char buf[bar->width + 32];
    bar__render(bar, value, buf);
    fputs(buf, bar->stream);
    fflush(bar->stream);
}

void sbar_step(SknBar *bar, int delta) { sbar_update(bar, bar->current + delta); }

void sbar_finish(SknBar *bar)
{
    char buf[bar->width + 32];
    bar__render(bar, bar->total, buf);
    fputs(buf, bar->stream);
    fputc('\n', bar->stream);
    fflush(bar->stream);
    bar->current = bar->total;
}

/* --- bar set: log/timer helpers ----------------------------------------- */

static void bar__print_timestamp(FILE *stream)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    fprintf(stream, "[%s] ", buf);
}

static void bar__print_level(FILE *stream, SknLogLevel level)
{
    static const char *labels[] = { "INFO", "WARNING", "ERROR" };
    fprintf(stream, "%s - ", labels[level]);
}

static long long bar__elapsed_ns(struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long)(now.tv_sec  - start->tv_sec)  * 1000000000LL
                    + (now.tv_nsec - start->tv_nsec);
}

static void bar__format_elapsed(long long ns, char *buf, size_t bufsz)
{
    if      (ns < 1000000LL)        snprintf(buf, bufsz, "%.2f \xc2\xb5s", ns / 1e3);
    else if (ns < 1000000000LL)     snprintf(buf, bufsz, "%.2f ms",  ns / 1e6);
    else if (ns < 60000000000LL)    snprintf(buf, bufsz, "%.2f s",   ns / 1e9);
    else if (ns < 3600000000000LL)  snprintf(buf, bufsz, "%.2f min", ns / 6e10);
    else if (ns < 86400000000000LL) snprintf(buf, bufsz, "%.2f h",   ns / 3.6e12);
    else                            snprintf(buf, bufsz, "%.2f days",ns / 8.64e13);
}

/* --- bar set: cursor management ----------------------------------------- */

static void barset__redraw_one(SknBarSet *set, int i)
{
    char buf[set->bars[i].width + 32];
    bar__render(&set->bars[i], set->bars[i].current, buf);
    fputs(buf, set->stream);   /* buf includes leading \r */
}

/* Cursor must be at bar 0's line, column 0. Prints all bars + \n each. */
static void barset__redraw_all(SknBarSet *set)
{
    for (int i = 0; i < set->n; i++) {
        char buf[set->bars[i].width + 32];
        bar__render(&set->bars[i], set->bars[i].current, buf);
        fputs(buf + 1, set->stream);   /* skip \r: already at column 0 */
        fputc('\n', set->stream);
    }
}

/* Erase the bar region and leave the cursor at bar 0's line, column 0. */
static void barset__clear(SknBarSet *set)
{
    fprintf(set->stream, "\033[%dA", set->n);
    for (int i = 0; i < set->n; i++) {
        fprintf(set->stream, "\033[2K");
        if (i < set->n - 1) fprintf(set->stream, "\033[1B");
    }
    if (set->n > 1) fprintf(set->stream, "\033[%dA", set->n - 1);
    fprintf(set->stream, "\r");
}

static void barset__print_prefix(SknBarSet *set, SknLogLevel level)
{
    if (set->mode == SKN_LOG_TIMED) {
        bar__print_timestamp(set->stream);
    } else if (set->mode == SKN_LOG_LEVEL) {
        bar__print_timestamp(set->stream);
        bar__print_level(set->stream, level);
    }
}

/* --- bar set: public functions ------------------------------------------ */

SknBarSet *sbars_init(int n, FILE *stream)
{
    if (n < 1) n = 1;
    SknBarSet *set = (SknBarSet *)malloc(sizeof(SknBarSet));
    if (!set) return NULL;
    set->bars = (SknBar *)malloc((size_t)n * sizeof(SknBar));
    if (!set->bars) { free(set); return NULL; }
    set->stream         = stream;
    set->n              = n;
    set->mode           = SKN_LOG_PLAIN;
    set->t_start.tv_sec  = 0;
    set->t_start.tv_nsec = 0;
    for (int i = 0; i < n; i++) {
        set->bars[i].stream  = stream;
        set->bars[i].total   = 100;
        set->bars[i].width   = 40;
        set->bars[i].current = 0;
        set->bars[i].fill    = '#';
        set->bars[i].empty   = '.';
    }
    return set;
}

void sbars_free(SknBarSet *set) { free(set->bars); free(set); }

void sbars_set_mode(SknBarSet *set, SknLogMode mode) { set->mode = mode; }

void sbars_config(SknBarSet *set, int i, int total, int width)
{
    set->bars[i].total = total < 1 ? 1 : total;
    set->bars[i].width = width < 1 ? 1 : width;
}

void sbars_set_chars(SknBarSet *set, int i, char fill, char empty)
{
    set->bars[i].fill  = fill;
    set->bars[i].empty = empty;
}

void sbars_start(SknBarSet *set)
{
    for (int i = 0; i < set->n; i++) {
        set->bars[i].current = 0;
        char buf[set->bars[i].width + 32];
        bar__render(&set->bars[i], 0, buf);
        fputs(buf + 1, set->stream);   /* skip \r on first draw */
        fputc('\n', set->stream);
    }
    fflush(set->stream);
}

void sbars_update(SknBarSet *set, int i, int value)
{
    if (value < 0)                  value = 0;
    if (value > set->bars[i].total) value = set->bars[i].total;
    set->bars[i].current = value;
    fprintf(set->stream, "\033[%dA", set->n - i);
    barset__redraw_one(set, i);
    fprintf(set->stream, "\033[%dB", set->n - i);
    fflush(set->stream);
}

void sbars_step(SknBarSet *set, int i, int delta)
{
    sbars_update(set, i, set->bars[i].current + delta);
}

void sbars_start_timer(SknBarSet *set)
{
    clock_gettime(CLOCK_MONOTONIC, &set->t_start);
}

void sbars_log(SknBarSet *set, SknLogLevel level, const char *fmt, ...)
{
    barset__clear(set);
    barset__print_prefix(set, level);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(set->stream, fmt, ap);
    va_end(ap);
    barset__redraw_all(set);
    fflush(set->stream);
}

void sbars_end_timer(SknBarSet *set, SknLogLevel level, const char *msg)
{
    long long ns = bar__elapsed_ns(&set->t_start);
    char elapsed[32];
    bar__format_elapsed(ns, elapsed, sizeof(elapsed));

    barset__clear(set);
    barset__print_prefix(set, level);

    const char *p = msg;
    while (*p) {
        if (*p == '%') {
            if      (*(p + 1) == 't') { fputs(elapsed, set->stream); p += 2; }
            else if (*(p + 1) == '%') { fputc('%',     set->stream); p += 2; }
            else                      { fputc('%',     set->stream); p++;    }
        } else {
            fputc(*p, set->stream);
            p++;
        }
    }

    barset__redraw_all(set);
    fflush(set->stream);
}

void sbars_finish(SknBarSet *set)
{
    for (int i = 0; i < set->n; i++)
        sbars_update(set, i, set->bars[i].total);
    fputc('\n', set->stream);
    fflush(set->stream);
}

#endif /* SKN_BAR_IMPLEMENTATION */
#endif /* SKN_BAR_H */
