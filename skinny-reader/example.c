#define SKN_DAT_IMPLEMENTATION
#include "skn_dat.h"

int main(void)
{
    DatDocument *doc = dat_load("example.dat");
    if (!doc) { fprintf(stderr, "failed to load example.dat\n"); return 1; }

    printf("parsed %d rows\n\n", doc->count);
    dat_print_doc(doc, 1);

    /* spot-check dat_get */
    printf("\nspot checks:\n");
    DatRow *r;

    r = dat_get(doc, "count");
    if (r) printf("  "); dat_print_row(r); printf("\n");

    r = dat_get(doc, "pi");
    if (r) printf("  pi     = %lf\n", r->value.d);

    r = dat_get(doc, "ratio");
    if (r) printf("  ratio  = %lf\n", r->value.f);

    r = dat_get(doc, "greeting");
    if (r) printf("  greeting = \"%s\"\n", r->value.s);

    r = dat_get(doc, "scores");
    if (r) {
        printf("  scores = [");
        for (DatListNode *n = r->value.list.head; n; n = n->next)
            printf("%d%s", n->i, n->next ? ", " : "");
        printf("]\n");
    }

    r = dat_get(doc, "names");
    if (r) {
        printf("  names  = [");
        for (DatListNode *n = r->value.list.head; n; n = n->next)
            printf("\"%s\"%s", n->s, n->next ? ", " : "");
        printf("]\n");
    }
    return 0;
}
