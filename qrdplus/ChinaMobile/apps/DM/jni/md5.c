#ifdef WIN32
#include "std_header.h"
#endif
#define _MD5_C_

#include "libmem.h"
#include "md5.h"

/* Constants for DM_MD5Transform routine.*/
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static unsigned char PADDING[64] = { 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0 };

/*  ���ܺ���*/
static void DM_MD5Transform(unsigned long int[4], unsigned char[64]);
static void DM_Encode(unsigned char *, unsigned long int *, unsigned int);
static void DM_Decode(unsigned long int *, unsigned char *, unsigned int);
static void DM_MD5_memcpy(unsigned char*, unsigned char*, unsigned int);
static void DM_MD5_memset(unsigned char*, int, unsigned int);

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
 Rotation is separate from addition to prevent recomputation.
 */
// Note: gcc cannot parse multi-line macros in files with DOS lineends
#define FF(a, b, c, d, x, s, ac) { (a) += F ((b), (c), (d)) + (x) + (unsigned long int)(ac); (a) = ROTATE_LEFT ((a), (s)); (a) += (b); }
#define GG(a, b, c, d, x, s, ac) { (a) += G ((b), (c), (d)) + (x) + (unsigned long int)(ac); (a) = ROTATE_LEFT ((a), (s)); (a) += (b); }
#define HH(a, b, c, d, x, s, ac) { (a) += H ((b), (c), (d)) + (x) + (unsigned long int)(ac); (a) = ROTATE_LEFT ((a), (s)); (a) += (b); }
#define II(a, b, c, d, x, s, ac) { (a) += I ((b), (c), (d)) + (x) + (unsigned long int)(ac); (a) = ROTATE_LEFT ((a), (s)); (a) += (b); }

/* MD5 initialization. Begins an MD5 operation, writing a new context.
 */
