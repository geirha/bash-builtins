#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"
#include "xmalloc.h"
#include "bashgetopt.h"

typedef struct CSV_context {
    int fd;     // fd to read from (default: 0)
    int col;    // current column number (starting from 0)
    int rs;     // record separator (default: -1, meaning "\r\n" or '\n')
    int fs;     // field separator (default: ',')
    int q;      // quote character (default: '"')
    ARRAY *cut; // list of fields to include
    int to_inf; // 1 if -f ended with N-, 0 otherwise
} CSV_context;

/* Copied from builtins/read.def */
static SHELL_VAR *
bind_read_variable (name, value)
     char *name, *value;
{
  SHELL_VAR *v;

#if defined (ARRAY_VARS)
  if (valid_array_reference (name, 0) == 0)
    v = bind_variable (name, value, 0);
  else
    v = assign_array_element (name, value, 0);
#else /* !ARRAY_VARS */
  v = bind_variable (name, value, 0);
#endif /* !ARRAY_VARS */
  return (v == 0 ? v
                 : ((readonly_p (v) || noassign_p (v)) ? (SHELL_VAR *)NULL : v));
}

int
skip_field(CSV_context *ctx)
{
    if ( ctx->col > array_max_index(ctx->cut) && ctx->to_inf )
        return 0;
    return array_reference(ctx->cut, ctx->col) ? 0 : 1;
}

int
parse_list(char *s, CSV_context *ctx)
{
    ARRAY *a;
    char *range;
    arrayind_t i, from, to;

    a = array_create();
    
    for (range = strtok(s, ","); range; range = strtok(NULL, ",")) {
        from = 0;
        to = -1;
        if (*range == '\0')
            return 0;
        else if (*range == '-') { // -M
            to = 0;
            while ( *++range ) {
                if ( ! isdigit(*range) )
                    return 0;
                to = to * 10 + *range - '0';
            }
        }
        else if ( ! isdigit(*range) )
            return 0;
        else {                  // N, N-M or N-
            from = *range - '0';
            while ( *++range && *range != '-') {
                if ( ! isdigit(*range) )
                    return 0;
                from = from * 10 + *range - '0';
            }
            if (*range == '\0') {
                to = from;
            }
            else if ( *(range+1) == 0 )
                to = -1;
            else {
                to = 0;
                while ( *++range ) {
                    if ( ! isdigit(*range) )
                        return 0;
                    to = to * 10 + *range - '0';
                }
            }
        }
        if ( to == -1 ) {
            array_insert(a, from, "");
            ctx->to_inf = 1;
        }
        else if (to < from)
            return 0;
        else {
            for (i = from; i <= to; ++i) {
                array_insert(a, i, "");
            }
        }
    }
    ctx->cut = a;
    return 1;
}

/*
 *  field is malloced, and filled with the next field separated either by the
 *  field separator or record separator. Caller should free it.
 *
 *  returns the separator (fs or rs or '\n') or -1 if eof or error
 * TODO: print error messages
 */

int
read_csv_field(char **field, CSV_context *ctx)
{
    char c, *p;
    char quote = 0;
    size_t n = 0, alloced = 1024;
    int ret;

    p = xmalloc(1024 * sizeof(char));
    ctx->col++;

    while ( (ret = zreadc(ctx->fd, &c)) > 0 ) {
        if (c == '\0' && ctx->fs != '\0' && ctx->rs != '\0')
            continue;
        if (quote) {
            if (c == ctx->q) {
                ret = zreadc(ctx->fd, &c);
                if (c != ctx->q)
                    quote = 0;
                else
                    p[n++] = c;
            }
            else
                p[n++] = c;
        }
        if (!quote) {
            if (c == ctx->fs || c == ctx->rs)
                break;
            if (ctx->rs == -1 && c == '\n') {
                if (n > 0 && p[n-1] == '\r')
                    n--;
                break;
            }
            if (c == ctx->q)
                quote = 1;
            else
                p[n++] = c;
        }
        if (n == alloced) {
            p = xrealloc(p, (alloced += 1024) * sizeof(char));
        }
    }

    p[n] = '\0';
    *field = p;

    if (ret <= 0)
        return -1;

    return c;
}

