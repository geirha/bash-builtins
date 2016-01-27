#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"

int string_compare(const void *p1, const void *p2) {
    const ARRAY_ELEMENT *e1 = *(ARRAY_ELEMENT * const *) p1;
    const ARRAY_ELEMENT *e2 = *(ARRAY_ELEMENT * const *) p2;
    return strcoll(e1->value, e2->value);
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
    char *word;

    if (list == 0) {
        builtin_usage();
        return(EX_USAGE);
    }

    while (list) {
        word = list->word->word;
        list = list->next;
        var = find_variable(word);

        if (var == 0 || array_p(var) == 0) {
            // FIXME: find the builtin equivalent for report_error
            report_error("%s: Not an array", word);
            return (EXECUTION_FAILURE);
        }

        a = array_cell(var);

        if (a->num_elements == 0)
            continue;

        sa = xmalloc(a->num_elements * sizeof(ARRAY_ELEMENT*));

        i = 0;
        for (ae = a->head->next; ae != a->head; ae = ae->next) {
            sa[i++] = ae;
        }

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
    (char *)NULL
};

struct builtin asort_struct = {
    "asort",
    asort_builtin,
    BUILTIN_ENABLED,
    asort_doc,
    "asort array...",
    0
};