void DM_MD5Init(MD5_CTX *context) {
    context->count[0] = context->count[1] = 0;
    /* Load magic initialization constants.
     */
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
 operation, processing another message block, and updating the
 context.
 */
void DM_MD5Update(MD5_CTX *context, /* context */
char *input, /* input block */
unsigned int inputLen /* length of input block */
) {
    unsigned int i, index, partLen;

    /* Compute number of bytes mod 64 */
    index = (unsigned int) ((context->count[0] >> 3) & 0x3F);

    /* Update number of bits */
    if ((context->count[0] += ((unsigned long int) inputLen << 3))
            < ((unsigned long int) inputLen << 3))
        context->count[1]++;
    context->count[1] += ((unsigned long int) inputLen >> 29);

    partLen = 64 - index;

    /* Transform as many times as possible.
     */
    if (inputLen >= partLen) {
        DM_MD5_memcpy((unsigned char*) &context->buffer[index],
                (unsigned char*) input, partLen);
        DM_MD5Transform(context->state, context->buffer);

        for (i = partLen; i + 63 < inputLen; i += 64)
            DM_MD5Transform(context->state, (unsigned char *) &input[i]);

        index = 0;
    } else
        i = 0;

    /* Buffer remaining input */
    DM_MD5_memcpy((unsigned char*) &context->buffer[index],
            (unsigned char*) &input[i], inputLen - i);
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
 the message digest and zeroizing the context.
 */
void DM_MD5Final(char digest[16], /* message digest */
MD5_CTX *context /* context */
) {
    unsigned char bits[8];
    unsigned int index, padLen;

    /* Save number of bits */
    DM_Encode(bits, context->count, 8);

    /* Pad out to 56 mod 64.
     */
    index = (unsigned int) ((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    DM_MD5Update(context, (char *) PADDING, padLen);

    /* Append length (before padding) */
    DM_MD5Update(context, (char *) bits, 8);
    /* Store state in digest */
    DM_Encode((unsigned char *) digest, context->state, 16);

    /* Zeroize sensitive information.
     */
    DM_MD5_memset((unsigned char*) context, 0, sizeof(*context));
}

/* MD5 basic transformation. Transforms state based on block.
 */
static void DM_MD5Transform(unsigned long int state[4], unsigned char block[64]) {
    unsigned long int a = state[0], b = state[1], c = state[2], d = state[3],
            x[16];

    DM_Decode(x, block, 64);

    /* Round 1 */
    FF(a, b, c, d, x[0], S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, x[1], S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, x[2], S13, 0x242070db); /* 3 */
    FF(b, c, d, a, x[3], S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, x[4], S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, x[5], S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, x[6], S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, x[7], S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[8], S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[9], S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

    /* Round 2 */
    GG(a, b, c, d, x[1], S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, x[6], S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[0], S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, x[5], S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[10], S22, 0x2441453); /* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[4], S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[9], S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[3], S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[8], S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[2], S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, x[7], S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH(a, b, c, d, x[5], S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[8], S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[1], S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, x[4], S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, x[7], S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[0], S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, x[3], S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, x[6], S34, 0x4881d05); /* 44 */
    HH(a, b, c, d, x[9], S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II(a, b, c, d, x[0], S41, 0xf4292244); /* 49 */
    II(d, a, b, c, x[7], S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[5], S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[3], S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[1], S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[8], S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[6], S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[4], S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[2], S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    /* Zeroize sensitive information.*/
    DM_MD5_memset((unsigned char*) x, 0, sizeof(x));
}

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
 a multiple of 4.
 */
static void DM_Encode(unsigned char *output, unsigned long int *input,
        unsigned int len) {
    unsigned int i, j;

    for (i = 0, j = 0; j < len - 3; i++, j += 4) //modify  2009.12.20 ,"j<len"->"j<len-3" for warning
            {
        output[j] = (unsigned char) (input[i] & 0xff);
        output[j + 1] = (unsigned char) ((input[i] >> 8) & 0xff);
        output[j + 2] = (unsigned char) ((input[i] >> 16) & 0xff);
        output[j + 3] = (unsigned char) ((input[i] >> 24) & 0xff);
    }
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
 a multiple of 4.
 */
static void DM_Decode(unsigned long int *output, unsigned char *input,
        unsigned int len) {
    unsigned int i, j;

    for (i = 0, j = 0; j < len - 3; i++, j += 4) //modify  2009.12.20,"j<len"->"j<len-3" for warning
            {
        output[i] = ((unsigned long int) input[j])
                | (((unsigned long int) input[j + 1]) << 8)
                | (((unsigned long int) input[j + 2]) << 16)
                | (((unsigned long int) input[j + 3]) << 24);
    }
}

/* Note: Replace "for loop" with standard memcpy if possible.
 */

static void DM_MD5_memcpy(unsigned char* output, unsigned char* input,
        unsigned int len) {
    unsigned int i;

    for (i = 0; i < len; i++)
        output[i] = input[i];
}

/* Note: Replace "for loop" with standard memset if possible.
 */
static void DM_MD5_memset(unsigned char* output, int value, unsigned int len) {
    unsigned int i;

    for (i = 0; i < len; i++)
        ((char *) output)[i] = (char) value;
}

/***************************************************************
 The following code implements the calculations of H(A1), H(A2),
 request-digest and response-digest, and a test program which computes
 the values used in the example of section 3.5. It uses the MD5
 implementation from RFC 1321.
 ***************************************************************/

void DM_CvtHex(HASH Bin, HASHHEX Hex) {
    unsigned short i;
    unsigned char j;

    for (i = 0; i < HASHLEN; i++) {
        j = (unsigned char) ((Bin[i] >> 4) & 0xf);
        if (j <= 9)
            Hex[i * 2] = (char) (j + '0');
        else
            Hex[i * 2] = (char) (j + 'a' - 10);
        j = (unsigned char) (Bin[i] & 0xf);
        if (j <= 9)
            Hex[i * 2 + 1] = (char) (j + '0');
        else
            Hex[i * 2 + 1] = (char) (j + 'a' - 10);
    };
    Hex[HASHHEXLEN] = '\0';
}

/* calculate H(A1) as per spec */
void DM_DigestCalcHA1(char * pszAlg, char * pszUserName, char * pszRealm,
        char * pszPassword, char * pszNonce, char * pszCNonce,
        HASHHEX SessionKey) {
    MD5_CTX Md5Ctx;
    HASH HA1;

    DM_MD5Init(&Md5Ctx);
    DM_MD5Update(&Md5Ctx, pszUserName, xppStrlen(pszUserName));
    DM_MD5Update(&Md5Ctx, ":", 1);
    DM_MD5Update(&Md5Ctx, pszRealm, xppStrlen(pszRealm));
    DM_MD5Update(&Md5Ctx, ":", 1);
    DM_MD5Update(&Md5Ctx, pszPassword, xppStrlen(pszPassword));
    DM_MD5Final(HA1, &Md5Ctx);
    if (xppStricmp(pszAlg, "md5-sess") == 0) {
        DM_MD5Init(&Md5Ctx);
        DM_MD5Update(&Md5Ctx, HA1, HASHLEN);
        DM_MD5Update(&Md5Ctx, ":", 1);
        DM_MD5Update(&Md5Ctx, pszNonce, xppStrlen(pszNonce));
        DM_MD5Update(&Md5Ctx, ":", 1);
        DM_MD5Update(&Md5Ctx, pszCNonce, xppStrlen(pszCNonce));
        DM_MD5Final(HA1, &Md5Ctx);
    };
    DM_CvtHex(HA1, SessionKey);
}

/* calculate request-digest/response-digest as per HTTP Digest spec */
void DM_DigestCalcResponse(HASHHEX HA1, /* H(A1) */
char * pszNonce, /* nonce from server */
char * pszNonceCount, /* 8 hex digits */
char * pszCNonce, /* client nonce */
char * pszQop, /* qop-value: "", "auth", "auth-int" */
char * pszMethod, /* method from the request */
char * pszDigestUri, /* requested URL */
HASHHEX HEntity, /* H(entity body) if qop="auth-int" */
HASHHEX Response /* request-digest or response-digest */
) {
    MD5_CTX Md5Ctx;
    HASH HA2;
    HASH RespHash;
    HASHHEX HA2Hex;

    // calculate H(A2)
    DM_MD5Init(&Md5Ctx);
    DM_MD5Update(&Md5Ctx, pszMethod, xppStrlen(pszMethod));
    DM_MD5Update(&Md5Ctx, ":", 1);
    DM_MD5Update(&Md5Ctx, pszDigestUri, xppStrlen(pszDigestUri));
    if (xppStricmp(pszQop, "auth-int") == 0) {
        DM_MD5Update(&Md5Ctx, ":", 1);
        DM_MD5Update(&Md5Ctx, HEntity, HASHHEXLEN);
    };
    DM_MD5Final(HA2, &Md5Ctx);
    DM_CvtHex(HA2, HA2Hex);

    // calculate response
    DM_MD5Init(&Md5Ctx);
    DM_MD5Update(&Md5Ctx, HA1, HASHHEXLEN);
    DM_MD5Update(&Md5Ctx, ":", 1);
    DM_MD5Update(&Md5Ctx, pszNonce, xppStrlen(pszNonce));
    DM_MD5Update(&Md5Ctx, ":", 1);
    if (*pszQop) {
        DM_MD5Update(&Md5Ctx, pszNonceCount, xppStrlen(pszNonceCount));
        DM_MD5Update(&Md5Ctx, ":", 1);
        DM_MD5Update(&Md5Ctx, pszCNonce, xppStrlen(pszCNonce));
        DM_MD5Update(&Md5Ctx, ":", 1);
        DM_MD5Update(&Md5Ctx, pszQop, xppStrlen(pszQop));
        DM_MD5Update(&Md5Ctx, ":", 1);
    };
    DM_MD5Update(&Md5Ctx, HA2Hex, HASHHEXLEN);
    DM_MD5Final(RespHash, &Md5Ctx);
    DM_CvtHex(RespHash, Response);
}

