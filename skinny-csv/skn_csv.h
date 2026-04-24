/*  skn_csv.h — header-only CSV parser
 *
 *  FORMAT
 *    First line : comma-separated column names (header row)
 *    Subsequent : comma-separated values
 *    Fields may be quoted with double-quotes; "" inside quotes = literal "
 *    Blank lines between data rows are ignored.
 *
 *  Value types (inferred per column; type of each cell is determined
 *  individually, and the column takes the most general type found):
 *    42, -7          -> int
 *    3.14f, 0.5f     -> float
 *    3.14, 1.0       -> double
 *    anything else   -> string
 *
 *  USAGE
 *    #define SKN_CSV_IMPLEMENTATION
 *    #include "skn_csv.h"
 *
 *  EXAMPLE — inferred types
 *    CsvDocument *doc = csv_load("data.csv");
 *    DatArray col = csv_get(doc, "price");
 *    for (int i = 0; i < col.size; i++)
 *        printf("%g\n", col.d[i]);
 *    csv_free(doc);
 *
 *  EXAMPLE — explicit schema
 *    CsvSchema schema[] = { {"price", CSV_DOUBLE}, {"name", CSV_STRING} };
 *    CsvDocument *doc = csv_load_schema("data.csv", schema, 2);
 *    DatArray col = csv_get(doc, "name");
 *    for (int i = 0; i < col.size; i++)
 *        puts(col.s[i]);
 *    csv_free(doc);
 */

#ifndef SKN_CSV_H
#define SKN_CSV_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------
 * Public types
 * ---------------------------------------------------------------------- */

/* Ordered: INT < FLOAT < DOUBLE < STRING so that (a > b) means "more general". */
typedef enum {
    CSV_INT    = 0,
    CSV_FLOAT  = 1,
    CSV_DOUBLE = 2,
    CSV_STRING = 3
} CsvType;

/* One entry per column whose type you want to fix explicitly.
 * Columns absent from the schema are inferred. */
typedef struct {
    const char *name;
    CsvType     type;
} CsvSchema;

/* Internal per-column storage (one typed array per column). */
typedef union {
    int    *i;
    float  *f;
    double *d;
    char  **s;
} CsvColData;

typedef struct {
    char      **names;      /* column names  [col_count] */
    CsvType    *types;       /* column types  [col_count] */
    CsvColData *data;        /* column arrays [col_count] */
    int         col_count;
    int         row_count;
} CsvDocument;

/* View returned by csv_get — points into the document's own storage.
 * Do not free the inner pointer; it is owned by CsvDocument. */
typedef struct {
    CsvType type;
    int     size;
    union {
        int    *i;
        float  *f;
        double *d;
        char  **s;
    };
} DatArray;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/* Load and parse a CSV file; types inferred. sep is the field separator (e.g. ',' or ';').
 * Returns NULL on failure. */
CsvDocument *csv_load(const char *filename, char sep);

/* Load and parse a CSV file with an explicit type schema.
 * Columns not listed in schema are inferred. Returns NULL on failure. */
CsvDocument *csv_load_schema(const char *filename, const CsvSchema *schema, int schema_count, char sep);

/* Parse a CSV document from a null-terminated string; types inferred. */
CsvDocument *csv_parse_string(const char *text, char sep);

/* Parse a CSV document from a null-terminated string with an explicit schema. */
CsvDocument *csv_parse_string_schema(const char *text, const CsvSchema *schema, int schema_count, char sep);

/* Return a view of the named column as a DatArray.
 * The returned pointers are owned by the document — do not free them.
 * Calls exit(EXIT_FAILURE) if the field name is not found. */
DatArray csv_get(CsvDocument *doc, const char *name);

/* Fill out[0..doc->col_count-1] with the document's column names and types.
 * The name pointers point into document storage — do not free them.
 * The caller must ensure out has at least doc->col_count elements. */
void csv_get_schema(CsvDocument *doc, CsvSchema *out);

/* Print each column name and its inferred/assigned type to stdout. */
void csv_print_schema(CsvDocument *doc);

/* Free all memory owned by the document. Safe to call with NULL. */
void csv_free(CsvDocument *doc);

/* -------------------------------------------------------------------------
 * Implementation
 * ---------------------------------------------------------------------- */

#ifdef SKN_CSV_IMPLEMENTATION



static char *csv__strndup(const char *s, size_t n)
{
    if (!s) return NULL;
    char *copy = (char *)malloc(n + 1);
    if (copy) { memcpy(copy, s, n); copy[n] = '\0'; }
    return copy;
}

