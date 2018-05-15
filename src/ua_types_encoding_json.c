/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 *
 *    Copyright 2014-2018 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2014-2017 (c) Florian Palm
 *    Copyright 2014-2016 (c) Sten Grüner
 *    Copyright 2014 (c) Leon Urbas
 *    Copyright 2015 (c) LEvertz
 *    Copyright 2015 (c) Chris Iatrou
 *    Copyright 2015-2016 (c) Oleksiy Vasylyev
 *    Copyright 2016-2017 (c) Stefan Profanter, fortiss GmbH
 *    Copyright 2016 (c) Lorenz Haas
 *    Copyright 2017 (c) Mark Giraud, Fraunhofer IOSB
 *    Copyright 2017 (c) Henrik Norrman
 */


#include "ua_types_encoding_json.h"
#include "ua_types_encoding_binary.h"
#include "ua_types_generated.h"
#include "ua_types_generated_handling.h"

#include "../deps/libb64/cencode.h"
#include "../deps/libb64/cdecode.h"
#include "../deps/jsmn/jsmn.h"
#include "libc_time.h"


/**
 * Type Encoding and Decoding
 * --------------------------
 * The following methods contain encoding and decoding functions for the builtin
 * data types and generic functions that operate on all types and arrays. This
 * requires the type description from a UA_DataType structure.
 *
 * Encoding Context
 * ^^^^^^^^^^^^^^^^
 * If possible, the encoding context is stored in a thread-local variable to
 * speed up encoding. If thread-local variables are not supported, the context
 * is "looped through" every method call. The ``_``-macro accesses either the
 * thread-local or the "looped through" context . */

#define UA_ENCODING_MAX_RECURSION 20

typedef struct {
    jsmntok_t tokenArray[128];
    UA_Int32 tokenCount;
    UA_UInt16 *index;
} ParseCtx;

typedef status(*encodeJsonSignature)(const void *UA_RESTRICT src, const UA_DataType *type,
        Ctx *UA_RESTRICT ctx);
typedef status (*decodeJsonSignature)(void *UA_RESTRICT dst, const UA_DataType *type,
                                        Ctx *UA_RESTRICT ctx, ParseCtx *parseCtx, UA_Boolean moveToken);


#define ENCODE_JSON(TYPE) static status \
    TYPE##_encodeJson(const UA_##TYPE *UA_RESTRICT src, const UA_DataType *type, Ctx *UA_RESTRICT ctx)

#define ENCODE_DIRECT(SRC, TYPE) TYPE##_encodeJson((const UA_##TYPE*)SRC, NULL, ctx)


extern const encodeJsonSignature encodeJsonJumpTable[UA_BUILTIN_TYPES_COUNT + 1];
extern const decodeJsonSignature decodeJsonJumpTable[UA_BUILTIN_TYPES_COUNT + 1];

static status encodeJsonInternal(const void *src, const UA_DataType *type, Ctx *ctx);

UA_String UA_DateTime_toJSON(UA_DateTime t);

void addMatrixContentJSON(Ctx *ctx, void* array, const UA_DataType *type, size_t *index, UA_UInt32 *arrayDimensions, size_t dimensionIndex, size_t dimensionSize);

static UA_Boolean useReversibleForm = UA_TRUE;

ENCODE_JSON(ByteString);

/**
 * JSON HELPER
 */
#define WRITE(ELEM) writeJson##ELEM(ctx)

#define JSON(ELEM) static status writeJson##ELEM(Ctx *UA_RESTRICT ctx)

static UA_Boolean commaNeeded = UA_FALSE;
static UA_UInt32 innerObject = 0;

/*JSON(Debug) {
    *(ctx->pos++) = '@';

    return UA_STATUSCODE_GOOD;
}*/

JSON(Quote) {
    *(ctx->pos++) = '"';
    return UA_STATUSCODE_GOOD;
}

JSON(ObjStart) {
    *(ctx->pos++) = '{';
    innerObject++;
    return UA_STATUSCODE_GOOD;
}

JSON(ObjEnd) {
    *(ctx->pos++) = '}';
    innerObject--;
    commaNeeded = UA_TRUE;
    return UA_STATUSCODE_GOOD;
}

JSON(ArrayStart) {
    *(ctx->pos++) = '[';
    return UA_STATUSCODE_GOOD;
}

JSON(ArrayEnd) {
    *(ctx->pos++) = ']';
    return UA_STATUSCODE_GOOD;
}

JSON(Comma) {
    *(ctx->pos++) = ',';
    return UA_STATUSCODE_GOOD;
}

JSON(dPoint) {
    *(ctx->pos++) = ':';
    return UA_STATUSCODE_GOOD;
}

status writeComma(Ctx *ctx) {
    if (commaNeeded) {
        WRITE(Comma);
    }

    return UA_STATUSCODE_GOOD;
}

status writeKey(Ctx *ctx, const char* key) {
    writeComma(ctx);
    WRITE(Quote);
    for (size_t i = 0; i < strlen(key); i++) {
        *(ctx->pos++) = (u8)key[i];
    }
    WRITE(Quote);
    WRITE(dPoint);

    commaNeeded = UA_TRUE;
    return UA_STATUSCODE_GOOD;
}

status encodingJsonStartObject(Ctx *ctx) {
    *(ctx->pos++) = '{';
    commaNeeded = UA_FALSE;
    return UA_STATUSCODE_GOOD;
}

size_t encodingJsonEndObject(Ctx *ctx) {
    *(ctx->pos++) = '}';
    return 0;
}

status encodingJsonStartArray(Ctx *ctx) {
    *(ctx->pos++) = '[';
    commaNeeded = UA_FALSE;
    return UA_STATUSCODE_GOOD;
}

size_t encodingJsonEndArray(Ctx *ctx) {
    *(ctx->pos++) = ']';
    return 0;
}

/**
 * Chunking
 * ^^^^^^^^
 * Breaking a message into chunks is integrated with the encoding. When the end
 * of a buffer is reached, a callback is executed that sends the current buffer
 * as a chunk and exchanges the encoding buffer "underneath" the ongoing
 * encoding. This reduces the RAM requirements and unnecessary copying.
 *
 * In encodeJsonInternal and Array_encodeJson, we store a pointer to the
 * last "good position" in the buffer. If we reach the end of the buffer, the
 * encoding until that point is sent out. Afterwards the "good position" pointer
 * is no longer valid. In order to prevent reuse, no method must return
 * UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED after having called exchangeBuffer().
 * This needs to be ensured for the following methods:
 *
 * encodeJsonInternal
 * Array_encodeJson
 * NodeId_encodeJson
 * ExpandedNodeId_encodeJson
 * LocalizedText_encodeJson
 * ExtensionObject_encodeJson
 * Variant_encodeJson
 * DataValue_encodeJson
 * DiagnosticInfo_encodeJson */

/* Send the current chunk and replace the buffer */
static status exchangeBuffer(Ctx *ctx) {
    if (!ctx->exchangeBufferCallback)
        return UA_STATUSCODE_BADENCODINGERROR;
    return ctx->exchangeBufferCallback(ctx->exchangeBufferCallbackHandle, &ctx->pos, &ctx->end);
}

/* If encoding fails, exchange the buffer and try again. It is assumed that the
 * following encoding never fails on a fresh buffer. This is true for numerical
 * types. */
static status
encodeWithExchangeBuffer(const void *ptr, encodeJsonSignature encodeFunc, Ctx *ctx) {
    status ret = encodeFunc(ptr, NULL, ctx);
    if (ret == UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED) {
        ret = exchangeBuffer(ctx);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        encodeFunc(ptr, NULL, ctx);
    }
    return UA_STATUSCODE_GOOD;
}

