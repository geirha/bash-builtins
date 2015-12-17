#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "bashtypes.h"
#include "shell.h"
#include "builtins.h"
#include "common.h"

// Let X <<< s denote the 32-bit value obtained by circularly
// shifting (rotating) X left by s bit positions.

#define ROTATE(X, s) ((X) << (s) | (X) >> (32 - (s)))


typedef struct MD5_CTX {
    uint32_t mdbuf[4];
    size_t bitlength;
    unsigned char block[64];
    size_t block_size;
} MD5_CTX;

void
MD5Init(MD5_CTX *context) {
    memset(context->block, 0, 64);
    context->block_size = 0;
    context->bitlength = 0;

    // 3.3 Step 3. Initialize MD Buffer
    context->mdbuf[0] = 0x67452301;
    context->mdbuf[1] = 0xefcdab89;
    context->mdbuf[2] = 0x98badcfe;
    context->mdbuf[3] = 0x10325476;

}

void debug_block(uint32_t*,size_t);

void process_block(MD5_CTX *context) {
    if (context->block_size != 64) {
        fprintf(stderr, "Wrong block_size: %lu\n", context->block_size);
    }
    // At this point the resulting message (after padding with bits and with
    // b) has a length that is an exact multiple of 512 bits. Equivalently,
    // this message has a length that is an exact multiple of 16 (32-bit)
    // words. Let M[0 ... N-1] denote the words of the resulting message,
    // where N is a multiple of 16.

    uint32_t *X = (uint32_t *)context->block;
    
    // 3.4 Step 4. Process Message in 16-Word Blocks

    // We first define four auxiliary functions that each take as input
    // three 32-bit words and produce as output one 32-bit word.
    
    #define F(X,Y,Z) ( (X) & (Y) | ~(X) & (Z) )
    #define G(X,Y,Z) ( (X) & (Z) | (Y) & ~(Z) )
    #define H(X,Y,Z) ( (X) ^ (Y) ^ (Z) )
    #define I(X,Y,Z) ( (Y) ^ ( (X) | ~(Z) ) )


    // This step uses a 64-element table T[1 ... 64] constructed from the
    // sine function. Let T[i] denote the i-th element of the table, which
    // is equal to the integer part of 4294967296 times abs(sin(i)), where i
    // is in radians. The elements of the table are given in the appendix.
             
    uint32_t T[] = { 0,
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    //   /* Save A as AA, B as BB, C as CC, and D as DD. */
    uint32_t A = context->mdbuf[0];
    uint32_t B = context->mdbuf[1];
    uint32_t C = context->mdbuf[2];
    uint32_t D = context->mdbuf[3];

    // /* Round 1. */
    // /* Let [abcd k s i] denote the operation
    // a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s). */
    // /* Do the following 16 operations. */
    // [ABCD  0  7  1]  [DABC  1 12  2]  [CDAB  2 17  3]  [BCDA  3 22  4]
    // [ABCD  4  7  5]  [DABC  5 12  6]  [CDAB  6 17  7]  [BCDA  7 22  8]
    // [ABCD  8  7  9]  [DABC  9 12 10]  [CDAB 10 17 11]  [BCDA 11 22 12]
    // [ABCD 12  7 13]  [DABC 13 12 14]  [CDAB 14 17 15]  [BCDA 15 22 16]
    #define R1(a, b, c, d, k, s, i) ( (a) = \
      (b) + (ROTATE(\
        (a) + (F((b),(c),(d))) + (X[(k)]) + (T[(i)])\
        , s)\
      )\
    )
    R1(A, B, C, D,  0,  7,  1);
    R1(D, A, B, C,  1, 12,  2);
    R1(C, D, A, B,  2, 17,  3);
    R1(B, C, D, A,  3, 22,  4);
    R1(A, B, C, D,  4,  7,  5);
    R1(D, A, B, C,  5, 12,  6);
    R1(C, D, A, B,  6, 17,  7);
    R1(B, C, D, A,  7, 22,  8);
    R1(A, B, C, D,  8,  7,  9);
    R1(D, A, B, C,  9, 12, 10);
    R1(C, D, A, B, 10, 17, 11);
    R1(B, C, D, A, 11, 22, 12);
    R1(A, B, C, D, 12,  7, 13);
    R1(D, A, B, C, 13, 12, 14);
    R1(C, D, A, B, 14, 17, 15);
    R1(B, C, D, A, 15, 22, 16);


    // /* Round 2. */
    // /* Let [abcd k s i] denote the operation
    //      a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s). */
    // /* Do the following 16 operations. */
    // [ABCD  1  5 17]  [DABC  6  9 18]  [CDAB 11 14 19]  [BCDA  0 20 20]
    // [ABCD  5  5 21]  [DABC 10  9 22]  [CDAB 15 14 23]  [BCDA  4 20 24]
    // [ABCD  9  5 25]  [DABC 14  9 26]  [CDAB  3 14 27]  [BCDA  8 20 28]
    // [ABCD 13  5 29]  [DABC  2  9 30]  [CDAB  7 14 31]  [BCDA 12 20 32]
    #define R2(a, b, c, d, k, s, i) ( (a) = \
      (b) + (ROTATE(\
        (a) + (G((b),(c),(d))) + (X[(k)]) + (T[(i)])\
        , s)\
      )\
    )
    R2(A, B, C, D,  1,  5, 17);
    R2(D, A, B, C,  6,  9, 18);
    R2(C, D, A, B, 11, 14, 19);
    R2(B, C, D, A,  0, 20, 20);
    R2(A, B, C, D,  5,  5, 21);
    R2(D, A, B, C, 10,  9, 22);
    R2(C, D, A, B, 15, 14, 23);
    R2(B, C, D, A,  4, 20, 24);
    R2(A, B, C, D,  9,  5, 25);
    R2(D, A, B, C, 14,  9, 26);
    R2(C, D, A, B,  3, 14, 27);
    R2(B, C, D, A,  8, 20, 28);
    R2(A, B, C, D, 13,  5, 29);
    R2(D, A, B, C,  2,  9, 30);
    R2(C, D, A, B,  7, 14, 31);
    R2(B, C, D, A, 12, 20, 32);


    // /* Round 3. */
    // /* Let [abcd k s t] denote the operation
    //      a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s). */
    // /* Do the following 16 operations. */
    // [ABCD  5  4 33]  [DABC  8 11 34]  [CDAB 11 16 35]  [BCDA 14 23 36]
    // [ABCD  1  4 37]  [DABC  4 11 38]  [CDAB  7 16 39]  [BCDA 10 23 40]
    // [ABCD 13  4 41]  [DABC  0 11 42]  [CDAB  3 16 43]  [BCDA  6 23 44]
    // [ABCD  9  4 45]  [DABC 12 11 46]  [CDAB 15 16 47]  [BCDA  2 23 48]
    #define R3(a, b, c, d, k, s, i) ( (a) = \
      (b) + (ROTATE(\
        (a) + (H((b),(c),(d))) + (X[(k)]) + (T[(i)])\
        , s)\
      )\
    )
    R3(A, B, C, D,  5,  4, 33);
    R3(D, A, B, C,  8, 11, 34);
    R3(C, D, A, B, 11, 16, 35);
    R3(B, C, D, A, 14, 23, 36);
    R3(A, B, C, D,  1,  4, 37);
    R3(D, A, B, C,  4, 11, 38);
    R3(C, D, A, B,  7, 16, 39);
    R3(B, C, D, A, 10, 23, 40);
    R3(A, B, C, D, 13,  4, 41);
    R3(D, A, B, C,  0, 11, 42);
    R3(C, D, A, B,  3, 16, 43);
    R3(B, C, D, A,  6, 23, 44);
    R3(A, B, C, D,  9,  4, 45);
    R3(D, A, B, C, 12, 11, 46);
    R3(C, D, A, B, 15, 16, 47);
    R3(B, C, D, A,  2, 23, 48);

    // /* Round 4. */
    // /* Let [abcd k s t] denote the operation
    //      a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s). */
    // /* Do the following 16 operations. */
    // [ABCD  0  6 49]  [DABC  7 10 50]  [CDAB 14 15 51]  [BCDA  5 21 52]
    // [ABCD 12  6 53]  [DABC  3 10 54]  [CDAB 10 15 55]  [BCDA  1 21 56]
    // [ABCD  8  6 57]  [DABC 15 10 58]  [CDAB  6 15 59]  [BCDA 13 21 60]
    // [ABCD  4  6 61]  [DABC 11 10 62]  [CDAB  2 15 63]  [BCDA  9 21 64]
    #define R4(a, b, c, d, k, s, i) ( (a) = \
      (b) + (ROTATE(\
        (a) + (I((b),(c),(d))) + (X[(k)]) + (T[(i)])\
        , s)\
      )\
    )
    R4(A, B, C, D,  0,  6, 49);
    R4(D, A, B, C,  7, 10, 50);
    R4(C, D, A, B, 14, 15, 51);
    R4(B, C, D, A,  5, 21, 52);
    R4(A, B, C, D, 12,  6, 53);
    R4(D, A, B, C,  3, 10, 54);
    R4(C, D, A, B, 10, 15, 55);
    R4(B, C, D, A,  1, 21, 56);
    R4(A, B, C, D,  8,  6, 57);
    R4(D, A, B, C, 15, 10, 58);
    R4(C, D, A, B,  6, 15, 59);
    R4(B, C, D, A, 13, 21, 60);
    R4(A, B, C, D,  4,  6, 61);
    R4(D, A, B, C, 11, 10, 62);
    R4(C, D, A, B,  2, 15, 63);
    R4(B, C, D, A,  9, 21, 64);

    // /* Then perform the following additions. (That is increment each
    //    of the four registers by the value it had before this block
    //    was started.) */
    // A = A + AA
    // B = B + BB
    // C = C + CC
    // D = D + DD
    context->mdbuf[0] += A;
    context->mdbuf[1] += B;
    context->mdbuf[2] += C;
    context->mdbuf[3] += D;
    context->block_size = 0;
}

