#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"

void swap(ARRAY_ELEMENT *a, ARRAY_ELEMENT *b) {
    char *tmp;
    tmp = a->value;
    a->value = b->value;
    b->value = tmp;
}

int
asort_builtin(list)
    WORD_LIST *list;
{

    SHELL_VAR *var;
    ARRAY *a;
    ARRAY_ELEMENT *ae, *n;
    int done = 0;

    if (list == 0) {
        builtin_usage();
        return(EX_USAGE);
    }

    var = find_variable(list->word->word);
    if (var == 0 || array_p(var) == 0) {
        // FIXME: find the builtin equivalent for report_error
        report_error("%s: Not an array", list->word->word);
        return (EXECUTION_FAILURE);
    }

    a = array_cell(var);

    // Make sure it is non-sparse
    if (a->num_elements > 0 && a->num_elements <= a->max_index) {
        a->max_index = -1;
        for (ae = a->head->next; ae != a->head; ae = ae->next) {
            ae->ind = ++a->max_index;
        }
    }

    while (!done) {
        done = 1;
        ae = a->head->next;
        for ( ; (n = ae->next) != a->head; ae = ae->next) {
            if (strcoll(ae->value, n->value) > 0) {
                swap(ae, n);
                done = 0;
            }
        }
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

