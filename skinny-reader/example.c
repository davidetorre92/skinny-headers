#define SKN_DAT_IMPLEMENTATION
#include "skn_dat.h"

static void print_value(DatValue *v)
{
    switch (v->type) {
        case DAT_INT:    printf("%d", v->i); break;
        case DAT_FLOAT:  printf("%gf", v->f); break;
        case DAT_DOUBLE: printf("%g", v->d); break;
        case DAT_STRING: printf("\"%s\"", v->s); break;
        case DAT_LIST: {
            printf("[");
            for (DatListNode *n = v->list.head; n; n = n->next) {
                switch (v->list.elem_type) {
                    case DAT_INT:    printf("%d", n->i); break;
                    case DAT_FLOAT:  printf("%gf", n->f); break;
                    case DAT_DOUBLE: printf("%g", n->d); break;
                    case DAT_STRING: printf("\"%s\"", n->s); break;
                    default: break;
                }
                if (n->next) printf(", ");
            }
            printf("]");
            break;
        }
    }
}

static void print_doc(DatDocument *d, int indent) 
{
    DatRow *row = d->head;
    while(row) {
        if (indent) printf("\t");
        printf("%s => ", row->key);
        print_value(&row->value);
        printf("\n");
        row=row->next;
    }
}

int main(void)
{
    DatDocument *doc = dat_load("example.dat");
    if (!doc) { fprintf(stderr, "failed to load example.dat\n"); return 1; }

    printf("parsed %d rows\n\n", doc->count);
    print_doc(doc, 1);

    /* spot-check dat_get */
    printf("\nspot checks:\n");
    DatRow *r;

    r = dat_get(doc, "count");
    if (r) printf("  count  = %d\n", r->value.i);

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
