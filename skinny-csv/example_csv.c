#define SKN_CSV_IMPLEMENTATION
#include "skn_csv.h"

static void print_csv(CsvDocument *doc)
{
    /* header */
    for (int c = 0; c < doc->col_count; c++) {
        printf("%-12s", doc->names[c]);
        if (c < doc->col_count - 1) printf("  ");
    }
    printf("\n");

    /* rows */
    for (int r = 0; r < doc->row_count; r++) {
        for (int c = 0; c < doc->col_count; c++) {
            switch (doc->types[c]) {
                case CSV_INT:    printf("%-12d",  doc->data[c].i[r]); break;
                case CSV_FLOAT:  printf("%-12gf", doc->data[c].f[r]); break;
                case CSV_DOUBLE: printf("%-12g",  doc->data[c].d[r]); break;
                case CSV_STRING: printf("%-12s",  doc->data[c].s[r]); break;
            }
            if (c < doc->col_count - 1) printf("  ");
        }
        printf("\n");
    }
}

int main(void)
{
    CsvDocument *doc = csv_load("example.csv", ',');
    if (!doc) { fprintf(stderr, "failed to load example.csv\n"); return 1; }

    /* --- print the full table --- */
    printf("=== CSV contents (%d rows) ===\n", doc->row_count);
    print_csv(doc);

    /* --- print the schema --- */
    printf("\n=== Schema ===\n");
    csv_print_schema(doc);

    /* --- retrieve the 'age' column --- */
    printf("\n=== Column 'age' ===\n");
    DatArray age = csv_get(doc, "age");
    printf("type : %s\n", age.type == CSV_INT ? "int" : "other");
    printf("size : %d\n", age.size);
    for (int i = 0; i < age.size; i++)
        printf("  age[%d] = %d\n", i, age.i[i]);

    csv_free(doc);
    return 0;
}