/* Append one character to a heap buffer, growing it with realloc as needed.
 * Returns 1 on success, 0 on allocation failure (buf is freed on failure). */
static int csv__buf_push(char **buf, size_t *len, size_t *cap, char c)
{
    if (*len + 1 >= *cap) {
        *cap *= 2;
        char *t = (char *)realloc(*buf, *cap);
        if (!t) { free(*buf); *buf = NULL; return 0; }
        *buf = t;
    }
    (*buf)[(*len)++] = c;
    return 1;
}

/*
 * Read one CSV field from *p and return it as a heap-allocated string.
 * Handles RFC-4180 quoting: a field wrapped in double-quotes may contain
 * commas and newlines; "" inside quotes encodes a literal ".
 * Advances *p past the field and its delimiter (comma, newline, or EOF).
 * Sets *eol = 1 when a newline or EOF terminated the field.
 * Returns NULL only on allocation failure.
 */
static char *csv__read_field(const char **p, int *eol, char sep)
{
    *eol = 0;

    size_t len = 0, cap = 32;
    char  *buf = (char *)malloc(cap);
    if (!buf) return NULL;

    const char *s = *p;
    if (*s == '"') {
        s++;
        while (*s) {
            if (*s == '"') {
                if (*(s + 1) == '"') {
                    if (!csv__buf_push(&buf, &len, &cap, '"')) return NULL;
                    s += 2;
                } else {
                    s++;
                    break;
                }
            } else {
                if (!csv__buf_push(&buf, &len, &cap, *s)) return NULL;
                s++;
            }
        }
    } else {
        while (*s && *s != sep && *s != '\n' && *s != '\r') {
            if (!csv__buf_push(&buf, &len, &cap, *s)) return NULL;
            s++;
        }
    }

    if      (*s == sep)  s++;
    else if (*s == '\r') { s++; if (*s == '\n') s++; *eol = 1; }
    else if (*s == '\n') { s++; *eol = 1; }
    else                  *eol = 1; /* '\0' — end of input */

    buf[len] = '\0';
    *p = s;
    return buf;
}

/* ---- type inference ---- */

static CsvType csv__infer_cell(const char *s)
{
    if (!s || *s == '\0') return CSV_STRING;
    const char *p = s;
    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) return CSV_STRING;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '\0') return CSV_INT;
    if (*p != '.') return CSV_STRING;
    p++;
    while (isdigit((unsigned char)*p)) p++;
    if ((*p == 'f' || *p == 'F') && *(p + 1) == '\0') return CSV_FLOAT;
    return (*p == '\0') ? CSV_DOUBLE : CSV_STRING;
}

static CsvType csv__infer_column(char **cells, int row_count, int col, int col_count)
{
    if (row_count == 0) return CSV_STRING;
    CsvType t = CSV_INT;
    for (int r = 0; r < row_count; r++) {
        CsvType ct = csv__infer_cell(cells[(size_t)r * col_count + col]);
        if (ct > t) t = ct;
        if (t == CSV_STRING) break;
    }
    return t;
}

/* ---- column conversion ---- */

static int csv__convert_column(char **cells, int row_count, int col, int col_count,
                                CsvType type, CsvColData *out)
{
    if (row_count == 0) { out->i = NULL; return 1; }

    switch (type) {
        case CSV_INT: {
            int *arr = (int *)malloc((size_t)row_count * sizeof(int));
            if (!arr) return 0;
            for (int r = 0; r < row_count; r++)
                arr[r] = (int)strtol(cells[(size_t)r * col_count + col], NULL, 10);
            out->i = arr;
            return 1;
        }
        case CSV_FLOAT: {
            float *arr = (float *)malloc((size_t)row_count * sizeof(float));
            if (!arr) return 0;
            for (int r = 0; r < row_count; r++)
                arr[r] = strtof(cells[(size_t)r * col_count + col], NULL);
            out->f = arr;
            return 1;
        }
        case CSV_DOUBLE: {
            double *arr = (double *)malloc((size_t)row_count * sizeof(double));
            if (!arr) return 0;
            for (int r = 0; r < row_count; r++)
                arr[r] = strtod(cells[(size_t)r * col_count + col], NULL);
            out->d = arr;
            return 1;
        }
        case CSV_STRING: {
            char **arr = (char **)malloc((size_t)row_count * sizeof(char *));
            if (!arr) return 0;
            for (int r = 0; r < row_count; r++) {
                const char *src = cells[(size_t)r * col_count + col];
                arr[r] = csv__strndup(src, strlen(src));
                if (!arr[r]) {
                    for (int j = 0; j < r; j++) free(arr[j]);
                    free(arr);
                    return 0;
                }
            }
            out->s = arr;
            return 1;
        }
    }
    return 0; /* unreachable */
}

