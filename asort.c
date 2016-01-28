#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"
#include "xmalloc.h"
#include "bashgetopt.h"

static int reverse_flag;

static int
string_compare(const void *p1, const void *p2) {
    const ARRAY_ELEMENT *e1 = *(ARRAY_ELEMENT * const *) p1;
    const ARRAY_ELEMENT *e2 = *(ARRAY_ELEMENT * const *) p2;
    if (reverse_flag)
        return -1 * strcoll(e1->value, e2->value);
    return strcoll(e1->value, e2->value);
}
static int
number_compare(const void *p1, const void *p2) {
    const ARRAY_ELEMENT *e1 = *(ARRAY_ELEMENT * const *) p1;
    const ARRAY_ELEMENT *e2 = *(ARRAY_ELEMENT * const *) p2;
    double d1 = strtod(e1->value, NULL);
    double d2 = strtod(e2->value, NULL);
    int r = reverse_flag ? -1 : 1;
    if (d1 < d2)
        return r * -1;
    else if (d1 > d2)
        return r * 1;
    return 0;
}

int
asort_builtin(list)
    WORD_LIST *list;
{
    SHELL_VAR *var;
    ARRAY *a;
    ARRAY_ELEMENT *ae, *n;
    ARRAY_ELEMENT **sa;
    size_t i;
    int done = 0;
    int numeric_flag = 0;
    char *word;
    int opt;

    reverse_flag = 0;

    reset_internal_getopt();
    while ((opt = internal_getopt(list, "nr")) != -1) {
        switch (opt) {
            case 'n': numeric_flag = 1; break;
            case 'r': reverse_flag = 1; break;
            CASE_HELPOPT;
            default:
                builtin_usage();
                return (EX_USAGE);
        }
    }
    list = loptend;

    if (list == 0) {
        builtin_usage();
        return(EX_USAGE);
    }

    while (list) {
        word = list->word->word;
        list = list->next;
        var = find_variable(word);

        if (var == 0 || array_p(var) == 0) {
            builtin_error("%s: Not an array", word);
            continue;
        }
        if (readonly_p(var) || noassign_p(var)) {
            if (readonly_p(var))
                err_readonly(word);
            continue;
        }

        a = array_cell(var);

        if (a->num_elements == 0)
            continue;

        sa = xmalloc(a->num_elements * sizeof(ARRAY_ELEMENT*));

        i = 0;
        for (ae = a->head->next; ae != a->head; ae = ae->next) {
            sa[i++] = ae;
        }
        if (numeric_flag)
            qsort(sa, a->num_elements, sizeof(ARRAY_ELEMENT*), number_compare);
        else
            qsort(sa, a->num_elements, sizeof(ARRAY_ELEMENT*), string_compare);

        sa[0]->prev = sa[a->num_elements-1]->next = a->head;
        a->head->next = sa[0];
        a->head->prev = sa[a->num_elements-1];
        a->max_index = a->num_elements-1;
        for (i = 0; i < a->num_elements; i++) {
            sa[i]->ind = i;
            if (i > 0)
                sa[i]->prev = sa[i-1];
            if (i < a->num_elements - 1)
                sa[i]->next = sa[i+1];
        }
        free(sa);
    }
    return (EXECUTION_SUCCESS);
}

char *asort_doc[] = {
    "Sort arrays in-place.",
    "",
    "Options:",
    "  -n  compare according to string numerical value",
    "  -r  reverse the result of comparisons",
    (char *)NULL
};

struct builtin asort_struct = {
    "asort",
    asort_builtin,
    BUILTIN_ENABLED,
    asort_doc,
    "asort [-nr] array...",
    0
};