void
debug_block(uint32_t *data, size_t len) {
    for (int i = 0; i < len; i++) {
        if (i%4==0) putchar('\n');
        printf(" %08x", data[i]);
    }
    putchar('\n');
}

void
MD5Update(MD5_CTX *context, const void *data, size_t len) {
    char *p = (char*)data;
    int i = 64 - context->block_size;
    while (i && len > i) {
        memcpy(context->block + 64-i, p, i);
        context->block_size += i;
        context->bitlength += i * 8;
        p += i;
        if (context->block_size == 64)
            process_block(context);
        len -= i;
        i = 64 - context->block_size;
    }
    memcpy(context->block + 64-i, p, len);
    context->block_size += len;
    context->bitlength += len * 8;
    p += len;
    if (context->block_size == 64)
        process_block(context);
}

void
MD5Pad(MD5_CTX *context) {
    int i;
    // 3.1 Step 1. Append Padding Bits
    context->block[context->block_size++] = 0b10000000;
    if (context->block_size > 56) {
        i = 64 - context->block_size;
        memset(context->block + context->block_size, 0, i);
        context->block_size += i;
        process_block(context);
    }
    i = 56 - context->block_size;
    memset(context->block + context->block_size, 0, i);
    context->block_size += i;

    // 3.2 Step 2. Append Length
    memcpy(context->block + context->block_size, &context->bitlength, 8);
    context->block_size += 8;

}