/* ---- CSV tokeniser ---- */

/*
 * Parse the entire text into a flat, row-major array of cell strings and
 * a separate array of column names.  The caller owns all returned memory.
 *
 * On success: fills *out_names, *out_cols, *out_cells, *out_rows and returns 1.
 * On failure: returns 0 (all allocations are cleaned up internally).
 */
static int csv__parse_cells(const char *text,
                              char ***out_names, int *out_cols,
                              char ***out_cells, int *out_rows,
                              char sep)
{
    const char *p = text;
    /* skip UTF-8 BOM */
    if ((unsigned char)p[0] == 0xEF &&
        (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) p += 3;

    /* --- header row --- */
    int    col_count = 0;
    size_t names_cap = 8;
    char **names = (char **)calloc(names_cap, sizeof(char *));
    if (!names) return 0;

    int eol = 0;
    while (!eol && *p) {
        if ((size_t)col_count >= names_cap) {
            names_cap *= 2;
            char **t = (char **)realloc(names, names_cap * sizeof(char *));
            if (!t) goto cleanup_names;
            names = t;
        }
        char *f = csv__read_field(&p, &eol, sep);
        if (!f) goto cleanup_names;
        names[col_count++] = f;
    }
    if (col_count == 0) goto cleanup_names;

    /* --- data rows --- */
    int    row_count = 0;
    size_t cells_cap = (size_t)col_count * 16;
    char **cells = (char **)calloc(cells_cap, sizeof(char *));
    if (!cells) goto cleanup_names;

    while (*p) {
        /* skip blank lines */
        while (*p == '\r' || *p == '\n') p++;
        if (*p == '\0') break;

        /* grow the cells buffer to fit one more row */
        if ((size_t)(row_count + 1) * (size_t)col_count > cells_cap) {
            size_t old_cap = cells_cap;
            cells_cap *= 2;
            char **t = (char **)realloc(cells, cells_cap * sizeof(char *));
            if (!t) goto cleanup_cells;
            cells = t;
            memset(cells + old_cap, 0, (cells_cap - old_cap) * sizeof(char *));
        }

        int col = 0;
        eol = 0;
        while (!eol && *p) {
            char *f = csv__read_field(&p, &eol, sep);
            if (!f) goto cleanup_cells;
            if (col < col_count)
                cells[(size_t)row_count * col_count + col] = f;
            else
                free(f); /* extra columns are discarded */
            col++;
        }
        /* fill any missing fields with empty string */
        for (; col < col_count; col++) {
            char *empty = csv__strndup("", 0);
            if (!empty) goto cleanup_cells;
            cells[(size_t)row_count * col_count + col] = empty;
        }
        row_count++;
    }

    *out_names = names;
    *out_cols  = col_count;
    *out_cells = cells;
    *out_rows  = row_count;
    return 1;

cleanup_cells:
    /* cells are calloc'd / memset to NULL, so free(NULL) is safe here */
    for (size_t i = 0; i < cells_cap; i++) free(cells[i]);
    free(cells);
cleanup_names:
    for (int i = 0; i < col_count; i++) free(names[i]);
    free(names);
    return 0;
}

/* ---- document builder ---- */

/*
 * Build a CsvDocument from raw cell strings and an optional schema.
 * Does NOT take ownership of names[] or cells[]; the caller frees them.
 */
static CsvDocument *csv__build(char **names, int col_count,
                                char **cells, int row_count,
                                const CsvSchema *schema, int schema_count)
{
    CsvDocument *doc = (CsvDocument *)malloc(sizeof(CsvDocument));
    if (!doc) return NULL;
    doc->col_count = col_count;
    doc->row_count = row_count;
    doc->names = (char **)calloc((size_t)col_count, sizeof(char *));
    doc->types = (CsvType *)calloc((size_t)col_count, sizeof(CsvType));
    doc->data  = (CsvColData *)calloc((size_t)col_count, sizeof(CsvColData));

    if (!doc->names || !doc->types || !doc->data) { csv_free(doc); return NULL; }

    /* duplicate column names */
    for (int i = 0; i < col_count; i++) {
        doc->names[i] = csv__strndup(names[i], strlen(names[i]));
        if (!doc->names[i]) { csv_free(doc); return NULL; }
    }

    /* determine types and convert each column */
    for (int i = 0; i < col_count; i++) {
        CsvType type = CSV_STRING;
        int found = 0;
        for (int j = 0; j < schema_count; j++) {
            if (strcmp(schema[j].name, names[i]) == 0) {
                type = schema[j].type;
                found = 1;
                break;
            }
        }
        if (!found) type = csv__infer_column(cells, row_count, i, col_count);

        doc->types[i] = type;
        if (!csv__convert_column(cells, row_count, i, col_count, type, &doc->data[i])) {
            csv_free(doc);
            return NULL;
        }
    }

    return doc;
}

/* ---- shared parse entry point ---- */

static char *csv__read_file(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)size, f);
    if (ferror(f)) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[nread] = '\0';
    fclose(f);
    return buf;
}