int
skip_csv_row(CSV_context *ctx)
{

    int ret, quote = 0;
    char c;

    while ( (ret = zreadc(ctx->fd, &c)) >= 0 ) {
        if (quote) {
            if (c == ctx->q) {
                ret = zreadc(ctx->fd, &c);
                if (c != ctx->q)
                    quote = 0;
            }
        }
        if (!quote) {
            if (c == ctx->rs || ctx->rs == -1 && c == '\n')
                break;
            if (c == ctx->q)
                quote = 1;
        }
    }
    if (ret <= 0)
        return -1;
    return c;
}

int
read_into_array(SHELL_VAR *array, SHELL_VAR *header, CSV_context *ctx) {

    int ret;
    char *key, *value, ibuf[INT_STRLEN_BOUND (intmax_t) + 1];
    ARRAY *row_a = NULL, *header_a = NULL;
    HASH_TABLE *row_h = NULL;
    
    
    if ( header ) {
        header_a = array_cell(header);
        if ( array_empty(header_a) )
            read_into_array(header, NULL, ctx);
    }

    if ( assoc_p(array) )
        row_h = assoc_cell(array);
    else
        row_a = array_cell(array);
    
    ctx->col = -1;
    while ( (ret = read_csv_field(&value, ctx)) >= 0 ) {
        if ( !skip_field(ctx) ) {
            if (row_a)
                array_insert(row_a, ctx->col, value);
            else if (row_h) {
                key = NULL;
                if (header_a)
                    key = array_reference(header_a, ctx->col);
                if (key == NULL)
                    key = fmtulong(ctx->col, 10, ibuf, sizeof(ibuf), 0);
                assoc_insert(row_h, savestring(key), value);
            }
        }
        xfree(value);
        if ( ret == ctx->rs || ctx->rs == -1 && ret == '\n' )
            return EXECUTION_SUCCESS;
    }
    if ( value[0] && !skip_field(ctx) ) {
        if (row_a)
            array_insert(row_a, ctx->col, value);
        else if (row_h) {
            key = NULL;
            if (header_a)
                key = array_reference(header_a, ctx->col);
            if (key == NULL)
                key = fmtulong(ctx->col, 10, ibuf, sizeof(ibuf), 0);
            assoc_insert(row_h, savestring(key), value);
        }
        xfree(value);
        return EXECUTION_SUCCESS;
    }
    xfree(value);
    return EXECUTION_FAILURE;

}

