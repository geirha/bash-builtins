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
    int print_mode;
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
skip_field(arrayind_t n, CSV_context *ctx)
{
    if ( ctx->cut == 0 || n > array_max_index(ctx->cut) && ctx->to_inf )
        return 0;
    return array_reference(ctx->cut, n) ? 0 : 1;
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
 *  field separator or row separator. Caller should free it.
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
    char *key, *value;
    char ibuf[INT_STRLEN_BOUND (intmax_t) + 1]; // used by fmtulong
    
    if ( header && array_empty(array_cell(header)) )
        read_into_array(header, NULL, ctx);

    ctx->col = -1;
    while ( (ret = read_csv_field(&value, ctx)) >= 0 ) {
        if ( !skip_field(ctx->col, ctx) ) {
            if ( assoc_p(array) ) {
                key = NULL;
                if ( header )
                    key = array_reference(array_cell(header), ctx->col);
                if ( key == NULL || key[0] == '\0' )
                    key = fmtulong(ctx->col, 10, ibuf, sizeof(ibuf), 0);
                bind_assoc_variable(array, array->name, savestring(key), value, 0);
            }
            else
                bind_array_element(array, ctx->col, value, 0);
        }
        xfree(value);
        if ( ret == ctx->rs || ctx->rs == -1 && ret == '\n' )
            return EXECUTION_SUCCESS;
    }
    if ( value[0] && !skip_field(ctx->col, ctx) ) {
        if (assoc_p(array)) {
            key = NULL;
            if ( header )
                key = array_reference(array_cell(header), ctx->col);
            if ( key == NULL || key[0] == '\0' )
                key = fmtulong(ctx->col, 10, ibuf, sizeof(ibuf), 0);
            bind_assoc_variable(array, array->name, savestring(key), value, 0);
        }
        else
            bind_array_element(array, ctx->col, value, 0);
        xfree(value);
        return EXECUTION_SUCCESS;
    }
    xfree(value);
    return EXECUTION_FAILURE;

}

void
print_field(char *word, CSV_context *ctx) {
    char special[5];
    size_t i, n = 0;

    if (ctx->q > 0)
        special[n++] = ctx->q;
    if (ctx->fs > 0)
        special[n++] = ctx->fs;
    if (ctx->rs > 0)
        special[n++] = ctx->rs;
    else if (ctx->rs == -1) {
        special[n++] = '\r';
        special[n++] = '\n';
    }
    special[n] = '\0';

    if ( strpbrk(word, special) ) {
        putchar(ctx->q);
        while (*word) {
            if (*word == ctx->q)
                putchar(ctx->q);
            putchar(*word);
            word++;
        }
        putchar(ctx->q);
    }
    else
        printf("%s", word);
}

void
print_array(ARRAY *a, CSV_context *ctx) {
    ARRAY_ELEMENT *ae;
    int first_printed = 0;

    if ( !a )
        return;

    for (ae = element_forw(a->head); ae != a->head; ae = element_forw(ae)) {
        if ( skip_field(element_index(ae), ctx) )
            continue;
        if ( first_printed )
            putchar(ctx->fs);
        print_field(element_value(ae), ctx);
        first_printed = 1;
    }
    if ( ctx->rs == -1 )
        printf("\r\n");
    else
        putchar(ctx->rs);
}

static char unsigned assoc_no_header_warn = 0;

void
print_assoc(HASH_TABLE *hash, ARRAY *header, CSV_context *ctx) {
    ARRAY_ELEMENT *ae;
    int first_printed = 0;
    char *data;

    if ( !hash ) {
        if ( ctx->rs == -1 )
            printf("\r\n");
        else
            putchar(ctx->rs);
        return;
    }

    if ( !header ) {
        if ( !assoc_no_header_warn++ )
            builtin_warning("Associative array without header. Values will be printed in arbitrary order"); 
        print_array(array_from_word_list(assoc_to_word_list(hash)), ctx);
        return;
    }

    for (ae = element_forw(header->head); ae != header->head; ae = element_forw(ae)) {
        if ( skip_field(element_index(ae), ctx) )
            continue;
        if ( first_printed )
            putchar(ctx->fs);
        data = assoc_reference(hash, element_value(ae));
        if (data)
            print_field(data, ctx);
        first_printed = 1;
    }
    if ( ctx->rs == -1 )
        printf("\r\n");
    else
        printf("\n");
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
        0,      // to_inf
        0       // print_mode
    };


    reset_internal_getopt();
    while ( (opt = internal_getopt(list, "ad:f:F:pq:u:")) != -1 ) {
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
            case 'p': ctx.print_mode = 1; break;
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
            if ( !ctx.print_mode )
                assoc_flush(assoc_cell(array));
        }
        else {
            array = find_or_make_array_variable(list->word->word, 1);
            if ( array == 0 )
                return EXECUTION_FAILURE;
            if ( !ctx.print_mode )
                array_flush(array_cell(array));
        }
        VUNSETATTR(array, att_invisible);
        
        list = list->next;
        if ( list ) {    
            if ( legal_identifier(list->word->word) == 0 ) {
                sh_invalidid(list->word->word);
                return EXECUTION_FAILURE;
            }
            //header = find_or_make_array_variable(list->word->word, 1);
            header = find_variable(list->word->word);
            if ( header == 0 )
                return EXECUTION_FAILURE;
            VUNSETATTR(header, att_invisible);
        }
        if ( ctx.print_mode ) {
            if ( assoc_p(array) )
                print_assoc(assoc_cell(array), header ? array_cell(header) : NULL, &ctx);
            else if ( array_p(array) )
                print_array(array_cell(array), &ctx);
            return EXECUTION_SUCCESS;
        }
        else {
            ret = read_into_array(array, header, &ctx);
            zsyncfd(ctx.fd);
        }
        return ret;
    }

    if ( ctx.print_mode ) {
        print_array(array_from_word_list(list), &ctx);
        return EXECUTION_SUCCESS;
    }


    // Let's go through the list before-hand and check if any of the variables are invalid
    while ( list ) {
        if ( legal_identifier (list->word->word) == 0 && valid_array_reference (list->word->word, 0) == 0 ) {
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
            while ( !eos && (sep = read_csv_field(&buf, &ctx)) >= 0 && skip_field(ctx.col, &ctx) ) {
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

            if ( skip_field(ctx.col, &ctx) )
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
    "commas, or sep if the -F option is supplied, and the first field",
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
    "           of numbers and ranges. e.g. -f-2,5,7-8 will pick fields",
    "           0, 1, 2, 5, 7, and 8.",
    "  -F sep   split fields on SEP instead of comma",
    "  -p       print a csv row instead of reading one. Each NAME are printed",
    "           separated by SEP, and quoted if necessary. If the -a option",
    "           is supplied, the first NAME is treated as an array holding the",
    "           values for the row. A second NAME may be provided to specify",
    "           the order of the printed fields.",
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
    "csv [-ap] [-d delim] [-f list] [-F sep] [-q quote] [-u fd] name ...",
    0
};