static CsvDocument *csv__parse_do(const char *text,
                                   const CsvSchema *schema, int schema_count,
                                   char sep)
{
    char **names; int col_count;
    char **cells; int row_count;

    if (!csv__parse_cells(text, &names, &col_count, &cells, &row_count, sep))
        return NULL;

    CsvDocument *doc = csv__build(names, col_count, cells, row_count,
                                   schema, schema_count);

    for (int i = 0; i < row_count * col_count; i++) free(cells[i]);
    free(cells);
    for (int i = 0; i < col_count; i++) free(names[i]);
    free(names);
    return doc;
}

/* ---- public API ---- */

CsvDocument *csv_parse_string(const char *text, char sep)
{
    return csv__parse_do(text, NULL, 0, sep);
}

CsvDocument *csv_parse_string_schema(const char *text,
                                      const CsvSchema *schema, int schema_count, char sep)
{
    return csv__parse_do(text, schema, schema_count, sep);
}

CsvDocument *csv_load(const char *filename, char sep)
{
    char *buf = csv__read_file(filename);
    if (!buf) return NULL;
    CsvDocument *doc = csv__parse_do(buf, NULL, 0, sep);
    free(buf);
    return doc;
}

CsvDocument *csv_load_schema(const char *filename,
                              const CsvSchema *schema, int schema_count, char sep)
{
    char *buf = csv__read_file(filename);
    if (!buf) return NULL;
    CsvDocument *doc = csv__parse_do(buf, schema, schema_count, sep);
    free(buf);
    return doc;
}

DatArray csv_get(CsvDocument *doc, const char *name)
{
    for (int i = 0; i < doc->col_count; i++) {
        if (strcmp(doc->names[i], name) == 0) {
            DatArray arr;
            arr.type = doc->types[i];
            arr.size = doc->row_count;
            switch (doc->types[i]) {
                case CSV_INT:    arr.i = doc->data[i].i; break;
                case CSV_FLOAT:  arr.f = doc->data[i].f; break;
                case CSV_DOUBLE: arr.d = doc->data[i].d; break;
                case CSV_STRING: arr.s = doc->data[i].s; break;
                default:         arr.i = NULL;            break;
            }
            return arr;
        }
    }
    fprintf(stderr, "csv_get: field '%s' not found\n", name);
    exit(EXIT_FAILURE);
}

static const char *csv__type_name(CsvType t)
{
    switch (t) {
        case CSV_INT:    return "int";
        case CSV_FLOAT:  return "float";
        case CSV_DOUBLE: return "double";
        case CSV_STRING: return "string";
        default:         return "unknown";
    }
}

void csv_get_schema(CsvDocument *doc, CsvSchema *out)
{
    for (int i = 0; i < doc->col_count; i++) {
        out[i].name = doc->names[i];
        out[i].type = doc->types[i];
    }
}

void csv_print_schema(CsvDocument *doc)
{
    for (int i = 0; i < doc->col_count; i++)
        printf("%-20s %s\n", doc->names[i], csv__type_name(doc->types[i]));
}

void csv_free(CsvDocument *doc)
{
    if (!doc) return;
    if (doc->names) {
        for (int i = 0; i < doc->col_count; i++) free(doc->names[i]);
        free(doc->names);
    }
    if (doc->data && doc->types) {
        for (int i = 0; i < doc->col_count; i++) {
            if (!doc->data[i].i) continue; /* column not initialized */
            if (doc->types[i] == CSV_STRING) {
                for (int r = 0; r < doc->row_count; r++) free(doc->data[i].s[r]);
            }
            free(doc->data[i].i); /* union — same pointer regardless of type */
        }
    }
    free(doc->types);
    free(doc->data);
    free(doc);
}

#endif /* SKN_CSV_IMPLEMENTATION */
#endif /* SKN_CSV_H */