int
csv_builtin(WORD_LIST *list)
{
    SHELL_VAR *var;
    SHELL_VAR *array;
    SHELL_VAR *header;
    ARRAY *a = NULL;
    char *word;
    char *buf = NULL;
    intmax_t intval;
    int opt, sep, eor, eos, ret;
    int use_array = 0;

    CSV_context ctx = {
        0,      // fd
        -1,     // col
        -1,     // rs
        ',',    // fs
        '"',    // q
        0,      // cut array
        0       // to_inf
    };


    reset_internal_getopt();
    while ( (opt = internal_getopt(list, "ad:f:F:q:u:")) != -1 ) {
        switch (opt) {
            case 'a': use_array=1; break;
            case 'd': ctx.rs = list_optarg[0]; break;
            case 'f':
                if (parse_list(list_optarg, &ctx) == 0) {
                    builtin_error("-f: illegal list value");
                    return EXECUTION_FAILURE;
                }
                break;
            case 'F': ctx.fs = list_optarg[0]; break;
            case 'q': ctx.q = list_optarg[0]; break;
            case 'u':
                ret = legal_number(list_optarg, &intval);
                if (ret == 0 || intval < 0 || intval != (int) intval) {
                    builtin_error ("%s: invalid file descriptor specification", list_optarg);
                    return EXECUTION_FAILURE;
                }
                ctx.fd = intval;
                break;
            CASE_HELPOPT;   // --help handler in bash-4.4
            default:
                builtin_usage();
                return EX_USAGE;
        }
    }
    list = loptend;

    if ( list == 0 ) {
        builtin_usage();
        return EX_USAGE;
    }

    if ( use_array ) {
        array = header = NULL;
        if ( legal_identifier(list->word->word) == 0 ) {
            sh_invalidid(list->word->word);
            return EXECUTION_FAILURE;
        }
        array = find_variable(list->word->word);
        if ( array && assoc_p(array) ) {
            array = find_or_make_array_variable(list->word->word, 1|2);
            if ( array == 0 )
                return EXECUTION_FAILURE;
            assoc_flush(assoc_cell(array));
        }
        else {
            array = find_or_make_array_variable(list->word->word, 1);
            if ( array == 0 )
                return EXECUTION_FAILURE;
            array_flush(array_cell(array));
        }
        VUNSETATTR(array, att_invisible);
        
        list = list->next;
        if ( list ) {    
            if ( legal_identifier(list->word->word) == 0 ) {
                sh_invalidid(list->word->word);
                return EXECUTION_FAILURE;
            }
            header = find_or_make_array_variable(list->word->word, 1);
            if ( header == 0 )
                return EXECUTION_FAILURE;
            VUNSETATTR(header, att_invisible);
        }
        ret = read_into_array(array, header, &ctx);
        zsyncfd(ctx.fd);
        return ret;
    }

    // Let's go through the list before-hand and check if any of the variables are invalid
    while (list) {
        if (legal_identifier (list->word->word) == 0 && valid_array_reference (list->word->word, 0) == 0) {
            sh_invalidid(list->word->word);
            return EXECUTION_FAILURE;
        }
        list = list->next;
    }
    list = loptend;

    eor = eos = 0;
    while (list) {
        word = list->word->word;
        if ( eor )
            bind_read_variable(word, "");
        else {
            while ( !eos && (sep = read_csv_field(&buf, &ctx)) >= 0 && skip_field(&ctx) ) {
                if ( sep == ctx.rs || ctx.rs == -1 && sep == '\n' ) {
                    eor = 1;
                    break;
                }
                xfree(buf);
            }
            if (sep == -1) {
                if (eos)
                    return EXECUTION_FAILURE;
                eos = 1;
            }

            if ( skip_field(&ctx) )
                bind_read_variable(word, "");
            else
                bind_read_variable(word, buf);
            xfree(buf);
        }

        list = list->next;
    }
    if (sep == -1 && buf[0] == 0)
        return EXECUTION_FAILURE;
    if ( !(sep == ctx.rs || ctx.rs == -1 && sep == '\n') )
        skip_csv_row(&ctx);
    zsyncfd(ctx.fd);
    return EXECUTION_SUCCESS;
}

char *csv_doc[] = {
    "Read CSV rows",
    "",
    "Reads a CSV row from standard input, or from file descriptor FD",
    "if the -u option is supplied.  The line is split into fields on",
    "commas, or sep if the -f option is supplied, and the first field",
    "is assigned to the first NAME, the second field to the second",
    "NAME, and so on.  If there are more fields in the row than",
    "supplied NAMEs, the remaining fields are read and discarded.",
    "",
    "Options:",
    "  -a       assign the fields read to sequential indices of the array",
    "           named by the first NAME, which may be associative.",
    "           If a second NAME to an indexed array is provided, its values",
    "           will be used as keys if the first NAME is an associative",
    "           array. If the second NAME is empty, a row will be read into",
    "           it first",
    "  -d delim read until the first character of DELIM is read,",
    "           rather than newline or carriage return and newline.",
    "  -f list  read only the listed fields. LIST is a comma separated list",
    "           of numbers and ranges. e.g. -f0-2,5,7-8 will pick fields",
    "           0, 1, 2, 5, 7, and 8.",
    "  -F sep   split fields on SEP instead of comma",
    "  -q quote use QUOTE as quote character, rather than `\"'.",
    "  -u fd    read from file descriptor FD instead of the standard input.",
    "",
    "Exit Status:",
    "The return code is zero, unless an error occured, or end-of-file was",
    "encountered with no bytes read.",
    (char *)NULL
};

struct builtin csv_struct = {
    "csv",
    csv_builtin,
    BUILTIN_ENABLED,
    csv_doc,
    "csv [-a] [-d delim] [-f list] [-F sep] [-q quote] [-u fd] name ...",
    0
};

