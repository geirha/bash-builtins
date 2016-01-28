#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"
#include "xmalloc.h"
#include "bashgetopt.h"

typedef struct sort_element {
    ARRAY_ELEMENT *v;
    double num; // used for numeric sort
} sort_element;

static int reverse_flag;
static int numeric_flag;

static int
compare(const void *p1, const void *p2) {
    const sort_element e1 = *(sort_element *) p1;
    const sort_element e2 = *(sort_element *) p2;

    if (numeric_flag) {
        if (reverse_flag)
            return (e2.num > e1.num) ? 1 : (e2.num < e1.num) ? -1 : 0;
        else
            return (e1.num > e2.num) ? 1 : (e1.num < e2.num) ? -1 : 0;
    }
    else {
        if (reverse_flag)
            return strcoll(e2.v->value, e1.v->value);
        else
            return strcoll(e1.v->value, e2.v->value);
    }
}

int
asort_builtin(list)
    WORD_LIST *list;
{
    SHELL_VAR *var;
    ARRAY *a;
    ARRAY_ELEMENT *ae, *n;
    sort_element *sa;
    size_t i;
    int done = 0;
    char *word;
    int opt;

    numeric_flag = 0;
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

    sa = 0;

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

        sa = xrealloc(sa, a->num_elements * sizeof(sort_element));

        i = 0;
        for (ae = a->head->next; ae != a->head; ae = ae->next) {
            sa[i].v = ae;
            if (numeric_flag)
                sa[i].num = strtod(ae->value, NULL);
            i++;
        }
        qsort(sa, a->num_elements, sizeof(sort_element), compare);

        sa[0].v->prev = sa[a->num_elements-1].v->next = a->head;
        a->head->next = sa[0].v;
        a->head->prev = sa[a->num_elements-1].v;
        a->max_index = a->num_elements - 1;
        for (i = 0; i < a->num_elements; i++) {
            sa[i].v->ind = i;
            if (i > 0)
                sa[i].v->prev = sa[i-1].v;
            if (i < a->num_elements - 1)
                sa[i].v->next = sa[i+1].v;
        }
    }
    free(sa);
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

