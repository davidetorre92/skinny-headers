/*  skn_dat.h — header-only .dat file parser
 *
 *  FORMAT
 *    key value
 *
 *  Value types:
 *    42          -> int
 *    3.14f       -> float
 *    3.14        -> double
 *    "hello"     -> string
 *    [1, 2, 3]   -> list (homogeneous; type inferred from first element)
 *
 *  Comments: lines whose first non-whitespace character is '#' are ignored.
 *
 *  USAGE
 *    #define SKN_DAT_IMPLEMENTATION
 *    #include "skn_dat.h"
 *
 *  EXAMPLE
 *    DatDocument *doc = dat_load("data.dat");
 *    DatRow      *row = dat_get(doc, "key1");
 *    if (row && row->value.type == DAT_INT)
 *        printf("%d\n", row->value.i);
 *    dat_free(doc);
 */

#ifndef SKN_DAT_H
#define SKN_DAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------
 * Public types
 * ---------------------------------------------------------------------- */

typedef enum {
    DAT_INT,
    DAT_FLOAT,
    DAT_DOUBLE,
    DAT_STRING,
    DAT_LIST
} DatType;

typedef struct DatListNode {
    union {
        int    i;
        float  f;
        double d;
        char  *s;
    };
    struct DatListNode *next;
} DatListNode;

typedef struct {
    DatType      elem_type;
    DatListNode *head;
    int          length;
} DatList;

typedef struct {
    DatType type;
    union {
        int     i;
        float   f;
        double  d;
        char   *s;
        DatList list;
    };
} DatValue;

typedef struct DatRow {
    char          *key;
    DatValue       value;
    struct DatRow *next;
} DatRow;

typedef struct {
    DatRow *head;
    int     count;
} DatDocument;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/* Load a .dat file from disk. Returns NULL on failure. */
DatDocument *dat_load(const char *filename);

/* Parse a .dat document from a null-terminated string. Returns NULL on failure. */
DatDocument *dat_parse_string(const char *text);

/* Look up a row by key. Returns NULL if not found. */
DatRow *dat_get(DatDocument *doc, const char *key);

/* Free all memory owned by the document. */
void dat_free(DatDocument *doc);

/* -------------------------------------------------------------------------
 * Implementation
 * ---------------------------------------------------------------------- */

#ifdef SKN_DAT_IMPLEMENTATION

static char *dat__strndup(const char *s, size_t n)
{
    if (!s) return NULL;
    char *copy = (char *)malloc(n + 1);
    if (copy) { memcpy(copy, s, n); copy[n] = '\0'; }
    return copy;
}

