#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"
#include "xmalloc.h"

typedef struct file {
    struct stat stat;
    char *name;
} file;

static int
compare(const void *p1, const void *p2) {
    const file f1 = *(file *) p1;
    const file f2 = *(file *) p2;

    if (f1.stat.st_mtime > f2.stat.st_mtime)
        return 1;
    if (f1.stat.st_mtime < f2.stat.st_mtime)
        return -1;

    // if seconds are equal, check nano seconds
#ifdef __linux__
    if (f1.stat.st_mtim.tv_nsec > f2.stat.st_mtim.tv_nsec)
        return 1;
    if (f1.stat.st_mtim.tv_nsec < f2.stat.st_mtim.tv_nsec)
        return -1;
#elif __APPLE__
    if (f1.stat.st_mtimensec > f2.stat.st_mtimensec)
        return 1;
    if (f1.stat.st_mtimensec < f2.stat.st_mtimensec)
        return -1;
#endif
    return 0;
}

static size_t
num_words(WORD_LIST *list) {
    size_t count = 0;
    while (list) {
        ++count;
        list = list->next;
    }
    return count;
}

int
fsort_builtin(WORD_LIST *list) {
    SHELL_VAR *var;
    ARRAY *array;
    size_t i, n;
    file *filelist;
    char *word;

    if (list == 0) {
        builtin_usage();
        return EX_USAGE;
    }
    var = find_or_make_array_variable(list->word->word, 1);
    if (var == 0)
        return EXECUTION_FAILURE;
    array = array_cell(var);

    list = list->next;

    filelist = xmalloc((num_words(list)+1) * sizeof(file));
    n = 0;
    while (list) {
        filelist[n].name = list->word->word;
        if (stat(filelist[n].name, &(filelist[n].stat)) != -1)
	    n++;

        //printf("name = %s, mtime = %ld.%09ld\n", filelist[n-1].name, filelist[n-1].stat.st_mtim.tv_sec, filelist[n-1].stat.st_mtim.tv_nsec);
        list = list->next;
    }

    qsort(filelist, n, sizeof(file), compare);

    for ( i = 0; i < n; ++i ) {
        array_insert(array, i, filelist[i].name);
    }
    xfree(filelist);
    return EXECUTION_SUCCESS;
}

char *fsort_doc[] = {
    "Sort files by metadata.",
    "",
    "Sorts FILEs by metadata (only mtime for now), and stores the",
    "filenames in ARRAY.",
    "",
    "Exit status:",
    "Return value is zero unless an error happened (like invalid variable name",
    "or readonly array).",
    (char *)NULL
};

struct builtin fsort_struct = {
    "fsort",
    fsort_builtin,
    BUILTIN_ENABLED,
    fsort_doc,
    "fsort array [file...]",
    0
};

