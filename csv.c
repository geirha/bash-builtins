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
    int unsigned *field_list;
    size_t field_list_size;
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
skip_field(n, ctx)
int n;
CSV_context *ctx;
{
    size_t i;
    if ( ctx->field_list_size == 0 )
        return 0;
    for (i = 0; i < ctx->field_list_size; ++i) {
        if ( ctx->field_list[i] > n)
            return 1;
        if ( ctx->field_list[i] == n)
            return 0;
    }
    return 1;
}

int
parse_list(s, ctx)
char *s;
CSV_context *ctx;
{
    char *c, *d, *bc, *bd;
    int unsigned from, to, i;
    intmax_t intval;
    int n, ret;
    int unsigned *array;
    size_t size;
    if ( !isdigit(*s) )
        return 0;
    
    array = xmalloc(sizeof(int) * 100);   // TODO: realloc as necessary
    size = 0;

    for (c = strtok_r(s, ",", &bc); c; c = strtok_r(NULL, ",", &bc)) {
        d = strtok_r(c, "-", &bd);
        ret = legal_number(d, &intval);
        if (ret == 0 || intval < 0 || intval != (int) intval)
            return 0;
        from = to = intval;

        d = strtok_r(NULL, "-", &bd);
        if (d) {
            ret = legal_number(d, &intval);
            if (ret == 0 || intval < from || intval != (int) intval)
                return 0;
            to = intval;
            if ( strtok_r(NULL, "-", &bd) )
                return 0;
        }

        for (i = from; i <= to; ++i)
            array[size++] = i;
    }
    ctx->field_list = array;
    ctx->field_list_size = size;
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
read_csv_field(field, ctx)
char **field;
CSV_context *ctx;
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
skip_csv_row(ctx)
CSV_context * ctx;
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
read_into_array(a, ctx)
ARRAY *a;
CSV_context *ctx;
{
    int sep;
    char *buf;

    array_flush(a);     // a=()
    while ( (sep = read_csv_field(&buf, ctx)) >= 0 ) {
        if ( !skip_field(ctx->col, ctx) )
            //TODO: array_insert may not be enough. Needs more work
            array_insert(a, ctx->col, buf);     // a[col]=$buf
        xfree(buf);
        if (sep == ctx->rs || ctx->rs == -1 && sep == '\n')
            return EXECUTION_SUCCESS;
    }
    if (buf[0] && !skip_field(ctx->col, ctx) ) {
        array_insert(a, ctx->col, buf);     // a[col]=$buf
        return EXECUTION_SUCCESS;
    }
    xfree(buf);
    return EXECUTION_FAILURE;
}

int
read_into_assoc(h, header, ctx)
HASH_TABLE *h;
ARRAY *header;
CSV_context *ctx;
{
    int sep;
    char *key, *buf, ibuf[INT_STRLEN_BOUND (intmax_t) + 1];

    assoc_flush(h);  // h=()

    // if header is an empty array, read the row into header instead
    if ( header && array_empty(header) )
        return read_into_array(header, ctx);
    
    while ( (sep = read_csv_field(&buf, ctx)) >= 0 ) {
        if ( !skip_field(ctx->col, ctx) ) {
            key = NULL;
            if (header)
                key = array_reference(header, ctx->col);
            if (key == NULL)
                key = fmtulong(ctx->col, 10, ibuf, sizeof(ibuf), 0);
            //TODO: assoc_insert is not enough. Needs more work
            assoc_insert(h, savestring(key), buf);  // h[${header[col]-$col}]=$buf  
        }
        xfree(buf);
        if (sep == ctx->rs || ctx->rs == -1 && sep == '\n')
            return EXECUTION_SUCCESS;
    }
    if (buf[0] && !skip_field(ctx->col, ctx)) {
        key = NULL;
        if (header)
            key = array_reference(header, ctx->col);
        if (key == NULL)
            key = fmtulong(ctx->col, 10, ibuf, sizeof(ibuf), 0);
        assoc_insert(h, savestring(key), buf);  // h[${header[col]-$col}]=$buf  
        return EXECUTION_SUCCESS;
    }
    xfree(buf);
    return EXECUTION_FAILURE;
}


int
csv_builtin(list)
WORD_LIST *list;
{
    SHELL_VAR *var;
    ARRAY *a = NULL;
    char *word;
    char *buf = NULL;
    intmax_t intval;
    int opt, sep, eor, ret;
    int use_array = 0;

    CSV_context ctx = {
        0,      // fd
        -1,     // col
        -1,     // rs
        ',',    // fs
        '"',    // q
        0,      // field_list
        0       // field_list_size
    };


    reset_internal_getopt();
    while ( (opt = internal_getopt(list, "aAd:f:F:q:u:")) != -1 ) {
        switch (opt) {
            case 'a':
                if (use_array == 0 || use_array == 'a')
                    use_array = 'a';
                else {
                    builtin_error("-a conflicts with -A");
                    return EX_USAGE;
                }
                break;
            case 'A':
                if (use_array == 0 || use_array == 'A')
                    use_array = 'A';
                else {
                    builtin_error("-A conflicts with -a");
                    return EX_USAGE;
                }
                break;
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

    if (use_array == 'a') {
        if (legal_identifier(list->word->word) == 0) {
            sh_invalidid(list->word->word);
            return EXECUTION_FAILURE;
        }
        if (list->next) {
            builtin_usage();
            return EX_USAGE;
        }
        return read_into_array(array_cell(find_or_make_array_variable(list->word->word, 1)), &ctx);
    }
    else if (use_array == 'A') {
        if (legal_identifier(list->word->word) == 0) {
            sh_invalidid(list->word->word);
            return EXECUTION_FAILURE;
        }
        if (list->next) {
            if (legal_identifier(list->next->word->word) == 0) {
                sh_invalidid(list->word->word);
                return EXECUTION_FAILURE;
            }
            a = array_cell(find_or_make_array_variable(list->next->word->word, 1));
        }
        return read_into_assoc(assoc_cell(find_or_make_array_variable(list->word->word, 1|2)), a, &ctx);
    }

    if (legal_identifier (list->word->word) == 0 && valid_array_reference (list->word->word, 0) == 0) {
        sh_invalidid(list->word->word);
        return EXECUTION_FAILURE;
    }


    eor = 0;
    while (list) {
        word = list->word->word;
        printf("word: %s, eor: %d\n", word, eor);
        if (legal_identifier (word) == 0 && valid_array_reference (word, 0) == 0) {
            sh_invalidid(word);
            return EXECUTION_FAILURE;
        }
        if ( eor )
            bind_read_variable(word, "");
        else {
            while ( (sep = read_csv_field(&buf, &ctx)) >= 0 && skip_field(ctx.col, &ctx) ) {
                printf("SKIP. buf: %s, col: %d, sep: %02x\n", buf, ctx.col, sep);
                if ( sep == ctx.rs || ctx.rs == -1 && sep == '\n' ) {
                    eor = 1;
                    break;
                }
                xfree(buf);
            }
            printf("buf: %s, col: %d, sep: %02x\n", buf, ctx.col, sep);
            
            if ( skip_field(ctx.col, &ctx) )
                bind_read_variable(word, "");
            else
                bind_read_variable(word, buf);
            xfree(buf);
        }
        
        list = list->next;
    }
    if ( !(sep == ctx.rs || ctx.rs == -1 && sep == '\n') )
        skip_csv_row(&ctx);
    zsyncfd(0);
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
    "           named by the first NAME", 
    "  -A       assign the fields read to sequential indices of the array",
    "           named by the first NAME. A second NAME to an indexed array",
    "           may be provided, and will be used as keys. If second NAME",
    "           is an empty array, the fields are read into that array",
    "           instead.", 
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
    "csv [-a | -A] [-d delim] [-f list] [-F sep] [-q quote] [-u fd] name ...",
    0
};