#define ENCODE_WITHEXCHANGE(VAR, TYPE) \
    encodeWithExchangeBuffer((const void*)VAR, (encodeJsonSignature)TYPE##_encodeJson, ctx)

/*****************/
/* Integer Types */
/*****************/

#if !UA_BINARY_OVERLAYABLE_INTEGER

#pragma message "Integer endianness could not be detected to be little endian. Use slow generic encoding."

/* These en/decoding functions are only used when the architecture isn't little-endian. */
static void
UA_encode16(const u16 v, u8 buf[2]) {
    buf[0] = (u8) v;
    buf[1] = (u8) (v >> 8);
}

static void
UA_encode32(const u32 v, u8 buf[4]) {
    buf[0] = (u8) v;
    buf[1] = (u8) (v >> 8);
    buf[2] = (u8) (v >> 16);
    buf[3] = (u8) (v >> 24);
}


static void
UA_encode64(const u64 v, u8 buf[8]) {
    buf[0] = (u8) v;
    buf[1] = (u8) (v >> 8);
    buf[2] = (u8) (v >> 16);
    buf[3] = (u8) (v >> 24);
    buf[4] = (u8) (v >> 32);
    buf[5] = (u8) (v >> 40);
    buf[6] = (u8) (v >> 48);
    buf[7] = (u8) (v >> 56);
}

#endif /* !UA_BINARY_OVERLAYABLE_INTEGER */


//http://www.techiedelight.com/implement-itoa-function-in-c/
//  function to swap two numbers

void swap(char *x, char *y);
char* reverse(char *buffer, UA_UInt16 i, UA_UInt16 j);
UA_UInt16 itoaUnsigned(UA_UInt64 value, char* buffer, UA_Byte base);
UA_UInt64 UA_abs(UA_Int64 v);
UA_UInt16 itoa(UA_Int64 value, char* buffer);

void swap(char *x, char *y) {
    char t = *x;
    *x = *y;
    *y = t;
}

// function to reverse buffer[i..j]

char* reverse(char *buffer, UA_UInt16 i, UA_UInt16 j) {
    while (i < j)
        swap(&buffer[i++], &buffer[j--]);

    return buffer;
}

// Iterative function to implement itoa() function in C

UA_UInt16 itoaUnsigned(UA_UInt64 value, char* buffer, UA_Byte base) {
    // consider absolute value of number
    UA_UInt64 n = value;

    UA_UInt16 i = 0;
    while (n) {
        UA_UInt64 r = n % base;

        if (r >= 10)
            buffer[i++] = (char)(65 + (r - 10));
        else
            buffer[i++] = (char)(48 + r);

        n = n / base;
    }

    // if number is 0
    if (i == 0)
        buffer[i++] = '0';


    buffer[i] = '\0'; // null terminate string

    i--;

    // reverse the string 
    reverse(buffer, 0, i);
    i++;
    return i;
}

UA_UInt64 UA_abs(UA_Int64 v) {
    if(v < 0){
        return (UA_UInt64)-v;
    }
    
    return (UA_UInt64)v;
}

//http://www.techiedelight.com/implement-itoa-function-in-c/
// Iterative function to implement itoa() function in C

UA_UInt16 itoa(UA_Int64 value, char* buffer) {
    // consider absolute value of number
    UA_UInt64 n = UA_abs(value);



    UA_UInt16 i = 0;
    while (n) {
        UA_UInt64 r = n % 10;

        if (r >= 10)
            buffer[i++] = (char)(65 + (r - 10));
        else
            buffer[i++] = (char)(48 + r);

        n = n / 10;
    }

    // if number is 0
    if (i == 0)
        buffer[i++] = '0';

    // If base is 10 and value is negative, the resulting string 
    // is preceded with a minus sign (-)
    // With any other base, value is always considered unsigned
    if (value < 0 && 10 == 10)
        buffer[i++] = '-';

    buffer[i] = '\0'; // null terminate string

    i--;
    // reverse the string and return it
    reverse(buffer, 0, i);
    i++;
    return i;
}

/* Boolean */
ENCODE_JSON(Boolean) {

    size_t sizeOfJSONBool;
    if (*src == UA_TRUE) {
        sizeOfJSONBool = 4; //"true"
    } else {
        sizeOfJSONBool = 5; //"false";
    }

    if (ctx->pos + sizeOfJSONBool > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    if (*src == UA_TRUE) {
        *(ctx->pos++) = 't';
        *(ctx->pos++) = 'r';
        *(ctx->pos++) = 'u';
        *(ctx->pos++) = 'e';
    } else {
        *(ctx->pos++) = 'f';
        *(ctx->pos++) = 'a';
        *(ctx->pos++) = 'l';
        *(ctx->pos++) = 's';
        *(ctx->pos++) = 'e';
    }

    return UA_STATUSCODE_GOOD;
}


/* Byte */
ENCODE_JSON(Byte) {
    char buf[3]; //TODO size
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);

    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}


/* signed Byte */
ENCODE_JSON(SByte) {
    char buf[4]; //TODO size
    UA_UInt16 digits = itoa(*src, buf);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);

    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* UInt16 */
ENCODE_JSON(UInt16) {

#if UA_BINARY_OVERLAYABLE_INTEGER

    char buf[50]; //TODO size
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
#else
    //TODO UA_encode16(*src, ctx->pos);
#endif
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}


/* Int16 */
ENCODE_JSON(Int16) {

#if UA_BINARY_OVERLAYABLE_INTEGER

    char buf[50]; //TODO size
    UA_UInt16 digits = itoa(*src, buf);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
#else
    //TODO UA_encode16(*src, ctx->pos);
#endif
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* UInt32 */
ENCODE_JSON(UInt32) {

#if UA_BINARY_OVERLAYABLE_INTEGER
    char buf[50]; //TODO size
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
#else
    //UA_encode32(*src, ctx->pos);
#endif
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}


/* Int32 */
ENCODE_JSON(Int32) {

#if UA_BINARY_OVERLAYABLE_INTEGER
    char buf[50]; //TODO size
    UA_UInt16 digits = itoa(*src, buf);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
#else
    //UA_encode32(*src, ctx->pos);
#endif
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* UInt64 */
ENCODE_JSON(UInt64) {
#if UA_BINARY_OVERLAYABLE_INTEGER
    char buf[50]; //TODO size
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
#else
    UA_encode64(*src, ctx->pos);
#endif
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}


/* Int64 */
ENCODE_JSON(Int64) {

#if UA_BINARY_OVERLAYABLE_INTEGER
    char buf[50]; //TODO size
    UA_UInt16 digits = itoa(*src, buf);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    //TODO Decimal?
    memcpy(ctx->pos, buf, digits);
#else
    UA_encode32(*src, ctx->pos);
#endif
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/************************/
/* Floating Point Types */
/************************/

#if UA_BINARY_OVERLAYABLE_FLOAT
#define Float_encodeJson UInt32_encodeJson
#define Float_decodeJson UInt32_decodeJson
#define Double_encodeJson UInt64_encodeJson
#define Double_decodeJson UInt64_decodeJson
#else

#include <math.h>

#pragma message "No native IEEE 754 format detected. Use slow generic encoding."

/* Handling of IEEE754 floating point values was taken from Beej's Guide to
 * Network Programming (http://beej.us/guide/bgnet/) and enhanced to cover the
 * edge cases +/-0, +/-inf and nan. */
static uint64_t
pack754(long double f, unsigned bits, unsigned expbits) {
    unsigned significandbits = bits - expbits - 1;
    long double fnorm;
    long long sign;
    if (f < 0) {
        sign = 1;
        fnorm = -f;
    } else {
        sign = 0;
        fnorm = f;
    }
    int shift = 0;
    while (fnorm >= 2.0) {
        fnorm /= 2.0;
        ++shift;
    }
    while (fnorm < 1.0) {
        fnorm *= 2.0;
        --shift;
    }
    fnorm = fnorm - 1.0;
    long long significand = (long long) (fnorm * ((float) (1LL << significandbits) + 0.5f));
    long long exponent = shift + ((1 << (expbits - 1)) - 1);
    return (uint64_t) ((sign << (bits - 1)) | (exponent << (bits - expbits - 1)) | significand);
}

static long double
unpack754(uint64_t i, unsigned bits, unsigned expbits) {
    unsigned significandbits = bits - expbits - 1;
    long double result = (long double) (i & (uint64_t) ((1LL << significandbits) - 1));
    result /= (1LL << significandbits);
    result += 1.0f;
    unsigned bias = (unsigned) (1 << (expbits - 1)) - 1;
    long long shift = (long long) ((i >> significandbits) & (uint64_t) ((1LL << expbits) - 1)) - bias;
    while (shift > 0) {
        result *= 2.0;
        --shift;
    }
    while (shift < 0) {
        result /= 2.0;
        ++shift;
    }
    result *= ((i >> (bits - 1))&1) ? -1.0 : 1.0;
    return result;
}

/* Float */
#define FLOAT_NAN 0xffc00000
#define FLOAT_INF 0x7f800000
#define FLOAT_NEG_INF 0xff800000
#define FLOAT_NEG_ZERO 0x80000000

ENCODE_JSON(Float) {
    //TODO
    UA_Float f = *src;
    u32 encoded;
    /* cppcheck-suppress duplicateExpression */
    if (f != f) encoded = FLOAT_NAN;
    else if (f == 0.0f) encoded = signbit(f) ? FLOAT_NEG_ZERO : 0;
    else if (f / f != f / f) encoded = f > 0 ? FLOAT_INF : FLOAT_NEG_INF;
    else encoded = (u32) pack754(f, 32, 8);
    return ENCODE_DIRECT(&encoded, UInt32);
}


/* Double */
#define DOUBLE_NAN 0xfff8000000000000L
#define DOUBLE_INF 0x7ff0000000000000L
#define DOUBLE_NEG_INF 0xfff0000000000000L
#define DOUBLE_NEG_ZERO 0x8000000000000000L

ENCODE_JSON(Double) {

    //TODO
    UA_Double d = *src;
    u64 encoded;
    /* cppcheck-suppress duplicateExpression */
    if (d != d) encoded = DOUBLE_NAN;
    else if (d == 0.0) encoded = signbit(d) ? DOUBLE_NEG_ZERO : 0;
    else if (d / d != d / d) encoded = d > 0 ? DOUBLE_INF : DOUBLE_NEG_INF;
    else encoded = pack754(d, 64, 11);
    return ENCODE_DIRECT(&encoded, UInt64);
}


#endif

/******************/
/* Array Handling */

/******************/


static status
Array_encodeJsonOverlayable(uintptr_t ptr, size_t length, size_t elementMemSize, Ctx *ctx) {
    /* Store the number of already encoded elements */
    size_t finished = 0;

    //size_t encode_index = type->builtin ? type->typeIndex : UA_BUILTIN_TYPES_COUNT;
    //encodeJsonSignature encodeType = encodeJsonJumpTable[encode_index];

    /* Loop as long as more elements remain than fit into the chunk */
    /*while (ctx->end < ctx->pos + (elementMemSize * (length - finished))) {
        size_t possible = ((uintptr_t) ctx->end - (uintptr_t) ctx->pos) / (sizeof (u8) * elementMemSize);
        size_t possibleMem = possible * elementMemSize;
        memcpy(ctx->pos, (void*) ptr, possibleMem);
        ctx->pos += possibleMem;
        ptr += possibleMem;
        finished += possible;
        status ret = exchangeBuffer(ctx);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    } TODO */

    /* Encode the remaining elements */
    memcpy(ctx->pos, (void*) ptr, elementMemSize * (length - finished));
    ctx->pos += elementMemSize * (length - finished);
    return UA_STATUSCODE_GOOD;
}

static status
Array_encodeJsonComplex(uintptr_t ptr, size_t length, const UA_DataType *type, Ctx *ctx) {
    /* Get the encoding function for the data type. The jumptable at
     * UA_BUILTIN_TYPES_COUNT points to the generic UA_encodeJson method */
    size_t encode_index = type->builtin ? type->typeIndex : UA_BUILTIN_TYPES_COUNT;
    encodeJsonSignature encodeType = encodeJsonJumpTable[encode_index];

    WRITE(ArrayStart);
    commaNeeded = false;

    /* Encode every element */
    for (size_t i = 0; i < length; ++i) {

        if (commaNeeded) {
            WRITE(Comma);
        }

        u8 *oldpos = ctx->pos;
        status ret = encodeType((const void*) ptr, type, ctx);
        ptr += type->memSize;
        /* Encoding failed, switch to the next chunk when possible */
        if (ret != UA_STATUSCODE_GOOD) {
            if (ret == UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED) {
                ctx->pos = oldpos; /* Set buffer position to the end of the last encoded element */
                ret = exchangeBuffer(ctx);
                ptr -= type->memSize; /* Undo to retry encoding the ith element */
                --i;
            }
            UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
            if (ret != UA_STATUSCODE_GOOD)
                return ret; /* Unrecoverable fail */
        }

        commaNeeded = true;
    }

    commaNeeded = true;
    WRITE(ArrayEnd);
    return UA_STATUSCODE_GOOD;
}

static status
Array_encodeJson(const void *src, size_t length, const UA_DataType *type, Ctx *ctx, UA_Boolean isVariantArray) {
    /* Check and convert the array length to int32 */
    /*i32 signed_length = -1;
    if (length > UA_INT32_MAX)
        return UA_STATUSCODE_BADINTERNALERROR;
    if (length > 0)
        signed_length = (i32) length;
    else if (src == UA_EMPTY_ARRAY_SENTINEL)
        signed_length = 0;*/


    //No need for a lengt in JSON!
    /* Encode the array length */
    //status ret = ENCODE_WITHEXCHANGE(&signed_length, UInt32);

    /* Quit early? */
    //if(ret != UA_STATUSCODE_GOOD || length == 0)
    //   return ret;

    status ret = UA_STATUSCODE_GOOD;
    /* Encode the content */



    /* Byte Array  ByteString TODO*/
    if (!isVariantArray && type->typeIndex == 2 /*byte*/) {
        //UA_ByteString bs;
        //bs.data = (UA_Byte const*)src;
        //bs.length = length;
        //size_t decode_index = type->builtin ? type->typeIndex : UA_BUILTIN_TYPES_COUNT;
        //ret = ENCODE_DIRECT(&bs, ByteString);
        return ret;
    } else {
        //@TODO FIXME
        ret = Array_encodeJsonComplex((uintptr_t) src, length, type, ctx);
        return ret;
    }

    if (!type->overlayable) {
        ret = Array_encodeJsonComplex((uintptr_t) src, length, type, ctx);
    } else {
        ret = Array_encodeJsonOverlayable((uintptr_t) src, length, type->memSize, ctx);
    }


    return ret;
}

/*****************/
/* Builtin Types */

/*****************/


static char escapeLookup[256];

ENCODE_JSON(String) {

    WRITE(Quote);
    //TODO: Escape String
    memset(&escapeLookup, 0, 256);
    escapeLookup['"'] = '"';
    escapeLookup['\\'] = '\\';
    escapeLookup['\n'] = 'n';
    escapeLookup['\r'] = 'r';
    escapeLookup['\t'] = 't';
    escapeLookup['\b'] = 'b';
    escapeLookup['\f'] = 'f';

    if (ctx->pos + src->length > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    size_t i;
    for (i = 0; i < src->length; i++) {
        if (escapeLookup[src->data[i]]) {
            *(ctx->pos++) = '\\';
            *(ctx->pos++) = (u8)escapeLookup[src->data[i]];
        } else {
            *(ctx->pos++) = src->data[i];
        }
    }

    WRITE(Quote);
    return UA_STATUSCODE_GOOD;
}

ENCODE_JSON(ByteString) {

    
    //Estimate base64 size, this is a few bytes bigger https://stackoverflow.com/questions/1533113/calculate-the-size-to-a-base-64-encoded-message
    UA_UInt32 output_size = (UA_UInt32)(((src->length * 4) / 3) + (src->length / 96) + 6);

    if (ctx->pos + output_size > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;


    //set up a destination buffer large enough to hold the encoded data
    char* output = (char*) malloc(output_size);
    if(!output){
        return UA_STATUSCODE_BADENCODINGERROR;
    }
    //keep track of our encoded position 
    char* c = output;
    // store the number of bytes encoded by a single call 
    UA_Int32 cnt = 0;
    // we need an encoder state 
    base64_encodestate s;

    //---------- START ENCODING ---------
    // initialise the encoder state
    base64_init_encodestate(&s);
    // gather data from the input and send it to the output
    cnt = base64_encode_block((char*)src->data, (int)src->length, (char*)c, &s);
    c += cnt;
    // since we have encoded the entire input string, we know that 
    //   there is no more input data; finalise the encoding 
    cnt = base64_encode_blockend(c, &s);
    c += cnt;
    //---------- STOP ENCODING  ----------


    WRITE(Quote);

    //Calculate size, Lib appends one \n, -1 because of this.
    UA_UInt64 actualLength = (UA_UInt64)((c - 1) - output);
    memcpy(ctx->pos, output, actualLength);
    ctx->pos += actualLength;

    free(output);
    output = NULL;

    WRITE(Quote);
    
   return UA_STATUSCODE_GOOD;
}


char hexmapLower[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
char hexmapUpper[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/* Guid */
ENCODE_JSON(Guid) {
    status ret = UA_STATUSCODE_GOOD;
    char *hexmap = hexmapUpper; //TODO: Define 

    if (ctx->pos + 38 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    WRITE(Quote);
    char buf[36];
    memset(&buf, 0, 20);

    UA_Byte b1 = (UA_Byte)(src->data1 >> 24);
    UA_Byte b2 = (UA_Byte)(src->data1 >> 16);
    UA_Byte b3 = (UA_Byte)(src->data1 >> 8);
    UA_Byte b4 = (UA_Byte)(src->data1 >> 0);
    buf[0] = hexmap[(b1 & 0xF0) >> 4];
    buf[1] = hexmap[b1 & 0x0F];
    buf[2] = hexmap[(b2 & 0xF0) >> 4];
    buf[3] = hexmap[b2 & 0x0F];
    buf[4] = hexmap[(b3 & 0xF0) >> 4];
    buf[5] = hexmap[b3 & 0x0F];
    buf[6] = hexmap[(b4 & 0xF0) >> 4];
    buf[7] = hexmap[b4 & 0x0F];

    buf[8] = '-';

    UA_Byte b5 = (UA_Byte)(src->data2 >> 8);
    UA_Byte b6 = (UA_Byte)(src->data2 >> 0);

    buf[9] = hexmap[(b5 & 0xF0) >> 4];
    buf[10] = hexmap[b5 & 0x0F];
    buf[11] = hexmap[(b6 & 0xF0) >> 4];
    buf[12] = hexmap[b6 & 0x0F];

    buf[13] = '-';

    UA_Byte b7 = (UA_Byte)(src->data3 >> 8);
    UA_Byte b8 = (UA_Byte)(src->data3 >> 0);

    buf[14] = hexmap[(b7 & 0xF0) >> 4];
    buf[15] = hexmap[b7 & 0x0F];
    buf[16] = hexmap[(b8 & 0xF0) >> 4];
    buf[17] = hexmap[b8 & 0x0F];

    buf[18] = '-';

    UA_Byte b9 = src->data4[0];
    UA_Byte b10 = src->data4[1];

    buf[19] = hexmap[(b9 & 0xF0) >> 4];
    buf[20] = hexmap[b9 & 0x0F];
    buf[21] = hexmap[(b10 & 0xF0) >> 4];
    buf[22] = hexmap[b10 & 0x0F];

    buf[23] = '-';

    UA_Byte b11 = src->data4[2];
    UA_Byte b12 = src->data4[3];
    UA_Byte b13 = src->data4[4];
    UA_Byte b14 = src->data4[5];
    UA_Byte b15 = src->data4[6];
    UA_Byte b16 = src->data4[7];

    buf[24] = hexmap[(b11 & 0xF0) >> 4];
    buf[25] = hexmap[b11 & 0x0F];
    buf[26] = hexmap[(b12 & 0xF0) >> 4];
    buf[27] = hexmap[b12 & 0x0F];
    buf[28] = hexmap[(b13 & 0xF0) >> 4];
    buf[29] = hexmap[b13 & 0x0F];
    buf[30] = hexmap[(b14 & 0xF0) >> 4];
    buf[31] = hexmap[b14 & 0x0F];
    buf[32] = hexmap[(b15 & 0xF0) >> 4];
    buf[33] = hexmap[b15 & 0x0F];
    buf[34] = hexmap[(b16 & 0xF0) >> 4];
    buf[35] = hexmap[b16 & 0x0F];

    memcpy(ctx->pos, buf, 36);
    ctx->pos += 36;
    WRITE(Quote);

    return ret;
}


static void
printNumber(u16 n, u8 *pos, size_t digits) {
    for (size_t i = digits; i > 0; --i) {
        pos[i - 1] = (u8) ((n % 10) + '0');
        n = n / 10;
    }
}

UA_String
UA_DateTime_toJSON(UA_DateTime t) {
    /* length of the string is 31 (plus \0 at the end) */
    UA_String str = {20, (u8*) UA_malloc(20)};
    if (!str.data)
        return UA_STRING_NULL;
    UA_DateTimeStruct tSt = UA_DateTime_toStruct(t);
    printNumber(tSt.year, str.data, 4);
    str.data[4] = '-';
    printNumber(tSt.month, &str.data[5], 2);
    str.data[7] = '-';
    printNumber(tSt.day, &str.data[8], 2);
    str.data[10] = 'T';
    printNumber(tSt.hour, &str.data[11], 2);
    str.data[13] = ':';
    printNumber(tSt.min, &str.data[14], 2);
    str.data[16] = ':';
    printNumber(tSt.sec, &str.data[17], 2);
    str.data[19] = 'Z';
    return str;
}

ENCODE_JSON(DateTime) {
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    UA_String str = UA_DateTime_toJSON(*src);
    //WRITE(Quote);
    ENCODE_DIRECT(&str, String);
    //WRITE(Quote);
    UA_String_deleteMembers(&str);
    return ret;
}

/* NodeId */
#define UA_NODEIDTYPE_NUMERIC_TWOBYTE 0
#define UA_NODEIDTYPE_NUMERIC_FOURBYTE 1
#define UA_NODEIDTYPE_NUMERIC_COMPLETE 2

#define UA_EXPANDEDNODEID_SERVERINDEX_FLAG 0x40
#define UA_EXPANDEDNODEID_NAMESPACEURI_FLAG 0x80

/* For ExpandedNodeId, we prefill the encoding mask. We can return
 * UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED before encoding the string, as the
 * buffer is not replaced. */
static status
NodeId_encodeJsonWithEncodingMask(UA_NodeId const *src, u8 encoding, Ctx *ctx) {
    status ret = UA_STATUSCODE_GOOD;

    switch (src->identifierType) {
        case UA_NODEIDTYPE_NUMERIC:
        {
            commaNeeded = UA_FALSE;

            ret |= writeKey(ctx, "Id");
            ret |= ENCODE_DIRECT(&src->identifier.numeric, UInt32);

            break;
        }
        case UA_NODEIDTYPE_STRING:
        {
            commaNeeded = UA_FALSE;

            ret |= writeKey(ctx, "IdType");
            UA_UInt16 typeNumber = 1;
            ret |= ENCODE_DIRECT(&typeNumber, UInt16);
            ret |= writeKey(ctx, "Id");
            ret |= ENCODE_DIRECT(&src->identifier.string, String);

            break;
        }
        case UA_NODEIDTYPE_GUID:
        {
            commaNeeded = UA_FALSE;
         
            ret |= writeKey(ctx, "IdType");
            UA_UInt16 typeNumber = 2;
            ret |= ENCODE_DIRECT(&typeNumber, UInt16);
            /* Id */
            ret |= writeKey(ctx, "Id");
            ret |= ENCODE_DIRECT(&src->identifier.guid, Guid);

            break;
        }
        case UA_NODEIDTYPE_BYTESTRING:
        {
            commaNeeded = UA_FALSE;
            // {"IdType":0,"Text":"Text"}

            ret |= writeKey(ctx, "IdType");
            UA_UInt16 typeNumber = 3;
            ret |= ENCODE_DIRECT(&typeNumber, UInt16);
            /* Id */
            ret |= writeKey(ctx, "Id");
            ret |= ENCODE_DIRECT(&src->identifier.byteString, ByteString);

            break;
        }
        default:
            return UA_STATUSCODE_BADINTERNALERROR;
    }



    if (useReversibleForm) {
        if (src->namespaceIndex > 0) {
            ret |= writeKey(ctx, "Namespace");
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
        }
    } else {
        /* For the non-reversible encoding, the field is the NamespaceUri 
         * associated with the NamespaceIndex, encoded as a JSON string.
         * A NamespaceIndex of 1 is always encoded as a JSON number.
         */

        //@TODO LOOKUP namespace uri and check if unknown
        UA_String namespaceUri = UA_STRING("dummy");
        if (src->namespaceIndex == 1 || namespaceUri.length) {
            writeKey(ctx, "Namespace");
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
        } else {
            writeKey(ctx, "Namespace");
            ret |= WRITE(Quote);
            ret |= ENCODE_DIRECT(&src->namespaceIndex, String);
            ret |= WRITE(Quote);
        }
    }

    return ret;
}

ENCODE_JSON(NodeId) {
    WRITE(ObjStart);
    UA_StatusCode ret = NodeId_encodeJsonWithEncodingMask(src, 0, ctx);
    WRITE(ObjEnd);
    return ret;
}

/* ExpandedNodeId */
ENCODE_JSON(ExpandedNodeId) {

    WRITE(ObjStart);

    /* Set up the encoding mask */
    u8 encoding = 0;
    if ((void*) src->namespaceUri.data > UA_EMPTY_ARRAY_SENTINEL)
        encoding |= UA_EXPANDEDNODEID_NAMESPACEURI_FLAG;
    if (src->serverIndex > 0)
        encoding |= UA_EXPANDEDNODEID_SERVERINDEX_FLAG;

    /* Encode the NodeId */
    status ret = NodeId_encodeJsonWithEncodingMask(&src->nodeId, encoding, ctx);
    if (ret != UA_STATUSCODE_GOOD)
        return ret;

    /* Encode the namespace. Do not return
     * UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED afterwards. */
    if ((void*) src->namespaceUri.data > UA_EMPTY_ARRAY_SENTINEL) {

        writeKey(ctx, "Namespace");
        WRITE(Quote);
        ret = ENCODE_DIRECT(&src->namespaceUri, String);
        WRITE(Quote);
        UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    /* Encode the serverIndex */
    if (src->serverIndex > 0) {
        writeKey(ctx, "ServerUri");
        ret = ENCODE_WITHEXCHANGE(&src->serverIndex, UInt32);
    }

    UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);

    ret |= WRITE(ObjEnd);

    return ret;
}

/* LocalizedText */
#define UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_LOCALE 0x01
#define UA_LOCALIZEDTEXT_ENCODINGMASKTYPE_TEXT 0x02

ENCODE_JSON(LocalizedText) {
    status ret = UA_STATUSCODE_GOOD;

    if (useReversibleForm) {
        commaNeeded = UA_FALSE;

        WRITE(ObjStart);
        // {"Locale":"asd","Text":"Text"}
        writeKey(ctx, "Locale");
        ret |= ENCODE_DIRECT(&src->locale, String);

        writeKey(ctx, "Text");
        ret |= ENCODE_DIRECT(&src->text, String);

        WRITE(ObjEnd);
    } else {
        /* For the non-reversible form, LocalizedText value shall 
         * be encoded as a JSON string containing the Text component.*/
        ret |= ENCODE_DIRECT(&src->text, String);
    }

    UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
    return ret;
}

ENCODE_JSON(QualifiedName) {
    status ret = UA_STATUSCODE_GOOD;

    commaNeeded = UA_FALSE;

    WRITE(ObjStart);
    writeKey(ctx, "Name");
    ret |= ENCODE_DIRECT(&src->name, String);

    if (useReversibleForm) {
        if (src->namespaceIndex != 0) {
            writeKey(ctx, "Uri");
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
        }
    } else {

        /*For the non-reversible form, the NamespaceUri associated with the NamespaceIndex portion of
the QualifiedName is encoded as JSON string unless the NamespaceIndex is 1 or if
NamespaceUri is unknown. In these cases, the NamespaceIndex is encoded as a JSON number.
         */

        //@TODO LOOKUP namespace uri and check if unknown
        UA_String namespaceUri = UA_STRING("dummy");
        if (src->namespaceIndex == 1 || namespaceUri.length) {
            writeKey(ctx, "Uri");
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
        } else {
            writeKey(ctx, "Uri");
            ret |= WRITE(Quote);
            ret |= ENCODE_DIRECT(&src->namespaceIndex, String);
            ret |= WRITE(Quote);
        }
    }

    WRITE(ObjEnd);
    return ret;
}

ENCODE_JSON(StatusCode) {
    status ret = UA_STATUSCODE_GOOD;

    if (!useReversibleForm) {
        ret |= WRITE(ObjStart);
        commaNeeded = UA_FALSE;
        writeKey(ctx, "StatusCode");
        ret |= ENCODE_DIRECT(src, UInt32);

        writeKey(ctx, "Symbol");
        //const char* statusDescriptionString = UA_StatusCode_name(*src);
        UA_String statusDescription = UA_STRING("todo");// TODO: statusDescriptionString);
        ret |= ENCODE_DIRECT(&statusDescription, String);

        ret |= WRITE(ObjEnd);
    } else {
        ret |= ENCODE_DIRECT(src, UInt32);
    }

    return ret;
}

/* The json encoding has a different nodeid from the data type. So it is not
 * possible to reuse UA_findDataType */
static const UA_DataType *
UA_findDataTypeByJsonInternal(const UA_NodeId *typeId, Ctx *ctx) {
    /* We only store a numeric identifier for the encoding nodeid of data types */
    if (typeId->identifierType != UA_NODEIDTYPE_NUMERIC)
        return NULL;

    /* Always look in built-in types first
     * (may contain data types from all namespaces) */
    for (size_t i = 0; i < UA_TYPES_COUNT; ++i) {
        if (UA_TYPES[i].binaryEncodingId == typeId->identifier.numeric &&
                UA_TYPES[i].typeId.namespaceIndex == typeId->namespaceIndex)
            return &UA_TYPES[i];
    }

    /* When other namespace look in custom types, too */
    if (typeId->namespaceIndex != 0) {
        for (size_t i = 0; i < ctx->customTypesArraySize; ++i) {
            if (ctx->customTypesArray[i].binaryEncodingId == typeId->identifier.numeric &&
                    ctx->customTypesArray[i].typeId.namespaceIndex == typeId->namespaceIndex)
                return &ctx->customTypesArray[i];
        }
    }

    return NULL;
}

const UA_DataType *
UA_findDataTypeByJson(const UA_NodeId *typeId) {
    Ctx ctx;
    ctx.customTypesArraySize = 0;
    ctx.customTypesArray = NULL;
    return UA_findDataTypeByJsonInternal(typeId, &ctx);
}

/* ExtensionObject */
ENCODE_JSON(ExtensionObject) {
    u8 encoding = (u8) src->encoding;
    status ret = UA_STATUSCODE_GOOD;
    /* No content or already encoded content. Do not return
     * UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED after encoding the NodeId. */
    if (encoding <= UA_EXTENSIONOBJECT_ENCODED_XML) {
        commaNeeded = UA_FALSE;

        WRITE(ObjStart);

        writeKey(ctx, "TypeId");
        ret = ENCODE_DIRECT(&src->content.encoded.typeId, NodeId);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;

        writeKey(ctx, "Encoding");
        ret = ENCODE_WITHEXCHANGE(&encoding, Byte);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        switch (src->encoding) {
            case UA_EXTENSIONOBJECT_ENCODED_NOBODY:
                break;
            case UA_EXTENSIONOBJECT_ENCODED_BYTESTRING:
                writeKey(ctx, "Body");
                ret = ENCODE_DIRECT(&src->content.encoded.body, ByteString); /* ByteString TODO: TEST*/
                break;
            case UA_EXTENSIONOBJECT_ENCODED_XML:
                writeKey(ctx, "Body");
                ret = ENCODE_DIRECT(&src->content.encoded.body, String); /* ByteString TODO: TEST*/
                break;
            default:
                ret = UA_STATUSCODE_BADINTERNALERROR;
        }

        WRITE(ObjEnd);
        return ret;
    }

    /* Cannot encode with no data or no type description */
    if (!src->content.decoded.type || !src->content.decoded.data)
        return UA_STATUSCODE_BADENCODINGERROR;

    /* Write the NodeId for the json encoded type. The NodeId is always
     * numeric, so no buffer replacement is taking place. */
    UA_NodeId typeId = src->content.decoded.type->typeId;
    if (typeId.identifierType != UA_NODEIDTYPE_NUMERIC)
        return UA_STATUSCODE_BADENCODINGERROR;
    typeId.identifier.numeric = src->content.decoded.type->binaryEncodingId;

    if (useReversibleForm) {

        WRITE(ObjStart);

        commaNeeded = UA_FALSE;
        writeKey(ctx, "TypeId");
        ret = ENCODE_DIRECT(&typeId, NodeId);

        /* Write the encoding byte */
        encoding = 0; //TODO: encoding
        writeKey(ctx, "Encoding");
        ret |= ENCODE_DIRECT(&encoding, Byte);

        /* Return early upon failures (no buffer exchange until here) */
        if (ret != UA_STATUSCODE_GOOD)
            return ret;

        const UA_DataType *contentType = src->content.decoded.type;

        /* Encode the content */
        writeKey(ctx, "Body");
        ret |= encodeJsonInternal(src->content.decoded.data, contentType, ctx);


        WRITE(ObjEnd);
    } else {

        /* For the non-reversible form, ExtensionObject values 
         * shall be encoded as a JSON object containing only the 
         * value of the Body field. The TypeId and Encoding fields are dropped.
         * 
         * Does this mean there is a "Body" in the ExtensionObject or not?
         */
        commaNeeded = UA_FALSE;
        WRITE(ObjStart);
        const UA_DataType *contentType = src->content.decoded.type;
        writeKey(ctx, "Body");
        ret |= encodeJsonInternal(src->content.decoded.data, contentType, ctx);
        WRITE(ObjEnd);
    }
    return ret;
}

/* Variant */
/* Never returns UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED */
static status
Variant_encodeJsonWrapExtensionObject(const UA_Variant *src, const bool isArray, Ctx *ctx) {
    /* Default to 1 for a scalar. */
    size_t length = 1;

    /* Encode the array length if required */
    status ret = UA_STATUSCODE_GOOD;
    if (isArray) {
        if (src->arrayLength > UA_INT32_MAX)
            return UA_STATUSCODE_BADENCODINGERROR;
        length = src->arrayLength;
        i32 encodedLength = (i32) src->arrayLength;
        ret = ENCODE_DIRECT(&encodedLength, UInt32); /* Int32 */
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    /* Set up the ExtensionObject */
    UA_ExtensionObject eo;
    UA_ExtensionObject_init(&eo);
    eo.encoding = UA_EXTENSIONOBJECT_DECODED;
    eo.content.decoded.type = src->type;
    const u16 memSize = src->type->memSize;
    uintptr_t ptr = (uintptr_t) src->data;

    /* Iterate over the array */
    for (size_t i = 0; i < length && ret == UA_STATUSCODE_GOOD; ++i) {
        eo.content.decoded.data = (void*) ptr;
        ret = encodeJsonInternal(&eo, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT], ctx);
        ptr += memSize;
    }
    return ret;
}

enum UA_VARIANT_ENCODINGMASKTYPE {
    UA_VARIANT_ENCODINGMASKTYPE_TYPEID_MASK = 0x3F, /* bits 0:5 */
    UA_VARIANT_ENCODINGMASKTYPE_DIMENSIONS = (0x01 << 6), /* bit 6 */
    UA_VARIANT_ENCODINGMASKTYPE_ARRAY = (0x01 << 7) /* bit 7 */
};

void addMatrixContentJSON(Ctx *ctx, void* array, const UA_DataType *type, size_t *index, UA_UInt32 *arrayDimensions, size_t dimensionIndex, size_t dimensionSize) {


    if (dimensionIndex == (dimensionSize - 1)) {
        //The inner Arrays are written
        commaNeeded = UA_FALSE;

        WRITE(ArrayStart);

        for (size_t i = 0; i < arrayDimensions[dimensionIndex]; i++) {
            if (commaNeeded) {
                WRITE(Comma);
            }

            encodeJsonInternal(((u8*)array) + (type->memSize * *index), type, ctx);
            commaNeeded = UA_TRUE;
            (*index)++;
        }
        WRITE(ArrayEnd);

    } else {
        UA_UInt32 currentDimensionSize = arrayDimensions[dimensionIndex];
        dimensionIndex++;

        commaNeeded = UA_FALSE;
        WRITE(ArrayStart);
        for (size_t i = 0; i < currentDimensionSize; i++) {
            if (commaNeeded) {
                WRITE(Comma);
            }
            addMatrixContentJSON(ctx, array, type, index, arrayDimensions, dimensionIndex, dimensionSize);
            commaNeeded = UA_TRUE;
        }

        WRITE(ArrayEnd);
    }
}

ENCODE_JSON(Variant) {
    /* Quit early for the empty variant */
    u8 encoding = 0;
    status ret = UA_STATUSCODE_GOOD;
    if (!src->type)
        return ENCODE_DIRECT(&encoding, Byte);

    /* Set the content type in the encoding mask */
    const bool isBuiltin = src->type->builtin;
    const bool isAlias = src->type->membersSize == 1
            && UA_TYPES[src->type->members[0].memberTypeIndex].builtin;
    if (isBuiltin)
        encoding |= UA_VARIANT_ENCODINGMASKTYPE_TYPEID_MASK & (u8) (src->type->typeIndex + 1);
    else if (isAlias)
        encoding |= UA_VARIANT_ENCODINGMASKTYPE_TYPEID_MASK & (u8) (src->type->members[0].memberTypeIndex + 1);
    else
        encoding |= UA_VARIANT_ENCODINGMASKTYPE_TYPEID_MASK & (u8) (UA_TYPES_EXTENSIONOBJECT + 1);

    /* Set the array type in the encoding mask */
    const bool isArray = src->arrayLength > 0 || src->data <= UA_EMPTY_ARRAY_SENTINEL;
    const bool hasDimensions = isArray && src->arrayDimensionsSize > 0;
    if (isArray) {
        encoding |= UA_VARIANT_ENCODINGMASKTYPE_ARRAY;
        if (hasDimensions)
            encoding |= UA_VARIANT_ENCODINGMASKTYPE_DIMENSIONS;
    }

    if (useReversibleForm) {
        WRITE(ObjStart);

        /* Encode the encoding byte */
        ret = UA_STATUSCODE_GOOD; //ENCODE_DIRECT(&encoding, Byte);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;

        /* Encode the content */
        if (!isBuiltin && !isAlias)
            ret = Variant_encodeJsonWrapExtensionObject(src, isArray, ctx);
        else if (!isArray) {

            commaNeeded = UA_FALSE;
            writeKey(ctx, "Type");
            ret |= ENCODE_DIRECT(&src->type->typeIndex, UInt16);

            writeKey(ctx, "Body");
            ret = encodeJsonInternal(src->data, src->type, ctx);
        } else {
            //TODO: Variant Array
            commaNeeded = UA_FALSE;
            writeKey(ctx, "Type");
            ret |= ENCODE_DIRECT(&src->type->typeIndex, UInt16);

            writeKey(ctx, "Body");
            ret = Array_encodeJson(src->data, src->arrayLength, src->type, ctx, UA_TRUE);
        }

        /* Encode the array dimensions */
        if (hasDimensions && ret == UA_STATUSCODE_GOOD) {
            //TODO: Variant Dimension
            writeKey(ctx, "Dimension");
            ret = Array_encodeJson(src->arrayDimensions, src->arrayDimensionsSize, &UA_TYPES[UA_TYPES_INT32], ctx, UA_FALSE);
        }


        WRITE(ObjEnd);
        commaNeeded = UA_TRUE;
    } else {

        /* Encode the content */
        if (!isBuiltin && !isAlias)
            ret = Variant_encodeJsonWrapExtensionObject(src, isArray, ctx);
        else if (!isArray) {
            WRITE(ObjStart);
            commaNeeded = UA_FALSE;
            writeKey(ctx, "Body");
            ret = encodeJsonInternal(src->data, src->type, ctx);
            WRITE(ObjEnd);
            commaNeeded = UA_TRUE;
        } else {

            size_t dimensionSize = src->arrayDimensionsSize;
            if (dimensionSize > 1) {
                //nonreversible multidimensional array
                size_t index = 0;
                size_t dimensionIndex = 0;
                void *ptr = src->data;
                const UA_DataType *arraytype = src->type;
                addMatrixContentJSON(ctx, ptr, arraytype, &index, src->arrayDimensions, dimensionIndex, dimensionSize);
            } else {
                //nonreversible simple array
                WRITE(ObjStart);
                commaNeeded = UA_FALSE;
                writeKey(ctx, "Body");
                ret = Array_encodeJson(src->data, src->arrayLength, src->type, ctx, UA_TRUE);
                WRITE(ObjEnd);
                commaNeeded = UA_TRUE;
            }
        }

    }
    return ret;
}

/* DataValue */
ENCODE_JSON(DataValue) {

    WRITE(ObjStart);
    commaNeeded = UA_FALSE;

    status ret = UA_STATUSCODE_GOOD; 

    /* Encode the variant. Afterwards, do not return
     * UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED, as the buffer might have been
     * exchanged during encoding of the variant. */
    if (src->hasValue) {
        writeKey(ctx, "Value");
        ret = ENCODE_DIRECT(&src->value, Variant);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    if (src->hasStatus) {
        writeKey(ctx, "Status");
        ret |= ENCODE_WITHEXCHANGE(&src->status, StatusCode);
    }
    if (src->hasSourceTimestamp) {
        writeKey(ctx, "SourceTimestamp");
        ret |= ENCODE_WITHEXCHANGE(&src->sourceTimestamp, DateTime);
    }
    if (src->hasSourcePicoseconds) {
        writeKey(ctx, "SourcePicoseconds");
        ret |= ENCODE_WITHEXCHANGE(&src->sourcePicoseconds, UInt16);
    }
    if (src->hasServerTimestamp) {
        writeKey(ctx, "ServerTimestamp");
        ret |= ENCODE_WITHEXCHANGE(&src->serverTimestamp, DateTime);
    }
    if (src->hasServerPicoseconds) {
        writeKey(ctx, "ServerPicoseconds");
        ret |= ENCODE_WITHEXCHANGE(&src->serverPicoseconds, UInt16);
    }
    UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);


    WRITE(ObjEnd);
    return ret;
}

/* DiagnosticInfo */
ENCODE_JSON(DiagnosticInfo) {
  
    commaNeeded = UA_FALSE;

    WRITE(ObjStart);

    status ret = UA_STATUSCODE_GOOD; 
    if (src->hasSymbolicId) {
        writeKey(ctx, "SymbolicId");
        ret |= ENCODE_DIRECT(&src->symbolicId, UInt32); /* Int32 */
    }

    if (src->hasNamespaceUri) {
        writeKey(ctx, "NamespaceUri");
        ret |= ENCODE_DIRECT(&src->namespaceUri, UInt32); /* Int32 */
    }

    if (src->hasLocalizedText) {
        writeKey(ctx, "LocalizedText");
        ret |= ENCODE_DIRECT(&src->localizedText, UInt32);
        /* Int32 */
    }
    if (src->hasLocale) {
        writeKey(ctx, "Locale");
        ret |= ENCODE_DIRECT(&src->locale, UInt32);
        /* Int32 */
    }
    if (ret != UA_STATUSCODE_GOOD)
        return ret;

    /* Encode the additional info */
    if (src->hasAdditionalInfo) {
        writeKey(ctx, "AdditionalInfo");
        //WRITE(Quote);
        ret = ENCODE_DIRECT(&src->additionalInfo, String);
        //WRITE(Quote);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    /* Encode the inner status code */
    if (src->hasInnerStatusCode) {
        writeKey(ctx, "InnerStatusCode");
        ret = ENCODE_DIRECT(&src->innerStatusCode, StatusCode);
        UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    /* Encode the inner diagnostic info */
    if (src->hasInnerDiagnosticInfo) {
        writeKey(ctx, "InnerDiagnosticInfo");
        ret = encodeJsonInternal(src->innerDiagnosticInfo, &UA_TYPES[UA_TYPES_DIAGNOSTICINFO], ctx);
    }

    WRITE(ObjEnd);

    UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
    return ret;
}


/********************/
/* Structured Types */
/********************/

const encodeJsonSignature encodeJsonJumpTable[UA_BUILTIN_TYPES_COUNT + 1] = {
    (encodeJsonSignature) Boolean_encodeJson,
    (encodeJsonSignature) SByte_encodeJson, /* SByte */
    (encodeJsonSignature) Byte_encodeJson,
    (encodeJsonSignature) Int16_encodeJson, /* Int16 */
    (encodeJsonSignature) UInt16_encodeJson,
    (encodeJsonSignature) Int32_encodeJson, /* Int32 */
    (encodeJsonSignature) UInt32_encodeJson,
    (encodeJsonSignature) Int64_encodeJson, /* Int64 */
    (encodeJsonSignature) UInt64_encodeJson,
    (encodeJsonSignature) Float_encodeJson,
    (encodeJsonSignature) Double_encodeJson,
    (encodeJsonSignature) String_encodeJson,
    (encodeJsonSignature) DateTime_encodeJson, /* DateTime */
    (encodeJsonSignature) Guid_encodeJson,
    (encodeJsonSignature) ByteString_encodeJson, /* ByteString */
    (encodeJsonSignature) String_encodeJson, /* XmlElement */
    (encodeJsonSignature) NodeId_encodeJson,
    (encodeJsonSignature) ExpandedNodeId_encodeJson,
    (encodeJsonSignature) StatusCode_encodeJson, /* StatusCode */
    (encodeJsonSignature) QualifiedName_encodeJson, /* QualifiedName TODO warum hier encodeJsonInternal?*/
    (encodeJsonSignature) LocalizedText_encodeJson,
    (encodeJsonSignature) ExtensionObject_encodeJson,
    (encodeJsonSignature) DataValue_encodeJson,
    (encodeJsonSignature) Variant_encodeJson,
    (encodeJsonSignature) DiagnosticInfo_encodeJson,
    (encodeJsonSignature) encodeJsonInternal,
};

static status
encodeJsonInternal(const void *src, const UA_DataType *type, Ctx *ctx) {
    /* Check the recursion limit */
    if (ctx->depth > UA_ENCODING_MAX_RECURSION)
        return UA_STATUSCODE_BADENCODINGERROR;
    ctx->depth++;

    if (!type->builtin) {
        WRITE(ObjStart);
    }

    commaNeeded = false;

    uintptr_t ptr = (uintptr_t) src;
    status ret = UA_STATUSCODE_GOOD;
    u8 membersSize = type->membersSize;
    const UA_DataType * typelists[2] = {UA_TYPES, &type[-type->typeIndex]};
    for (size_t i = 0; i < membersSize && ret == UA_STATUSCODE_GOOD; ++i) {
        const UA_DataTypeMember *member = &type->members[i];
        const UA_DataType *membertype = &typelists[!member->namespaceZero][member->memberTypeIndex];

        //TODO: Special case for Strings?
        //TODO: Special case for ByteString
        //TODO Special Case for QualifiedName

        if (member->memberName != NULL && *member->memberName != 0) {
            //TODO:
            writeKey(ctx, member->memberName);

        }


        if (!member->isArray) {
            ptr += member->padding;
            size_t encode_index = membertype->builtin ? membertype->typeIndex : UA_BUILTIN_TYPES_COUNT;
            size_t memSize = membertype->memSize;
            u8 *oldpos = ctx->pos;
            ret = encodeJsonJumpTable[encode_index]((const void*) ptr, membertype, ctx);
            ptr += memSize;
            if (ret == UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED) {
                ctx->pos = oldpos; /* exchange/send the buffer */
                ret = exchangeBuffer(ctx);
                ptr -= member->padding + memSize; /* encode the same member in the next iteration */
                if (ret == UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED || ctx->pos + memSize > ctx->end) {
                    /* the send buffer is too small to encode the member, even after exchangeBuffer */
                    ret = UA_STATUSCODE_BADRESPONSETOOLARGE;
                    break;
                }
                --i;
            }
        } else {

            ptr += member->padding;
            const size_t length = *((const size_t*) ptr);
            ptr += sizeof (size_t);

            //UA_Boolean realArray = UA_FALSE;
            //@TODO Special case for String because of its "bytearray"
            if (type->typeIndex == 11) {
                ENCODE_DIRECT(src, String);
                continue;
            }

            ret = Array_encodeJson(*(void *UA_RESTRICT const *) ptr, length, membertype, ctx, UA_FALSE);
            ptr += sizeof (void*);
        }

        if (member->memberTypeIndex != 0 && membertype->builtin) {
            //WRITE(ObjEnd);
        }

    }

    if (!type->builtin) {
        WRITE(ObjEnd);
    }
    
    commaNeeded = true;

    UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
    ctx->depth--;
    return ret;
}

status
UA_encodeJson(const void *src, const UA_DataType *type,
        u8 **bufPos, const u8 **bufEnd,
        UA_exchangeEncodeBuffer exchangeCallback, void *exchangeHandle) {
    /* Set up the context */
    Ctx ctx;
    ctx.pos = *bufPos;
    ctx.end = *bufEnd;
    ctx.depth = 0;
    ctx.exchangeBufferCallback = exchangeCallback;
    ctx.exchangeBufferCallbackHandle = exchangeHandle;

    /* Encode */
    status ret = encodeJsonInternal(src, type, &ctx);

    /* Set the new buffer position for the output. Beware that the buffer might
     * have been exchanged internally. */
    *bufPos = ctx.pos;
    *bufEnd = ctx.end;
    return ret;
}
/* -----------------------------------------DECODE---------------------------------------------------*/

status UA_atoi(const char input[], size_t size, UA_UInt64 *result);

status UA_atoi(const char input[], size_t size, UA_UInt64 *result){
    if(size < 1){   
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    UA_UInt64 number = 0;
    UA_Boolean neg = (input[0] == '-');
    size_t i = neg ? 1 : 0;
    while ( i < size) {
        if ( input[i] >= '0' && input[i] <= '9' )
        {
          number *= 10;
          number = (number + (UA_UInt64)(input[i] - '0'));
          i++;
        }else{
            return UA_STATUSCODE_BADDECODINGERROR;
        }
    }
    //if (neg){
    //   number *= -1;
    //}
    
    *result = number;
    return UA_STATUSCODE_GOOD;
   }

status UA_atoiSigned(const char input[], size_t size, UA_Int64 *result);

status UA_atoiSigned(const char input[], size_t size, UA_Int64 *result){
    if(size < 1){   
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    UA_Int64 number = 0;
    UA_Boolean neg = (input[0] == '-');
    size_t i = neg ? 1 : 0;
    while ( i < size) {
        if ( input[i] >= '0' && input[i] <= '9' )
        {
          number *= 10;
          number = (number + (input[i] - '0'));
          i++;
        }else{
            return UA_STATUSCODE_BADDECODINGERROR;
        }
    }
    if (neg){
       number *= -1;
    }
    
    *result = number;
    return UA_STATUSCODE_GOOD;
   }


#define DECODE_JSON(TYPE) static status \
    TYPE##_decodeJson(UA_##TYPE *UA_RESTRICT dst, const UA_DataType *type, Ctx *UA_RESTRICT ctx, ParseCtx *parseCtx, UA_Boolean moveToken)

#define DECODE_DIRECT(DST, TYPE) TYPE##_decodeJson((UA_##TYPE*)DST, NULL, ctx, parseCtx, UA_FALSE)

static status 
decodeFields(Ctx *ctx, ParseCtx *parseCtx, u8 memberSize, const char* fieldNames[], decodeJsonSignature functions[], void *fieldPointer[], const UA_DataType *type, UA_Boolean found[]);


static status
decodeJsonInternal(void *dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken);


static status
Array_decodeJson(void *dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken);


static jsmntype_t getJsmnType(const ParseCtx *parseCtx){
    return parseCtx->tokenArray[*parseCtx->index].type;
}

static UA_Boolean isJsonNull(const Ctx *ctx, const ParseCtx *parseCtx){
    if(parseCtx->tokenArray[*parseCtx->index].type != JSMN_PRIMITIVE){
        return false;
    }
    char* elem = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    return (elem[0] == 'n' && elem[0] == 'u' && elem[0] == 'l' && elem[0] == 'l');
}


static int equalCount = 0;
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {

    equalCount++;
    if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, (size_t)(tok->end - tok->start)) == 0) {
        return 0;
    }
    return -1;
}

DECODE_JSON(Boolean) {
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    
    UA_Boolean d = UA_TRUE;
    if(size == 4){
        d = UA_TRUE;
    }else if(size == 5){
        d = UA_FALSE;
    }else{
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    memcpy(dst, &d, 1);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(Byte) {
    UA_UInt64 d = 0;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoi(data, size, &d);
    
    memcpy(dst, &d, 1);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(SByte) {
    UA_Int64 d = 0;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoiSigned(data, size, &d);
    
    memcpy(dst, &d, 1);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(UInt16) {
    UA_UInt64 d = 0;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoi(data, size, &d);
    
    memcpy(dst, &d, 2);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(UInt32) {
    UA_UInt64 d = 0;
   
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoi(data, size, &d);
    
    memcpy(dst, &d, 4);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(UInt64) {
    UA_UInt64 d = 0;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoi(data, size, &d);
    
    memcpy(dst, &d, 8);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(Int16) {
    UA_Int64 d = 0;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoiSigned(data, size, &d);
    
    memcpy(dst, &d, 2);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(Int32) {
    UA_Int64 d = 0;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoiSigned(data, size, &d);
    
    memcpy(dst, &d, 4);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(Int64) {
    UA_Int64 d = 0;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    UA_atoiSigned(data, size, &d);
    
    memcpy(dst, &d, 8);
    
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}
UA_UInt32 hex2int(char c);
UA_UInt32 hex2int(char ch)
{
    if (ch >= '0' && ch <= '9')
        return (UA_UInt32)(ch - '0');
    if (ch >= 'A' && ch <= 'F')
        return (UA_UInt32)(ch - 'A' + 10);
    if (ch >= 'a' && ch <= 'f')
        return (UA_UInt32)(ch - 'a' + 10);
    return 0;
}

DECODE_JSON(Guid) {
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_STRING && tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    if(size != 36){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    char *buf = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    
    dst->data1 |= (UA_Byte)(hex2int(buf[0]) << 28);
    dst->data1 |= (UA_Byte)(hex2int(buf[1]) << 24);
    dst->data1 |= (UA_Byte)(hex2int(buf[2]) << 20);
    dst->data1 |= (UA_Byte)(hex2int(buf[3]) << 16);
    dst->data1 |= (UA_Byte)(hex2int(buf[4]) << 12);
    dst->data1 |= (UA_Byte)(hex2int(buf[5]) << 8);
    dst->data1 |= (UA_Byte)(hex2int(buf[6]) << 4);
    dst->data1 |= (UA_Byte)(hex2int(buf[7]) << 0);
    // index 8 should be '-'
    dst->data2 |= (UA_UInt16)(hex2int(buf[9]) << 12);
    dst->data2 |= (UA_UInt16)(hex2int(buf[10]) << 8);
    dst->data2 |= (UA_UInt16)(hex2int(buf[11]) << 4);
    dst->data2 |= (UA_UInt16)(hex2int(buf[12]) << 0);
    // index 13 should be '-'
    dst->data3 |= (UA_UInt16)(hex2int(buf[14]) << 12);
    dst->data3 |= (UA_UInt16)(hex2int(buf[15]) << 8);
    dst->data3 |= (UA_UInt16)(hex2int(buf[16]) << 4);
    dst->data3 |= (UA_UInt16)(hex2int(buf[17]) << 0);
    // index 18 should be '-'
    dst->data4[0] |= (UA_Byte)(hex2int(buf[19]) << 4);
    dst->data4[0] |= (UA_Byte)(hex2int(buf[20]) << 0);
    dst->data4[1] |= (UA_Byte)(hex2int(buf[21]) << 4);
    dst->data4[1] |= (UA_Byte)(hex2int(buf[22]) << 0);
    // index 23 should be '-'
    dst->data4[2] |= (UA_Byte)(hex2int(buf[24]) << 4);
    dst->data4[2] |= (UA_Byte)(hex2int(buf[25]) << 0);
    dst->data4[3] |= (UA_Byte)(hex2int(buf[26]) << 4);
    dst->data4[3] |= (UA_Byte)(hex2int(buf[27]) << 0);
    dst->data4[4] |= (UA_Byte)(hex2int(buf[28]) << 4);
    dst->data4[4] |= (UA_Byte)(hex2int(buf[29]) << 0);
    dst->data4[5] |= (UA_Byte)(hex2int(buf[30]) << 4);
    dst->data4[5] |= (UA_Byte)(hex2int(buf[31]) << 0);
    dst->data4[6] |= (UA_Byte)(hex2int(buf[32]) << 4);
    dst->data4[6] |= (UA_Byte)(hex2int(buf[33]) << 0);
    dst->data4[7] |= (UA_Byte)(hex2int(buf[34]) << 4);
    dst->data4[7] |= (UA_Byte)(hex2int(buf[35]) << 0);
  
    if(moveToken)
        (*parseCtx->index)++; // is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(String) {
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_STRING && tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    dst->data = ctx->pos + parseCtx->tokenArray[*parseCtx->index].start;
    dst->length = size;

    if(moveToken)
        (*parseCtx->index)++; // String is one element

    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(ByteString) {
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_STRING && tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    int size = (parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    const char* input = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    //dst->length = size;

    //estimate size
    UA_UInt32 outputsize = (UA_UInt32)(size * 3 / 4);
    
    /* set up a destination buffer large enough to hold the encoded data */
    char* output = (char*)malloc(outputsize);
    if(!output){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    /* keep track of our decoded position */
    char* c = output;
    /* store the number of bytes decoded by a single call */
    int cnt = 0;
    /* we need a decoder state */
    base64_decodestate s;

    /*---------- START DECODING ----------*/
    /* initialise the decoder state */
    base64_init_decodestate(&s);
    /* decode the input data */
    cnt = base64_decode_block(input, size, c, &s);
    c += cnt;
    /* note: there is no base64_decode_blockend! */
    /*---------- STOP DECODING  ----------*/
    
    UA_UInt64 actualLength = (UA_UInt64)(c - output);
    
    char* dstData = (char*)malloc(actualLength);
    memcpy(dstData, output, actualLength);
    dst->data = (u8*)dstData;
    dst->length = actualLength;

    free(output);
    
    if(moveToken)
        (*parseCtx->index)++; // String is one element

    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(LocalizedText) {
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    const char* fieldNames[] = {"Locale", "Text"};
    void *fieldPointer[] = {&dst->locale, &dst->text};
    decodeJsonSignature functions[] = {(decodeJsonSignature) String_decodeJson, (decodeJsonSignature) String_decodeJson};
    
    decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, type, NULL);
    
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(QualifiedName) {
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    const char* fieldNames[] = {"Name", "Uri"};
    void *fieldPointer[] = {&dst->name, &dst->namespaceIndex};
    decodeJsonSignature functions[] = {(decodeJsonSignature) String_decodeJson, (decodeJsonSignature) UInt16_decodeJson};
    
    decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, type, NULL);
    
    return UA_STATUSCODE_GOOD;
}

status lookAheadForKey(UA_String search, Ctx *ctx, ParseCtx *parseCtx, size_t *resultIndex);
status searchObjectForKeyRec(char* s, Ctx *ctx, ParseCtx *parseCtx, size_t *resultIndex, UA_UInt16 depth);

status searchObjectForKeyRec(char* s, Ctx *ctx, ParseCtx *parseCtx, size_t *resultIndex, UA_UInt16 depth){
    
    UA_StatusCode ret = UA_STATUSCODE_BADDECODINGERROR;
    
    if(parseCtx->tokenArray[(*parseCtx->index)].type == JSMN_OBJECT){
        size_t objectCount = (size_t)(parseCtx->tokenArray[(*parseCtx->index)].size);
        
        (*parseCtx->index)++; //Object to first Key
        size_t i;
        for (i = 0; i < objectCount; i++) {
            if(depth == 0){ // we search only on first layer
                if (jsoneq((char*)ctx->pos, &parseCtx->tokenArray[*parseCtx->index], s) == 0) {
                    //found
                    (*parseCtx->index)++; //We give back a pointer to the value of the searched key!
                    *resultIndex = *parseCtx->index;
                    ret = UA_STATUSCODE_GOOD;
                    break;
                }
            }
               
            (*parseCtx->index)++; //value
            if(parseCtx->tokenArray[(*parseCtx->index)].type == JSMN_OBJECT){
               searchObjectForKeyRec( s, ctx, parseCtx, resultIndex, (UA_UInt16)(depth + 1));
            }else if(parseCtx->tokenArray[(*parseCtx->index)].type == JSMN_ARRAY){
               searchObjectForKeyRec( s, ctx, parseCtx, resultIndex, (UA_UInt16)(depth + 1));
            }else{
                //Only Primitive or string
                (*parseCtx->index)++;
            }
            
        }
    }else if(parseCtx->tokenArray[(*parseCtx->index)].type == JSMN_ARRAY){
        size_t arraySize = (size_t)(parseCtx->tokenArray[(*parseCtx->index)].size);
        
        (*parseCtx->index)++; //Object to first element
        size_t i;
        for (i = 0; i < arraySize; i++) {
            if(parseCtx->tokenArray[(*parseCtx->index)].type == JSMN_OBJECT){
               searchObjectForKeyRec( s, ctx, parseCtx, resultIndex, (UA_UInt16)(depth + 1));
            }else if(parseCtx->tokenArray[(*parseCtx->index)].type == JSMN_ARRAY){
               searchObjectForKeyRec( s, ctx, parseCtx, resultIndex, (UA_UInt16)(depth + 1));
            }else{
                //Only Primitive or string
                (*parseCtx->index)++;
            }
        }
        
    }
    return ret;
}

status lookAheadForKey(UA_String search, Ctx *ctx, ParseCtx *parseCtx, size_t *resultIndex){
    //DEBUG: (char*)(&ctx->pos[parseCtx->tokenArray[*parseCtx->index].start])
    
    //save index for later restore
    UA_UInt16 oldIndex = *parseCtx->index;
    
    char s[search.length + 1];
    memcpy(&s, search.data, search.length);
    s[search.length] = '\0';
    
    UA_UInt16 depth = 0;
    searchObjectForKeyRec( s, ctx, parseCtx, resultIndex, depth);
    //Restore index
    *parseCtx->index = oldIndex;
    
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(NodeId) {
    
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    UA_Boolean hasNamespace = UA_FALSE;
    size_t searchResultNamespace = 0;
    UA_String searchKeyNamespace = UA_STRING("Namespace");
            
    lookAheadForKey(searchKeyNamespace, ctx, parseCtx, &searchResultNamespace);
    
    if(searchResultNamespace == 0){
        dst->namespaceIndex = 0;
    } else{
        hasNamespace = UA_TRUE;
    }
    
    size_t searchResult = 0;
    UA_String searchKey = UA_STRING("IdType");
    //status searchStatus = 
            
    lookAheadForKey(searchKey, ctx, parseCtx, &searchResult);

    //If non found the type is UINT
    //if(searchStatus != UA_STATUSCODE_GOOD){
    //    return searchStatus;
    //}
    
    UA_String dummy;
    if(searchResult != 0){
        
        size_t size = (size_t)(parseCtx->tokenArray[searchResult].end - parseCtx->tokenArray[searchResult].start);
        if(size < 1){
            return UA_STATUSCODE_BADDECODINGERROR;
        }

        char *idType = (char*)(ctx->pos + parseCtx->tokenArray[searchResult].start);

        const char* fieldNames[] = {"Id", "IdType", "Namespace"};
        u8 fieldCount = sizeof(fieldNames)/ sizeof(fieldNames[0]);
        if(!hasNamespace){
            //If no namespace, fieldcount to 2
            fieldCount--;
        }
        
        void *fieldPointer[] = {NULL, &dummy, &dst->namespaceIndex};
        decodeJsonSignature functions[] = {NULL, (decodeJsonSignature) String_decodeJson, (decodeJsonSignature) UInt16_decodeJson};
        if(idType[0] == '2'){
            dst->identifierType = UA_NODEIDTYPE_GUID;
            fieldPointer[0] = &dst->identifier.guid;
            functions[0] = (decodeJsonSignature) Guid_decodeJson;
        }else if(idType[0] == '1'){
            dst->identifierType = UA_NODEIDTYPE_STRING;
            fieldPointer[0] = &dst->identifier.string;
            functions[0] = (decodeJsonSignature) String_decodeJson;
        }else if(idType[0] == '3'){
            dst->identifierType = UA_NODEIDTYPE_BYTESTRING;
            fieldPointer[0] = &dst->identifier.byteString;
            functions[0] = (decodeJsonSignature) ByteString_decodeJson;
        }else{
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        return decodeFields(ctx, parseCtx, fieldCount, fieldNames, functions, fieldPointer, type, NULL);
    }else{
        const char* fieldNames[] = {"Id", "Namespace"};
        u8 fieldCount = sizeof(fieldNames)/ sizeof(fieldNames[0]);
        if(!hasNamespace){
            //If no namespace, fieldcount to 1
            fieldCount--;
        }
        
        //No IdType give, Id is encoded as Number
        dst->identifierType = UA_NODEIDTYPE_NUMERIC;
        void *fieldPointer[] = {&dst->identifier.numeric, &dst->namespaceIndex};
        decodeJsonSignature functions[] = {(decodeJsonSignature) UInt32_decodeJson, (decodeJsonSignature) UInt16_decodeJson};
        return decodeFields(ctx, parseCtx, fieldCount, fieldNames, functions, fieldPointer, type, NULL);
    }
}



DECODE_JSON(ExpandedNodeId) {
    
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    UA_Boolean hasNamespace = UA_FALSE;
    size_t searchResultNamespace = 0;
    UA_String searchKeyNamespace = UA_STRING("Namespace");
            
    lookAheadForKey(searchKeyNamespace, ctx, parseCtx, &searchResultNamespace);
    
    if(searchResultNamespace == 0){
        dst->serverIndex = 0;
    } else{
        hasNamespace = UA_TRUE;
    }
    
    UA_Boolean hasServerUri = UA_FALSE;
    size_t searchResultServerUri = 0;
    UA_String searchKeyServerUri = UA_STRING("ServerUri");
            
    lookAheadForKey(searchKeyServerUri, ctx, parseCtx, &searchResultServerUri);
    
    if(searchResultNamespace == 0){
        //TODO: Lookup
        dst->namespaceUri = UA_STRING(""); 
    } else{
        hasServerUri = UA_TRUE;
    }
    
    UA_Boolean hasIdType = UA_FALSE;
    size_t searchResult = 0;
    UA_String searchKey = UA_STRING("IdType");
    //status searchStatus = 
            
    lookAheadForKey(searchKey, ctx, parseCtx, &searchResult);
    if(searchResult != 0){
         hasIdType = UA_TRUE;
    }
    
    
    /* Keep track over number of keys present, incremented if key found */
    u8 fieldCount = 0;
    
    /* possible keys */
    char* idString = "Id";
    char* namespaceString = "Namespace";
    char* serverUriString = "ServerUri";
    char* idTypeString = "IdType";
    
    /* Setup fields, max 4 keys */
    const char* fieldNames[4];
    decodeJsonSignature functions[4];
    void *fieldPointer[4];
    
    
    /* Id must alway be present */
    fieldNames[fieldCount] = idString;
    
    
    if(hasIdType){
        
        size_t size = (size_t)(parseCtx->tokenArray[searchResult].end - parseCtx->tokenArray[searchResult].start);
        if(size < 1){
            return UA_STATUSCODE_BADDECODINGERROR;
        }

        char *idType = (char*)(ctx->pos + parseCtx->tokenArray[searchResult].start);
      
        if(idType[0] == '2'){
            dst->nodeId.identifierType = UA_NODEIDTYPE_GUID;
            fieldPointer[fieldCount] = &dst->nodeId.identifier.guid;
            functions[fieldCount] = (decodeJsonSignature) Guid_decodeJson;
        }else if(idType[0] == '1'){
            dst->nodeId.identifierType = UA_NODEIDTYPE_STRING;
            fieldPointer[fieldCount] = &dst->nodeId.identifier.string;
            functions[fieldCount] = (decodeJsonSignature) String_decodeJson;
        }else if(idType[0] == '3'){
            dst->nodeId.identifierType = UA_NODEIDTYPE_BYTESTRING;
            fieldPointer[fieldCount] = &dst->nodeId.identifier.byteString;
            functions[fieldCount] = (decodeJsonSignature) ByteString_decodeJson;
        }else{
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        //Id alway present
        fieldCount++;
        
        //IdType is parsed again so its token is stepped over. TODO other solution? 
        UA_String dummy;
        fieldNames[fieldCount] = idTypeString;
        fieldPointer[fieldCount] = &dummy;
        functions[fieldCount] = (decodeJsonSignature) String_decodeJson;
        
        //IdType
        fieldCount++;
    }else{
        dst->nodeId.identifierType = UA_NODEIDTYPE_NUMERIC;
        fieldPointer[fieldCount] = &dst->nodeId.identifier.numeric;
        functions[fieldCount] = (decodeJsonSignature) UInt32_decodeJson;
        
        //Id alway present
        fieldCount++;    
    }
    
    if(hasNamespace){
        fieldNames[fieldCount] = namespaceString;
        
        //TODO: which one is namespace?
        //fieldPointer[fieldCount] = &dst->nodeId.namespaceIndex;
        fieldPointer[fieldCount] = &dst->serverIndex;
        functions[fieldCount] = (decodeJsonSignature) UInt16_decodeJson;
        fieldCount++;  
    }
    
    UA_UInt32 namespaceUriIndex;
    if(hasServerUri){
        fieldNames[fieldCount] = serverUriString;
        fieldPointer[fieldCount] = &namespaceUriIndex;
        functions[fieldCount] = (decodeJsonSignature) UInt32_decodeJson;
        fieldCount++;  
    }
    
    status ret = decodeFields(ctx, parseCtx, fieldCount, fieldNames, functions, fieldPointer, type, NULL);
    
    if(hasServerUri){
        //User namespaceUriIndex to lookup namespaceUri
        dst->namespaceUri = UA_STRING("@");
    }
    
    return ret;
}

DECODE_JSON(DateTime) {
    if(getJsmnType(parseCtx) != JSMN_STRING){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    const char *input = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    //DateTime  ISO 8601:2004 is 20 Characters
    if(size != 20){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    struct mytm dts;
    memset(&dts, 0, sizeof(dts));
    
    UA_UInt64 year;
    UA_atoi(&input[0], 4, &year);
    dts.tm_year = (UA_UInt16)year - 1900;
    UA_UInt64 month;
    UA_atoi(&input[5], 2, &month);
    dts.tm_mon = (UA_UInt16)month - 1;
    UA_UInt64 day;
    UA_atoi(&input[8], 2, &day);
    dts.tm_mday = (UA_UInt16)day;
    UA_UInt64 hour;
    UA_atoi(&input[11], 2, &hour);
    dts.tm_hour = (UA_UInt16)hour;
    UA_UInt64 min;
    UA_atoi(&input[14], 2, &min);
    dts.tm_min = (UA_UInt16)min;
    UA_UInt64 sec;
    UA_atoi(&input[17], 2, &sec);
    dts.tm_sec = (UA_UInt16)sec;
    
    long long sinceunix = __tm_to_secs(&dts);
    UA_DateTime dt = (sinceunix*UA_DATETIME_SEC + UA_DATETIME_UNIX_EPOCH);
    memcpy(dst, &dt, 8);
  
    if(moveToken)
        (*parseCtx->index)++; // DateTime is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(StatusCode) {
    //TODO: Switch to UInt32?
    UA_UInt32 d;
    DECODE_DIRECT(&d, UInt32);
    memcpy(dst, &d, 4);

    if(moveToken)
        (*parseCtx->index)++;
    return UA_STATUSCODE_GOOD;
}

static status
VariantDimension_decodeJson(void *UA_RESTRICT dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken) {
    const UA_DataType *dimType = &UA_TYPES[UA_TYPES_UINT32];
    return Array_decodeJson(dst, dimType, ctx, parseCtx, moveToken);
}

DECODE_JSON(Variant) {
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    size_t searchResultType = 0;
    UA_String searchKeyType = UA_STRING("Type");
 
    lookAheadForKey(searchKeyType, ctx, parseCtx, &searchResultType);
    
    //If non found we cannot decode
    //if(searchStatus != UA_STATUSCODE_GOOD){
    //    return searchStatus;
    //}
    
    //TODO: Better way of not found condition.
    if(searchResultType != 0){  
        size_t size = (size_t)(parseCtx->tokenArray[searchResultType].end - parseCtx->tokenArray[searchResultType].start);
        if(size < 1){
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        /* Does the variant contain an array? */
        UA_Boolean isArray = UA_FALSE;
        size_t arraySize = 0;
        
        UA_Boolean hasDimension = UA_FALSE;
        size_t dimensionSize = 0;
        
        //Is the Body an Array?
        size_t searchResultBody = 0;
        UA_String searchKeyBody = UA_STRING("Body");
        lookAheadForKey(searchKeyBody, ctx, parseCtx, &searchResultBody);
        if(searchResultBody != 0){
            jsmntok_t bodyToken = parseCtx->tokenArray[searchResultBody];
            if(bodyToken.type == JSMN_ARRAY){
                isArray = UA_TRUE;
                
                arraySize = (size_t)parseCtx->tokenArray[searchResultBody].size;
                dst->arrayLength = arraySize;
            }
        }
        
        //Has the variant dimension?
        size_t searchResultDim = 0;
        UA_String searchKeyDim = UA_STRING("Dimension");
        lookAheadForKey(searchKeyDim, ctx, parseCtx, &searchResultDim);
        if(searchResultDim != 0){
            hasDimension = UA_TRUE;
            dimensionSize = (size_t)parseCtx->tokenArray[searchResultDim].size;
            
            dst->arrayDimensionsSize = dimensionSize;
        }

        //Parse the type
        UA_UInt64 idTypeDecoded;
        char *idTypeEncoded = (char*)(ctx->pos + parseCtx->tokenArray[searchResultType].start);
        UA_atoi(idTypeEncoded, size, &idTypeDecoded);
        
        //Set the type
        const UA_DataType *BodyType = &UA_TYPES[idTypeDecoded];
        dst->type = BodyType;
        
        if(!isArray){
            
            //Allocate Memory for Body
            void* bodyPointer = UA_new(BodyType);
            memcpy(&dst->data, &bodyPointer, sizeof(void*)); //Copy new Pointer do dest
            
            const char* fieldNames[] = {"Type", "Body"};
            UA_String dummy;
            void *fieldPointer[] = {&dummy, bodyPointer};
            decodeJsonSignature functions[] = {(decodeJsonSignature) String_decodeJson, (decodeJsonSignature) decodeJsonInternal};
            UA_Boolean found[] = {UA_FALSE, UA_FALSE};

            decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, BodyType, found);
            
        }else{
            
            if(!hasDimension){
                const char* fieldNames[] = {"Type", "Body"};

                UA_String dummy;
                //if(idType[0] == '2'){
                void *fieldPointer[] = {&dummy, &dst->data};
                decodeJsonSignature functions[] = {(decodeJsonSignature) String_decodeJson, (decodeJsonSignature) Array_decodeJson};
                UA_Boolean found[] = {UA_FALSE, UA_FALSE};
                decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, BodyType, found);
            }else{
                const char* fieldNames[] = {"Type", "Body", "Dimension"};
                UA_String dummy;
                void *fieldPointer[] = {&dummy, &dst->data, &dst->arrayDimensions};
                decodeJsonSignature functions[] = {(decodeJsonSignature) String_decodeJson, (decodeJsonSignature) Array_decodeJson, (decodeJsonSignature) VariantDimension_decodeJson};
                UA_Boolean found[] = {UA_FALSE, UA_FALSE, UA_FALSE};

                decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, BodyType, found);
            }
        }
    }
    
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(DataValue) {
    const char* fieldNames[] = {"Value", "Status", "SourceTimestamp", "SourcePicoseconds", "ServerTimestamp", "ServerPicoseconds"};
    
    void *fieldPointer[] = {
        &dst->value, 
        &dst->status, 
        &dst->sourceTimestamp, 
        &dst->sourcePicoseconds, 
        &dst->serverTimestamp, 
        &dst->serverPicoseconds};
    
    decodeJsonSignature functions[] = {
        (decodeJsonSignature) Variant_decodeJson, 
        (decodeJsonSignature) StatusCode_decodeJson,
        (decodeJsonSignature) DateTime_decodeJson,
        (decodeJsonSignature) UInt16_decodeJson,
        (decodeJsonSignature) DateTime_decodeJson,
        (decodeJsonSignature) UInt16_decodeJson};
    
    UA_Boolean found[] = {UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE};
    decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, type, found);
    dst->hasValue = found[0];
    dst->hasStatus = found[1];
    dst->hasSourceTimestamp = found[2];
    dst->hasSourcePicoseconds = found[3];
    dst->hasServerTimestamp = found[4];
    dst->hasServerPicoseconds = found[5];
    
    return UA_STATUSCODE_GOOD;
}

/* The binary encoding has a different nodeid from the data type. So it is not
 * possible to reuse UA_findDataType */
static const UA_DataType *
UA_findDataTypeByBinaryInternal(const UA_NodeId *typeId, Ctx *ctx) {
    /* We only store a numeric identifier for the encoding nodeid of data types */
    if(typeId->identifierType != UA_NODEIDTYPE_NUMERIC)
        return NULL;

    /* Always look in built-in types first
     * (may contain data types from all namespaces) */
    for(size_t i = 0; i < UA_TYPES_COUNT; ++i) {
        if(UA_TYPES[i].binaryEncodingId == typeId->identifier.numeric &&
           UA_TYPES[i].typeId.namespaceIndex == typeId->namespaceIndex)
            return &UA_TYPES[i];
    }

    /* When other namespace look in custom types, too */
    if(typeId->namespaceIndex != 0) {
        for(size_t i = 0; i < ctx->customTypesArraySize; ++i) {
            if(ctx->customTypesArray[i].binaryEncodingId == typeId->identifier.numeric &&
               ctx->customTypesArray[i].typeId.namespaceIndex == typeId->namespaceIndex)
                return &ctx->customTypesArray[i];
        }
    }

    return NULL;
}

DECODE_JSON(ExtensionObject) {
    if(isJsonNull(ctx, parseCtx)){
        /* 
        * TODO:
        * If the Body is empty, the ExtensionObject is NULL and is omitted or encoded as a JSON null.
        */   
        return UA_STATUSCODE_BADNOTIMPLEMENTED;
    }
    
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    //Search for Encoding
    size_t searchEncodingResult = 0;
    UA_String searchEncodingKey = UA_STRING("Encoding");
    lookAheadForKey(searchEncodingKey, ctx, parseCtx, &searchEncodingResult);
    
    //If no encoding found it is Structure encoding
    if(searchEncodingResult == 0){
        
        dst->encoding = UA_EXTENSIONOBJECT_DECODED;
        
        UA_NodeId typeId;
        UA_NodeId_init(&typeId);

        size_t searchTypeIdResult = 0;
        UA_String searchTypeIdKey = UA_STRING("TypeId");
        lookAheadForKey(searchTypeIdKey, ctx, parseCtx, &searchTypeIdResult);  
        
        /* parse the nodeid */
        //for restore
        UA_UInt16 index = *parseCtx->index;
        *parseCtx->index = (UA_UInt16)searchTypeIdResult;
        NodeId_decodeJson(&typeId, &UA_TYPES[UA_TYPES_NODEID], ctx, parseCtx, UA_TRUE);
        //restore
        *parseCtx->index = index;
        const UA_DataType *typeOfBody = UA_findDataTypeByBinaryInternal(&typeId, ctx);
        if(!typeOfBody){
            return UA_STATUSCODE_BADNOTIMPLEMENTED;
        }
        
        //Set Found Type
        dst->content.decoded.type = typeOfBody;
        
        
        if( searchTypeIdResult != 0){
            
            const char* fieldNames[] = {"TypeId", "Body"};

            dst->content.decoded.data = UA_new(type);
            if(!dst->content.decoded.data)
                return UA_STATUSCODE_BADOUTOFMEMORY;
            
            UA_NodeId typeId_dummy;
            void *fieldPointer[] = {
                &typeId_dummy, 
                dst->content.decoded.data
            };

            size_t decode_index = typeOfBody->builtin ? typeOfBody->typeIndex : UA_BUILTIN_TYPES_COUNT;
            decodeJsonSignature functions[] = {
                (decodeJsonSignature) NodeId_decodeJson, 
                decodeJsonJumpTable[decode_index]};

            return decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, typeOfBody, NULL);
        }else{
           return UA_STATUSCODE_BADDECODINGERROR;
        }
    }else{
        //Parse the encoding
        UA_UInt64 encoding = 0;
        char *extObjEncoding = (char*)(ctx->pos + parseCtx->tokenArray[searchEncodingResult].start);
        size_t size = (size_t)(parseCtx->tokenArray[searchEncodingResult].end - parseCtx->tokenArray[searchEncodingResult].start);
        UA_atoi(extObjEncoding, size, &encoding);

 
        if(encoding == 1) {
            /* BYTESTRING in Json Body */
            dst->encoding = UA_EXTENSIONOBJECT_ENCODED_BYTESTRING;
            
            const char* fieldNames[] = {"Encoding", "Body", "TypeId"};
            
            UA_UInt16 encodingTypeJson;
            void *fieldPointer[] = {
                &encodingTypeJson, 
                &dst->content.encoded.body, 
                &dst->content.encoded.typeId
            };

            decodeJsonSignature functions[] = {
                (decodeJsonSignature) UInt16_decodeJson,
                (decodeJsonSignature) ByteString_decodeJson, 
                (decodeJsonSignature) NodeId_decodeJson};
            
            return decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, type, NULL);
        } else if(encoding == 2) {
            /* XmlElement in Json Body */
            dst->encoding = UA_EXTENSIONOBJECT_ENCODED_XML;
            
            const char* fieldNames[] = {"Encoding", "Body", "TypeId"};
            
            UA_UInt16 encodingTypeJson;
            void *fieldPointer[] = {
                &encodingTypeJson, 
                &dst->content.encoded.body, 
                &dst->content.encoded.typeId
            };

            decodeJsonSignature functions[] = {
                (decodeJsonSignature) UInt16_decodeJson,
                (decodeJsonSignature) String_decodeJson, 
                (decodeJsonSignature) NodeId_decodeJson};
            
            return decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, type, NULL);
        } else {
            //UA_NodeId_deleteMembers(&typeId);
            return UA_STATUSCODE_BADDECODINGERROR;
        }
    }
    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

status DiagnosticInfoInner_decodeJson(UA_DiagnosticInfo* dst, const UA_DataType* type, Ctx* ctx, ParseCtx* parseCtx);

DECODE_JSON(DiagnosticInfo) {
    
    const char* fieldNames[] = {"SymbolicId", "LocalizedText", "Locale", "AdditionalInfo", "InnerStatusCode", "InnerDiagnosticInfo"};
    
    //Can we check if a inner diag info is needed? Here we allocate a inner Diag undo preeemptive
    //UA_DiagnosticInfo *inner = (UA_DiagnosticInfo*)UA_calloc(1, sizeof(UA_DiagnosticInfo));
    
    void *fieldPointer[] = {&dst->symbolicId, &dst->localizedText, &dst->locale, &dst->additionalInfo, &dst->innerStatusCode, &dst->innerDiagnosticInfo};
    decodeJsonSignature functions[] = {(decodeJsonSignature) Int32_decodeJson, (decodeJsonSignature) Int32_decodeJson,(decodeJsonSignature) Int32_decodeJson,(decodeJsonSignature) String_decodeJson,(decodeJsonSignature) StatusCode_decodeJson, (decodeJsonSignature) DiagnosticInfoInner_decodeJson};
    UA_Boolean found[] = {UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE};
    decodeFields(ctx, parseCtx, sizeof(fieldNames)/ sizeof(fieldNames[0]), fieldNames, functions, fieldPointer, type, found);
    dst->hasSymbolicId = found[0];
    dst->hasLocalizedText = found[1];
    dst->hasLocale = found[2];
    dst->hasAdditionalInfo = found[3];
    dst->hasInnerStatusCode = found[4];
    dst->hasInnerDiagnosticInfo = found[5];
    
    /*if(dst->hasInnerDiagnosticInfo){
        dst->innerDiagnosticInfo = inner;
    }else{
        free(inner); //Free inner if not needed...
    }*/

    return UA_STATUSCODE_GOOD;
}

status DiagnosticInfoInner_decodeJson(UA_DiagnosticInfo* dst, const UA_DataType* type, Ctx* ctx, ParseCtx* parseCtx){
    UA_DiagnosticInfo *inner = (UA_DiagnosticInfo*)UA_calloc(1, sizeof(UA_DiagnosticInfo));
    memcpy(dst, &inner, sizeof(UA_DiagnosticInfo*)); //Copy new Pointer do dest
    return DiagnosticInfo_decodeJson(inner, type, ctx, parseCtx, UA_TRUE);
}

static status 
decodeFields(Ctx *ctx, ParseCtx *parseCtx, u8 memberSize, const char* fieldNames[], decodeJsonSignature functions[], void *fieldPointer[], const UA_DataType *type, UA_Boolean found[]) {
    size_t objectCount = (size_t)(parseCtx->tokenArray[(*parseCtx->index)].size);
    
    if(memberSize == 1){ // TODO: Experimental, is this assumption correct?
        if(*fieldNames[0] == 0){ //No MemberName
            return functions[0](fieldPointer[0], type, ctx, parseCtx, UA_TRUE); //ENCODE DIRECT
        }
    }else if(memberSize == 0){
        return UA_STATUSCODE_BADENCODINGERROR;
    }
    
    (*parseCtx->index)++; //go to first key

    size_t foundCount = 0;
    size_t currentObjectCout = 0;
    while (currentObjectCout < objectCount && *parseCtx->index < parseCtx->tokenCount) {

        size_t i;//TODO: consider to jump over already searched tokens
        for (i = 0; i < memberSize; i++) { //Search for KEY, if found outer loop will be one less. Best case is objectCount if in order!
            if (jsoneq((char*)ctx->pos, &parseCtx->tokenArray[*parseCtx->index], fieldNames[i]) == 0) {
                if(found != NULL){
                    found[i] = UA_TRUE;
                }
                
                (*parseCtx->index)++; //goto value
                //type->
                functions[i](fieldPointer[i], type, ctx, parseCtx, UA_TRUE);//Move Token True
                currentObjectCout++;
            }
        }

        if (currentObjectCout == foundCount) {
            //printf("Search failed %d fields of %d found.\n", found, (int)objectCount);
            break; //nothing found
        }
        foundCount = currentObjectCout;
       
    }

    return UA_STATUSCODE_GOOD;
}


const decodeJsonSignature decodeJsonJumpTable[UA_BUILTIN_TYPES_COUNT + 1] = {
    (decodeJsonSignature)Boolean_decodeJson,
    (decodeJsonSignature)SByte_decodeJson, /* SByte */
    (decodeJsonSignature)Byte_decodeJson,
    (decodeJsonSignature)Int16_decodeJson, /* Int16 */
    (decodeJsonSignature)UInt16_decodeJson,
    (decodeJsonSignature)Int32_decodeJson, /* Int32 */
    (decodeJsonSignature)UInt32_decodeJson,
    (decodeJsonSignature)Int64_decodeJson, /* Int64 */
    (decodeJsonSignature)UInt64_decodeJson,
    (decodeJsonSignature)NULL,//DFloat_decodeBinary,
    (decodeJsonSignature)NULL,//DDouble_decodeBinary,
    (decodeJsonSignature)String_decodeJson,
    (decodeJsonSignature)DateTime_decodeJson, /* DateTime */
    (decodeJsonSignature)Guid_decodeJson,
    (decodeJsonSignature)ByteString_decodeJson, /* ByteString */
    (decodeJsonSignature)String_decodeJson, /* XmlElement */
    (decodeJsonSignature)NodeId_decodeJson,
    (decodeJsonSignature)ExpandedNodeId_decodeJson,
    (decodeJsonSignature)StatusCode_decodeJson, /* StatusCode */
    (decodeJsonSignature)QualifiedName_decodeJson, /* QualifiedName */
    (decodeJsonSignature)LocalizedText_decodeJson,
    (decodeJsonSignature)ExtensionObject_decodeJson,
    (decodeJsonSignature)DataValue_decodeJson,
    (decodeJsonSignature)Variant_decodeJson,
    (decodeJsonSignature)DiagnosticInfo_decodeJson,
    (decodeJsonSignature)decodeJsonInternal
};


static status
Array_decodeJson(void *UA_RESTRICT dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken) {
    status ret = UA_STATUSCODE_GOOD;
    
    if(parseCtx->tokenArray[*parseCtx->index].type != JSMN_ARRAY){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    size_t length = (size_t)parseCtx->tokenArray[*parseCtx->index].size;
    
    /* Return early for empty arrays */
    if(length <= 0) {
        dst = UA_EMPTY_ARRAY_SENTINEL;
        return UA_STATUSCODE_GOOD;
    }

    /* Allocate memory */
    void* mem = UA_calloc(length, type->memSize);
    memcpy(dst, &mem, sizeof(void*)); //Copy new Pointer do dest
    if(!dst)
        return UA_STATUSCODE_BADOUTOFMEMORY;

    (*parseCtx->index)++; // We go to first Array member!
    
    /* Decode array members */
    uintptr_t ptr = (uintptr_t)mem;
    size_t decode_index = type->builtin ? type->typeIndex : UA_BUILTIN_TYPES_COUNT;
    for(size_t i = 0; i < length; ++i) {
        ret = decodeJsonJumpTable[decode_index]((void*)ptr, type, ctx, parseCtx, UA_TRUE);
        if(ret != UA_STATUSCODE_GOOD) {
            /* +1 because last element is also already initialized */
            UA_Array_delete(dst, i+1, type);
            dst = NULL;
            return ret;
        }
        ptr += type->memSize;
    }
    
    return UA_STATUSCODE_GOOD;
}





static status
decodeJsonInternal(void *dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken) {
    /* Check the recursion limit */
    if(ctx->depth > UA_ENCODING_MAX_RECURSION)
        return UA_STATUSCODE_BADENCODINGERROR;
    ctx->depth++;

    uintptr_t ptr = (uintptr_t)dst;
    status ret = UA_STATUSCODE_GOOD;
    u8 membersSize = type->membersSize;
    const UA_DataType *typelists[2] = { UA_TYPES, &type[-type->typeIndex] };
    
    
    /* For decode Fields */
    const char* fieldNames[membersSize];
    void *fieldPointer[membersSize];
    decodeJsonSignature functions[membersSize];
    
    
    for(size_t i = 0; i < membersSize && ret == UA_STATUSCODE_GOOD; ++i) {
        const UA_DataTypeMember *member = &type->members[i];
        const UA_DataType *membertype = &typelists[!member->namespaceZero][member->memberTypeIndex];
        if(!member->isArray) {
            ptr += member->padding;
            size_t fi = membertype->builtin ? membertype->typeIndex : UA_BUILTIN_TYPES_COUNT;
            size_t memSize = membertype->memSize;
            
            fieldNames[i] = member->memberName;
            fieldPointer[i] = (void *)ptr;
            functions[i] = decodeJsonJumpTable[fi];
            
            //ret |= decodeJsonJumpTable[fi]((void *UA_RESTRICT)ptr, membertype, ctx, parseCtx);
            ptr += memSize;
        } else {
            ptr += member->padding;
           // size_t *length = (size_t*)ptr;
            ptr += sizeof(size_t);
            fieldNames[i] = member->memberName;
            fieldPointer[i] = (void *)ptr;
            functions[i] = Array_decodeJson;
            //ret |= Array_decodeBinary((void *UA_RESTRICT *UA_RESTRICT)ptr, length, membertype, ctx);
            ptr += sizeof(void*);
        }
    }
    
    
    decodeFields(ctx, parseCtx, membersSize, fieldNames, functions, fieldPointer, type, NULL);

    ctx->depth--;
    return ret;
}

status
UA_decodeJson(const UA_ByteString *src, size_t *offset, void *dst,
                const UA_DataType *type, size_t customTypesSize,
                const UA_DataType *customTypes) {
    /* Set up the context */
    Ctx ctx;
    ctx.pos = &src->data[*offset];
    ctx.end = &src->data[src->length];
    ctx.depth = 0;
    ctx.customTypesArraySize = customTypesSize;
    ctx.customTypesArray = customTypes;

    
    UA_UInt16 tokenIndex = 0;
    ParseCtx parseCtx;
    parseCtx.tokenCount = 0;
    parseCtx.index = &tokenIndex;

    jsmn_parser p;
    

    jsmn_init(&p);
    parseCtx.tokenCount = (UA_Int32)jsmn_parse(&p, (char*)src->data, src->length, parseCtx.tokenArray, sizeof (parseCtx.tokenArray) / sizeof (parseCtx.tokenArray[0]));
    
    if (parseCtx.tokenCount < 0) {
        //printf("Failed to parse JSON: %d\n", tokenCount);
        return UA_STATUSCODE_BADDECODINGERROR;
    }

    /* Assume the top-level element is an object */
    if (parseCtx.tokenCount < 1 || parseCtx.tokenArray[0].type != JSMN_OBJECT) {
        //printf("Object expected\n");
        
        if(parseCtx.tokenCount == 1){
            if(parseCtx.tokenArray[0].type == JSMN_PRIMITIVE || parseCtx.tokenArray[0].type == JSMN_STRING){
                            /* Decode */
               memset(dst, 0, type->memSize); /* Initialize the value */
               status ret = decodeJsonInternal(dst, type, &ctx, &parseCtx, UA_TRUE);
               return ret;
            }
        }
        
        return UA_STATUSCODE_BADDECODINGERROR;
    }

    
    /* Decode */
    memset(dst, 0, type->memSize); /* Initialize the value */
    status ret = decodeJsonInternal(dst, type, &ctx, &parseCtx, UA_TRUE);

    /*if(ret == UA_STATUSCODE_GOOD) {
        // Set the new offset 
        *offset = (size_t)(ctx.pos - src->data) / sizeof(u8);
    } else {
        // Clean up 
        UA_deleteMembers(dst, type);
        memset(dst, 0, type->memSize);
    }*/
    return ret;
}