void
MD5Final(unsigned char digest[16], MD5_CTX *context) {
    MD5Pad(context);
    process_block(context);
    //3.5 Step 5. Output
    memcpy(digest, context->mdbuf, 16);
}

int
md5_builtin(list)
    WORD_LIST *list;
{
    unsigned char digest[16];
    char hexdigest[33];
    char buf[4096];
    int n;
    SHELL_VAR *shell_REPLY;
    MD5_CTX context;

    MD5Init(&context);

	if (list == 0) {
        while (n = read(0, buf, sizeof(buf))) {
            MD5Update(&context, buf, n);
        }
	}
    else if (list->next) {
		builtin_usage();
		return(EX_USAGE);
    }
    else {
        MD5Update(&context, list->word->word, strlen(list->word->word));
    }

    MD5Final(digest, &context);
    for (int i = 0; i < 16; i++)
        sprintf(hexdigest+(i*2), "%02x", digest[i]);
    hexdigest[32] = '\0';

    shell_REPLY = bind_variable("REPLY", hexdigest, 0);

    return (EXECUTION_SUCCESS);
}

char *md5_doc[] = {
    "Calculate MD5 sum.",
    "",
    "Calculates the MD5 sum of the string argument, or stdin if no",
    "argument is proveded. The result is assigned to the REPLY ",
    "variable.",
    (char *)NULL
};

struct builtin md5_struct = {
    "md5",
    md5_builtin,
    BUILTIN_ENABLED,
    md5_doc,
    "md5 [string]",
    0
};

