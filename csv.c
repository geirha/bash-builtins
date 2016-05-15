#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"
#include "xmalloc.h"
#include "bashgetopt.h"

/* 
 *  field is malloced, and filled with the next field separated either by the
 *  field separator or record separator. Caller should free it.
 *
 *  returns the separator (fs or rs or '\n') or -1 if eof or error
 * TODO: print error messages
 * TODO: keep quotes if field is incomplete(?)
 */

typedef struct CSV_context {
    int fd;     // fd to read from (default: 0)
    int col;    // current column number (starting from 0)
    int rs;     // record separator (default: -1, meaning "\r\n" or '\n')
    int fs;     // field separator (default: ',')
    int q;      // quote character (default: '"')
} CSV_context;


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
csv_builtin(list)
    WORD_LIST *list;
{
    SHELL_VAR *var;
    ARRAY *a;
    char *word;
    char *buf = NULL;
    intmax_t intval;
    int opt, sep, eor, ret;

    CSV_context ctx = { 0, -1, -1, ',', '"' };

    reset_internal_getopt();
    while ( (opt = internal_getopt(list, "d:f:q:u:")) != -1 ) {
        switch (opt) {
            case 'd': ctx.rs = list_optarg[0]; break;
            case 'f': ctx.fs = list_optarg[0]; break;
            case 'q': ctx.q = list_optarg[0]; break;
            case 'u': 
                ret = legal_number(list_optarg, &intval);
                if (ret == 0 || intval < 0 || intval != (int) intval) {
                    builtin_error ("%s: invalid file descriptor specification", list_optarg);
                    return EXECUTION_FAILURE;
                }
                ctx.fd = intval;
                break;
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

    eor = 0;
    while (list) {
        word = list->word->word;
        if ( eor )
            bind_variable(word, "", 0);
        else if ( (sep = read_csv_field(&buf, &ctx)) >= 0 ) {
            bind_variable(word, buf, 0);
            xfree(buf);
        }
        else
            bind_variable(word, "", 0);
        if ( sep == ctx.rs || ctx.rs == -1 && sep == '\n' )
            eor = 1;
        
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
    "  -f sep   split fields on SEP instead of the default comma",
    "  -d delim read until the first character of DELIM is read,",
    "           rather than the default newline optionally preceded",
    "           by carriage return.",
    "  -q quote use QUOTE as quote character instead of the default",
    "           double quote (\").",
    "  -u fd    read from file descriptor FD instead of the standard input.",
    "",
    "Exit Status:",
    "To be continued...",
    (char *)NULL
};

struct builtin csv_struct = {
    "csv",
    csv_builtin,
    BUILTIN_ENABLED,
    csv_doc,
    "csv [-f sep] [-d delim] [-q quote] [-u fd] name ...",
    0
};