static const char *dat__skip_spaces(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/*
 * Parse a single scalar value (int / float / double / string) starting at p.
 * On success: fills *out and sets *end to the character after the value, returns 1.
 * On failure: returns 0.
 */
static int dat__parse_scalar(const char *p, DatValue *out, const char **end)
{
    p = dat__skip_spaces(p);

    /* --- string --- */
    if (*p == '"') {
        p++;
        const char *start = p;
        while (*p && *p != '"' && *p != '\n') p++;
        if (*p != '"') return 0; /* unterminated string */
        out->type = DAT_STRING;
        out->s    = dat__strndup(start, (size_t)(p - start));
        if (!out->s) return 0;
        *end = p + 1;
        return 1;
    }

    /* --- number --- */
    if (*p == '-' || isdigit((unsigned char)*p)) {
        const char *q = p;
        if (*q == '-') q++;
        while (isdigit((unsigned char)*q)) q++;

        int is_decimal = (*q == '.');
        if (is_decimal) {
            q++;
            while (isdigit((unsigned char)*q)) q++;
        }
        int is_float = is_decimal && (*q == 'f' || *q == 'F');

        char *num_end;
        if (is_float) {
            out->type = DAT_FLOAT;
            out->f    = strtof(p, &num_end);
            if (*num_end == 'f' || *num_end == 'F') num_end++;
        } else if (is_decimal) {
            out->type = DAT_DOUBLE;
            out->d    = strtod(p, &num_end);
        } else {
            out->type = DAT_INT;
            out->i    = (int)strtol(p, &num_end, 10);
        }
        *end = num_end;
        return 1;
    }

    return 0;
}

/*
 * Parse a value (scalar or list) starting at p.
 * On success: fills *out and sets *end to the character after the value, returns 1.
 * On failure: returns 0.
 */
static int dat__parse_value(const char *p, DatValue *out, const char **end)
{
    p = dat__skip_spaces(p);

    /* --- list --- */
    if (*p == '[') {
        p++;
        DatList      list = { DAT_INT, NULL, 0 };
        DatListNode *tail = NULL;
        int          first = 1;

        while (1) {
            p = dat__skip_spaces(p);
            if (*p == ']') { p++; break; }
            if (*p == ',') { p++; continue; }
            if (*p == '\0' || *p == '\n') return 0; /* malformed list */

            DatValue     elem;
            const char  *elem_end;
            if (!dat__parse_scalar(p, &elem, &elem_end)) return 0;

            if (first) {
                list.elem_type = elem.type;
                first = 0;
            } else if (elem.type != list.elem_type) {
                /* heterogeneous list: reject */
                if (elem.type == DAT_STRING) free(elem.s);
                return 0;
            }

            DatListNode *node = (DatListNode *)malloc(sizeof(DatListNode));
            if (!node) return 0;
            switch (elem.type) {
                case DAT_INT:    node->i = elem.i; break;
                case DAT_FLOAT:  node->f = elem.f; break;
                case DAT_DOUBLE: node->d = elem.d; break;
                case DAT_STRING: node->s = elem.s; break;
                default: break;
            }
            node->next = NULL;

            if (!tail) list.head = node;
            else        tail->next = node;
            tail = node;
            list.length++;

            p = elem_end;
        }

        out->type = DAT_LIST;
        out->list = list;
        *end = p;
        return 1;
    }

    return dat__parse_scalar(p, out, end);
}

static void dat__free_value(DatValue *v)
{
    if (v->type == DAT_STRING) {
        free(v->s);
    } else if (v->type == DAT_LIST) {
        DatListNode *node = v->list.head;
        while (node) {
            DatListNode *next = node->next;
            if (v->list.elem_type == DAT_STRING) free(node->s);
            free(node);
            node = next;
        }
    }
}

static DatDocument *dat__parse_lines(const char *text)
{
    DatDocument *doc = (DatDocument *)malloc(sizeof(DatDocument));
    if (!doc) return NULL;
    doc->head  = NULL;
    doc->count = 0;

    DatRow     *tail = NULL;
    const char *p    = text;

    while (*p) {
        /* skip inline whitespace at start of line */
        while (*p == ' ' || *p == '\t') p++;

        /* skip comments and blank lines */
        if (*p == '#' || *p == '\n' || *p == '\r') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }
        if (*p == '\0') break;

        /* parse key */
        const char *key_start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        char *key = dat__strndup(key_start, (size_t)(p - key_start));
        if (!key) { dat_free(doc); return NULL; }

        while (*p == ' ' || *p == '\t') p++;

        /* parse value */
        DatValue    value;
        const char *val_end;
        if (!dat__parse_value(p, &value, &val_end)) {
            free(key);
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }
        p = val_end;

        /* skip rest of line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        /* append row */
        DatRow *row = (DatRow *)malloc(sizeof(DatRow));
        if (!row) { free(key); dat__free_value(&value); dat_free(doc); return NULL; }
        row->key   = key;
        row->value = value;
        row->next  = NULL;

        if (!tail) doc->head = row;
        else        tail->next = row;
        tail = row;
        doc->count++;
    }

    return doc;
}

DatDocument *dat_parse_string(const char *text)
{
    return dat__parse_lines(text);
}

DatDocument *dat_load(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t nread  = fread(buf, 1, (size_t)size, f);
    buf[nread]    = '\0';
    fclose(f);

    DatDocument *doc = dat__parse_lines(buf);
    free(buf);
    return doc;
}

DatRow *dat_get(DatDocument *doc, const char *key)
{
    for (DatRow *row = doc->head; row; row = row->next)
        if (strcmp(row->key, key) == 0) return row;
    return NULL;
}

void dat_free(DatDocument *doc)
{
    if (!doc) return;
    DatRow *row = doc->head;
    while (row) {
        DatRow *next = row->next;
        free(row->key);
        dat__free_value(&row->value);
        free(row);
        row = next;
    }
    free(doc);
}

#endif /* SKN_DAT_IMPLEMENTATION */
#endif /* SKN_DAT_H */
