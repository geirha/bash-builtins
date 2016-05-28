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
    ARRAY_ELEMENT *v;   // used when sorting array in-place
    char *key;          // used when sorting assoc array
    char *value;        // points to value of array element or assoc entry
    double num;         // used for numeric sort
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
            return strcoll(e2.value, e1.value);
        else
            return strcoll(e1.value, e2.value);
    }
}

static int
sort_inplace(SHELL_VAR *var) {
    size_t i, n;
    ARRAY *a;
    ARRAY_ELEMENT *ae;
    sort_element *sa = 0;

    a = array_cell(var);
    n = array_num_elements(a);

    if ( n == 0 )
        return EXECUTION_SUCCESS;

    sa = xmalloc(n * sizeof(sort_element));

    i = 0;
    for (ae = element_forw(a->head); ae != a->head; ae = element_forw(ae)) {
        sa[i].v = ae;
        sa[i].value = element_value(ae);
        if (numeric_flag)
            sa[i].num = strtod(element_value(ae), NULL);
        i++;
    }

    // sanity check
    if ( i != n ) {
        builtin_error("%s: corrupt array", var->name);
        return EXECUTION_FAILURE;
    }

    qsort(sa, n, sizeof(sort_element), compare);

    // for in-place sort, simply "rewire" the array elements
    sa[0].v->prev = sa[n-1].v->next = a->head;
    a->head->next = sa[0].v;
    a->head->prev = sa[n-1].v;
    a->max_index = n - 1;
    for (i = 0; i < n; i++) {
        sa[i].v->ind = i;
        if (i > 0)
            sa[i].v->prev = sa[i-1].v;
        if (i < n - 1)
            sa[i].v->next = sa[i+1].v;
    }
    xfree(sa);
    return EXECUTION_SUCCESS;
}

int
asort_builtin(list)
    WORD_LIST *list;
{
    SHELL_VAR *var;
    char *word;
    int opt, ret;

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
        return EX_USAGE;
    }


    while (list) {
        word = list->word->word;
        var = find_variable(word);
        list = list->next;

        if (var == 0 || array_p(var) == 0) {
            builtin_error("%s: Not an array", word);
            continue;
        }
        if (readonly_p(var) || noassign_p(var)) {
            if (readonly_p(var))
                err_readonly(word);
            continue;
        }

        if ( (ret = sort_inplace(var)) != EXECUTION_SUCCESS )
            return ret;
    }
    return EXECUTION_SUCCESS;

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

