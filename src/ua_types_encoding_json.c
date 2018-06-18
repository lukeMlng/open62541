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
#include "ua_plugin_log.h"

#include "../deps/libb64/cencode.h"
#include "../deps/libb64/cdecode.h"
#include "../deps/musl/floatscan.h"
#include "../deps/musl/vfprintf.h"

#include "libc_time.h"

#define UA_ENCODING_MAX_RECURSION 20

#define ENCODE_JSON(TYPE) static status \
    TYPE##_encodeJson(const UA_##TYPE *UA_RESTRICT src, const UA_DataType *type, Ctx *UA_RESTRICT ctx, UA_Boolean useReversible)

#define ENCODE_DIRECT(SRC, TYPE) TYPE##_encodeJson((const UA_##TYPE*)SRC, NULL, ctx, useReversible)

extern const encodeJsonSignature encodeJsonJumpTable[UA_BUILTIN_TYPES_COUNT + 1];
extern const decodeJsonSignature decodeJsonJumpTable[UA_BUILTIN_TYPES_COUNT + 1];

static status encodeJsonInternal(const void *src, const UA_DataType *type, Ctx *ctx, UA_Boolean useReversible);
UA_String UA_DateTime_toJSON(UA_DateTime t);
ENCODE_JSON(ByteString);


const u8 hexmapLower[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
const u8 hexmapUpper[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};


/**
 * JSON HELPER
 */
#define WRITE(ELEM) writeJson##ELEM(ctx)

#define JSON(ELEM) static status writeJson##ELEM(Ctx *UA_RESTRICT ctx)

//static UA_Boolean commaNeeded = UA_FALSE;
//static UA_UInt32 innerObject = 0;

JSON(Quote) {
    if (ctx->pos + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    *(ctx->pos++) = '"';
    return UA_STATUSCODE_GOOD;
}

JSON(ObjStart) {
    if (ctx->pos + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    *(ctx->pos++) = '{';
    //innerObject++;
    return UA_STATUSCODE_GOOD;
}

JSON(ObjEnd) {
    if (ctx->pos + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
        
    *(ctx->pos++) = '}';
    //innerObject--;
    //commaNeeded = UA_TRUE;
    return UA_STATUSCODE_GOOD;
}

JSON(ArrayStart) {
    if (ctx->pos + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
        
    *(ctx->pos++) = '[';
    return UA_STATUSCODE_GOOD;
}

JSON(ArrayEnd) {
    if (ctx->pos + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    *(ctx->pos++) = ']';
    return UA_STATUSCODE_GOOD;
}

JSON(Comma) {
    if (ctx->pos + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    *(ctx->pos++) = ',';
    return UA_STATUSCODE_GOOD;
}

JSON(dPoint) {
    if (ctx->pos + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    *(ctx->pos++) = ':';
    return UA_STATUSCODE_GOOD;
}

status writeComma(Ctx *ctx, UA_Boolean commaNeeded) {
    if (commaNeeded) {
        return WRITE(Comma);
    }

    return UA_STATUSCODE_GOOD;
}

status writeNull(Ctx *ctx) {
    if (ctx->pos + 4 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    *(ctx->pos++) = 'n';
    *(ctx->pos++) = 'u';
    *(ctx->pos++) = 'l';
    *(ctx->pos++) = 'l';
    return UA_STATUSCODE_GOOD;
}

status writeKey_UA_String(Ctx *ctx, UA_String *key, UA_Boolean commaNeeded){
    if (ctx->pos + key->length + 1 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    if(!key || key->length < 1){
        return UA_STATUSCODE_BADENCODINGERROR;
    }
    UA_STACKARRAY(char, fieldKeyString, key->length + 1);
    //char fieldKeyString[key->length + 1];
    memset(&fieldKeyString, 0, key->length + 1);
    memcpy(&fieldKeyString, key->data, key->length);
    return writeKey(ctx, fieldKeyString, commaNeeded);
}

status writeKey(Ctx *ctx, const char* key, UA_Boolean commaNeeded) {
    
    size_t size = strlen(key);
    if (ctx->pos + size + 4 > ctx->end) //4 because of " " : and ,
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    status ret = UA_STATUSCODE_GOOD;
    ret |= writeComma(ctx, commaNeeded);
    ret |= WRITE(Quote);
    for (size_t i = 0; i < size; i++) {
        *(ctx->pos++) = (u8)key[i];
    }
    ret |= WRITE(Quote);
    ret |= WRITE(dPoint);

    //commaNeeded = UA_TRUE;
    return ret;
}

status encodingJsonStartObject(Ctx *ctx) {
    status ret = WRITE(ObjStart);
    //commaNeeded = UA_FALSE;
    return ret;
}

size_t encodingJsonEndObject(Ctx *ctx) {
    return WRITE(ObjEnd);
}

status encodingJsonStartArray(Ctx *ctx) {
    status ret = WRITE(ArrayStart);
    //commaNeeded = UA_FALSE;
    return ret;
}

size_t encodingJsonEndArray(Ctx *ctx) {
    return WRITE(ArrayEnd);
}

/*****************/
/* Integer Types */
/*****************/

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

// function to reverse buffer
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

//Based on http://www.techiedelight.com/implement-itoa-function-in-c/
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

    if (value < 0)
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
    if(!src){
        return UA_STATUSCODE_BADENCODINGERROR;//writeNull(ctx);
    }
    
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
    if(!src){
        return writeNull(ctx);
    }
    char buf[3];
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);
    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* signed Byte */
ENCODE_JSON(SByte) {
    if(!src){
        return writeNull(ctx);
    }
    char buf[4];
    UA_UInt16 digits = itoa(*src, buf);
    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* UInt16 */
ENCODE_JSON(UInt16) {
    if(!src){
        return writeNull(ctx);
    }
    char buf[5];
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* Int16 */
ENCODE_JSON(Int16) {
    if(!src){
        return writeNull(ctx);
    }
    char buf[6];
    UA_UInt16 digits = itoa(*src, buf);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* UInt32 */
ENCODE_JSON(UInt32) {
    if(!src){
        return writeNull(ctx);
    }
    char buf[10];
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* Int32 */
ENCODE_JSON(Int32) {
    if(!src){
        return writeNull(ctx);
    }
    char buf[11];
    UA_UInt16 digits = itoa(*src, buf);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* UInt64 */
ENCODE_JSON(UInt64) {
    if(!src){
        return writeNull(ctx);
    }
    char buf[20];
    UA_UInt16 digits = itoaUnsigned(*src, buf, 10);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/* Int64 */
ENCODE_JSON(Int64) {
    if(!src){
        return writeNull(ctx);
    }

    char buf[20]; //TODO size
    UA_UInt16 digits = itoa(*src, buf);

    if (ctx->pos + digits > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buf, digits);
    ctx->pos += digits;
    return UA_STATUSCODE_GOOD;
}

/************************/
/* Floating Point Types */
/************************/

ENCODE_JSON(Float) {
    if(!src){
        return writeNull(ctx);
    }
    char buffer[50]; //TODO: minimize stack size
    memset(buffer, 0, 50);
    fmt_fp(buffer, *src, 0, -1, 0, 'g');
    size_t len = strlen(buffer);

    if (ctx->pos + len > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buffer, len);

    ctx->pos += (len);
    return UA_STATUSCODE_GOOD;
}

ENCODE_JSON(Double) {
    if(!src){
        return writeNull(ctx);
    }
    char buffer[50]; //TODO: minimize stack size
    memset(buffer, 0, 50);
    fmt_fp(buffer, *src, 0, 17, 0, 'g');
    size_t len = strlen(buffer);

    if (ctx->pos + len > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    memcpy(ctx->pos, buffer, len);

    ctx->pos += (len);
    return UA_STATUSCODE_GOOD;
}

/******************/
/* Array Handling */

/******************/

static status
Array_encodeJsonComplex(uintptr_t ptr, size_t length, const UA_DataType *type, Ctx *ctx, UA_Boolean useReversible) {
    status ret = UA_STATUSCODE_GOOD; 
    
    /* Get the encoding function for the data type. The jumptable at
     * UA_BUILTIN_TYPES_COUNT points to the generic UA_encodeJson method */
    size_t encode_index = type->builtin ? type->typeIndex : UA_BUILTIN_TYPES_COUNT;
    encodeJsonSignature encodeType = encodeJsonJumpTable[encode_index];

    WRITE(ArrayStart);
    UA_Boolean commaNeeded = false;

    /* Encode every element */
    for (size_t i = 0; i < length; ++i) {
        if (commaNeeded) {
            WRITE(Comma);
        }

        ret = encodeType((const void*) ptr, type, ctx, useReversible);
        ptr += type->memSize;

        if (ret != UA_STATUSCODE_GOOD) {
            //TODO
            return ret;
        }
        commaNeeded = true;
    }

    //commaNeeded = true;
    WRITE(ArrayEnd);
    return ret;
}

static status
Array_encodeJson(const void *src, size_t length, const UA_DataType *type, Ctx *ctx, UA_Boolean isVariantArray, UA_Boolean useReversible) {
    return Array_encodeJsonComplex((uintptr_t) src, length, type, ctx, useReversible);
}

/*****************/
/* Builtin Types */
/*****************/

static size_t utf8_check_first(char byte)
{
    unsigned char u = (unsigned char)byte;

    if(u < 0x80)
        return 1;

    if(0x80 <= u && u <= 0xBF) {
        /* second, third or fourth byte of a multi-byte
           sequence, i.e. a "continuation byte" */
        return 0;
    }
    else if(u == 0xC0 || u == 0xC1) {
        /* overlong encoding of an ASCII byte */
        return 0;
    }
    else if(0xC2 <= u && u <= 0xDF) {
        /* 2-byte sequence */
        return 2;
    }

    else if(0xE0 <= u && u <= 0xEF) {
        /* 3-byte sequence */
        return 3;
    }
    else if(0xF0 <= u && u <= 0xF4) {
        /* 4-byte sequence */
        return 4;
    }
    else { /* u >= 0xF5 */
        /* Restricted (start of 4-, 5- or 6-byte sequence) or invalid
           UTF-8 */
        return 0;
    }
}

static size_t utf8_check_full(const char *buffer, size_t size, int32_t *codepoint)
{
    size_t i;
    int32_t value = 0;
    unsigned char u = (unsigned char)buffer[0];

    if(size == 2)
    {
        value = u & 0x1F;
    }
    else if(size == 3)
    {
        value = u & 0xF;
    }
    else if(size == 4)
    {
        value = u & 0x7;
    }
    else
        return 0;

    for(i = 1; i < size; i++)
    {
        u = (unsigned char)buffer[i];

        if(u < 0x80 || u > 0xBF) {
            /* not a continuation byte */
            return 0;
        }

        value = (value << 6) + (u & 0x3F);
    }

    if(value > 0x10FFFF) {
        /* not in Unicode range */
        return 0;
    }

    else if(0xD800 <= value && value <= 0xDFFF) {
        /* invalid code point (UTF-16 surrogate halves) */
        return 0;
    }

    else if((size == 2 && value < 0x80) ||
            (size == 3 && value < 0x800) ||
            (size == 4 && value < 0x10000)) {
        /* overlong encoding */
        return 0;
    }

    if(codepoint)
        *codepoint = value;

    return 1;
}

static const char *utf8_iterate(const char *buffer, size_t bufsize, int32_t *codepoint)
{
    size_t count;
    int32_t value;

    if(!bufsize)
        return buffer;

    count = utf8_check_first(buffer[0]);
    if(count <= 0)
        return NULL;

    if(count == 1)
        value = (unsigned char)buffer[0];
    else
    {
        if(count > bufsize || !utf8_check_full(buffer, count, &value))
            return NULL;
    }

    if(codepoint)
        *codepoint = value;

    return buffer + count;
}

ENCODE_JSON(String) {
    if (!src) {
        return writeNull(ctx);
    }
    
    if(src->data == NULL){
        return writeNull(ctx);
    }

    /*size_t escape_characters = 0;
    size_t actualLengthWithoutQuotes = 0;


    for (size_t i = 0; i < src->length; i++) {
        switch (src->data[i]) {
            case '\"'://quotation mark
            case '\\'://reverse solidus
            case '\b'://backspace
            case '\f'://formfeed
            case '\n'://newline
            case '\r'://carriage return
            case '\t'://horizontal tab
                // one character escape sequence
                escape_characters++;
                break;
            default:
                if (src->data[i] < 32 ) {
                    // UTF-16 escape sequence \uXXXX 
                    escape_characters += 6;
                }
                break;
        }
    }

    actualLengthWithoutQuotes = src->length + escape_characters;

    if (ctx->pos + actualLengthWithoutQuotes + 2 > ctx->end) // +2 for quotation marks
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    ret |= WRITE(Quote);

    // no characters have to be escaped 
    if (escape_characters == 0) {
        memcpy(ctx->pos, src->data, actualLengthWithoutQuotes);
        ctx->pos += actualLengthWithoutQuotes;
    } else {
        // copy the string 
        for (size_t i = 0; i < src->length; i++) {

            // character needs to be escaped
            switch (src->data[i]) {
                case '\\':
                    *(ctx->pos++) = '\\';
                    *(ctx->pos++) = '\\';
                    break;
                case '\"':
                    *(ctx->pos++) = '\\';
                    *(ctx->pos++) = '\"';
                    break;
                case '\b':
                    *(ctx->pos++) = '\\';
                    *(ctx->pos++) = 'b';
                    break;
                case '\f':
                    *(ctx->pos++) = '\\';
                    *(ctx->pos++) = 'f';
                    break;
                case '\n':
                    *(ctx->pos++) = '\\';
                    *(ctx->pos++) = 'n';
                    break;
                case '\r':
                    *(ctx->pos++) = '\\';
                    *(ctx->pos++) = 'r';
                    break;
                case '\t':
                    *(ctx->pos++) = '\\';
                    *(ctx->pos++) = 't';
                    break;
                default:
                    if (src->data[i] <= '\x1f' ) {
                        *(ctx->pos++) = '\\';
                        *(ctx->pos++) = 'u';
                        *(ctx->pos++) = '0';
                        *(ctx->pos++) = '0';

                        UA_Byte n1 = src->data[i] >> 4;
                        UA_Byte n2 = src->data[i] & 0xf;
                        *(ctx->pos++) = hexmapLower[n1];
                        *(ctx->pos++) = hexmapLower[n2];
                    } else {
                        *(ctx->pos++) = src->data[i];
                    }
                    break;
            }
        }
    }*/
    
    
    //------------------------------
    //escaping adapted from https://github.com/akheron/jansson dump.c
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    
    
    const char *pos, *end, *lim;
    UA_Int32 codepoint;
    const char *str = (char*)src->data;
    ret |= WRITE(Quote);
    if(ret != UA_STATUSCODE_GOOD)
        return ret;
    

    end = pos = str;
    lim = str + src->length;
    while(1)
    {
        const char *text;
        u8 seq[13];
        size_t length;

        while(end < lim)
        {
            end = utf8_iterate(pos, (size_t)(lim - pos), &codepoint);
            if(!end)
                return UA_STATUSCODE_BADENCODINGERROR;

            /* mandatory escape or control char */
            if(codepoint == '\\' || codepoint == '"' || codepoint < 0x20)
                break;

            /* slash */
            //if((flags & JSON_ESCAPE_SLASH) && codepoint == '/')
            //    break;

            /* non-ASCII */
            //if((flags & JSON_ENSURE_ASCII) && codepoint > 0x7F)
            //    break;

            pos = end;
        }

        if(pos != str) {
            if (ctx->pos + (pos - str) > ctx->end)
                return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
            memcpy(ctx->pos, str, (size_t)(pos - str));
            ctx->pos += pos - str;
        }

        if(end == pos)
            break;

        /* handle \, /, ", and control codes */
        length = 2;
        switch(codepoint)
        {
            case '\\': text = "\\\\"; break;
            case '\"': text = "\\\""; break;
            case '\b': text = "\\b"; break;
            case '\f': text = "\\f"; break;
            case '\n': text = "\\n"; break;
            case '\r': text = "\\r"; break;
            case '\t': text = "\\t"; break;
            case '/':  text = "\\/"; break;
            default:
            {
                /* codepoint is in BMP */
                if(codepoint < 0x10000)
                {
                    seq[0] = '\\';
                    seq[1] = 'u';
                    UA_Byte b1 = (UA_Byte)(codepoint >> 8);
                    UA_Byte b2 = (UA_Byte)(codepoint >> 0);
                    seq[2] = hexmapLower[(b1 & 0xF0) >> 4];
                    seq[3] = hexmapLower[b1 & 0x0F];
                    seq[4] = hexmapLower[(b2 & 0xF0) >> 4];
                    seq[5] = hexmapLower[b2 & 0x0F];

                    //snprintf(seq, sizeof(seq), "\\u%04X", (unsigned int)codepoint);
                    length = 6;
                }

                /* not in BMP -> construct a UTF-16 surrogate pair */
                else
                {
                    UA_Int32 first, last;

                    codepoint -= 0x10000;
                    first = 0xD800 | ((codepoint & 0xffc00) >> 10);
                    last = 0xDC00 | (codepoint & 0x003ff);

                    UA_Byte fb1 = (UA_Byte)(first >> 8);
                    UA_Byte fb2 = (UA_Byte)(first >> 0);
                    
                    UA_Byte lb1 = (UA_Byte)(last >> 8);
                    UA_Byte lb2 = (UA_Byte)(last >> 0);
                    
                    seq[0] = '\\';
                    seq[1] = 'u';
                    seq[2] = hexmapLower[(fb1 & 0xF0) >> 4];
                    seq[3] = hexmapLower[fb1 & 0x0F];
                    seq[4] = hexmapLower[(fb2 & 0xF0) >> 4];
                    seq[5] = hexmapLower[fb2 & 0x0F];
                    
                    seq[6] = '\\';
                    seq[7] = 'u';
                    seq[8] = hexmapLower[(lb1 & 0xF0) >> 4];
                    seq[9] = hexmapLower[lb1 & 0x0F];
                    seq[10] = hexmapLower[(lb2 & 0xF0) >> 4];
                    seq[11] = hexmapLower[lb2 & 0x0F];
                    
                    //snprintf(seq, sizeof(seq), "\\u%04X\\u%04X", (unsigned int)first, (unsigned int)last);
                    length = 12;
                }

                text = (char*)seq;
                break;
            }
        }

        if (ctx->pos + length > ctx->end)
            return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
        memcpy(ctx->pos, text, length);
        ctx->pos += length;
        
        str = pos = end;
    }

    
    ret |= WRITE(Quote);
    
    return ret;
}
    
ENCODE_JSON(ByteString) {
    if(!src || src->length < 1 || src->data == NULL){
        return writeNull(ctx);
    }
  
    //Estimate base64 size, this is a few bytes bigger 
    //https://stackoverflow.com/questions/1533113/calculate-the-size-to-a-base-64-encoded-message
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

#undef hexCharlowerCase

/* Guid */
ENCODE_JSON(Guid) {
    if(!src){
        return writeNull(ctx);
    }
    
    status ret = UA_STATUSCODE_GOOD;
#ifdef hexCharlowerCase
    const u8 *hexmap = hexmapLower;
#else
    const u8 *hexmap = hexmapUpper;
#endif
    if (ctx->pos + 38 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;

    ret |= WRITE(Quote);
    u8 *buf = ctx->pos;

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

    ctx->pos += 36;
    ret |= WRITE(Quote);

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
    UA_DateTimeStruct tSt = UA_DateTime_toStruct(t);

    UA_String str = {24, (u8*) UA_malloc(24)};
    if (!str.data)
        return UA_STRING_NULL;
    
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
    str.data[19] = '.';
    printNumber(tSt.milliSec, &str.data[20], 3);
    str.data[23] = 'Z';
    return str;
}

ENCODE_JSON(DateTime) {
    if(!src){
        return writeNull(ctx);
    }
    
    if (ctx->pos + 24 > ctx->end)
        return UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED;
    
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    UA_String str = UA_DateTime_toJSON(*src);
    ret = ENCODE_DIRECT(&str, String);
    UA_String_deleteMembers(&str);
    return ret;
}

/* NodeId */
#define UA_NODEIDTYPE_NUMERIC_TWOBYTE 0
#define UA_NODEIDTYPE_NUMERIC_FOURBYTE 1
#define UA_NODEIDTYPE_NUMERIC_COMPLETE 2

#define UA_EXPANDEDNODEID_SERVERINDEX_FLAG 0x40
#define UA_EXPANDEDNODEID_NAMESPACEURI_FLAG 0x80

//TODO: Namespace and encoding
static status
NodeId_encodeJsonInternal(UA_NodeId const *src, Ctx *ctx, UA_Boolean useReversible) {
    status ret = UA_STATUSCODE_GOOD;
    if(!src){
        writeNull(ctx);
        return ret;
    }
    
    switch (src->identifierType) {
        case UA_NODEIDTYPE_NUMERIC:
        {
            ret |= writeKey(ctx, "Id", UA_FALSE);
            ret |= ENCODE_DIRECT(&src->identifier.numeric, UInt32);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            
            break;
        }
        case UA_NODEIDTYPE_STRING:
        {
            ret |= writeKey(ctx, "IdType", UA_FALSE);
            UA_UInt16 typeNumber = 1;
            ret |= ENCODE_DIRECT(&typeNumber, UInt16);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            ret |= writeKey(ctx, "Id", UA_TRUE);
            ret |= ENCODE_DIRECT(&src->identifier.string, String);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            break;
        }
        case UA_NODEIDTYPE_GUID:
        {
            ret |= writeKey(ctx, "IdType", UA_FALSE);
            UA_UInt16 typeNumber = 2;
            ret |= ENCODE_DIRECT(&typeNumber, UInt16);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            /* Id */
            ret |= writeKey(ctx, "Id", UA_TRUE);
            ret |= ENCODE_DIRECT(&src->identifier.guid, Guid);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;

            break;
        }
        case UA_NODEIDTYPE_BYTESTRING:
        {
            // {"IdType":0,"Text":"Text"}

            ret |= writeKey(ctx, "IdType", UA_FALSE);
            UA_UInt16 typeNumber = 3;
            ret |= ENCODE_DIRECT(&typeNumber, UInt16);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            /* Id */
            ret |= writeKey(ctx, "Id", UA_TRUE);
            ret |= ENCODE_DIRECT(&src->identifier.byteString, ByteString);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;

            break;
        }
        default:
            return UA_STATUSCODE_BADENCODINGERROR;
    }

    if (useReversible) {
        if (src->namespaceIndex > 0) {
            ret |= writeKey(ctx, "Namespace", UA_TRUE);
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
        }
    } else {
        /* For the non-reversible encoding, the field is the NamespaceUri 
         * associated with the NamespaceIndex, encoded as a JSON string.
         * A NamespaceIndex of 1 is always encoded as a JSON number.
         */

        //@TODO LOOKUP namespace uri and check if unknown
        if (src->namespaceIndex == 1) {
            writeKey(ctx, "Namespace", UA_TRUE);
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
        } else {
            writeKey(ctx, "Namespace", UA_TRUE);
            
            //Check if Namespace given and in range
            if(src->namespaceIndex < ctx->namespacesSize 
                    && ctx->namespaces != NULL){
                
                UA_String namespaceEntry = ctx->namespaces[src->namespaceIndex];
                ret |= ENCODE_DIRECT(&namespaceEntry, String);
                if (ret != UA_STATUSCODE_GOOD)
                    return ret;
            }else{
                return UA_STATUSCODE_BADNOTFOUND;
            }
        }
    }

    return ret;
}

ENCODE_JSON(NodeId) {
    if(!src){
        return writeNull(ctx);
    }
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    ret |= WRITE(ObjStart);
    if (ret != UA_STATUSCODE_GOOD)
        return ret;
    ret = NodeId_encodeJsonInternal(src, ctx, useReversible);
    if (ret != UA_STATUSCODE_GOOD)
        return ret;
    ret = WRITE(ObjEnd);
    return ret;
}

/* ExpandedNodeId */
ENCODE_JSON(ExpandedNodeId) {
    if(!src){
        return writeNull(ctx);
    }

    WRITE(ObjStart);
    /* Set up the encoding mask */
    u8 encoding = 0;
    if ((void*) src->namespaceUri.data > UA_EMPTY_ARRAY_SENTINEL)
        encoding |= UA_EXPANDEDNODEID_NAMESPACEURI_FLAG;
    if (src->serverIndex > 0)
        encoding |= UA_EXPANDEDNODEID_SERVERINDEX_FLAG;

    /* Encode the NodeId */
    status ret = NodeId_encodeJsonInternal(&src->nodeId, ctx, useReversible);
    if (ret != UA_STATUSCODE_GOOD)
        return ret;
    
    if ((void*) src->namespaceUri.data > UA_EMPTY_ARRAY_SENTINEL) {
        writeKey(ctx, "Namespace", UA_TRUE);
        WRITE(Quote);
        ret = ENCODE_DIRECT(&src->namespaceUri, String);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        WRITE(Quote);
        UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    /* Encode the serverIndex */
    if (src->serverIndex > 0) {
        writeKey(ctx, "ServerUri", UA_TRUE);
        ret = ENCODE_DIRECT(&src->serverIndex, UInt32);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    ret |= WRITE(ObjEnd);
    return ret;
}

/* LocalizedText */
ENCODE_JSON(LocalizedText) {
    if(!src){
        return writeNull(ctx);
    }
    status ret = UA_STATUSCODE_GOOD;

    if (useReversible) {
        //commaNeeded = UA_FALSE;

        ret = WRITE(ObjStart);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        // {"Locale":"asd","Text":"Text"}
        ret = writeKey(ctx, "Locale", UA_FALSE);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        ret |= ENCODE_DIRECT(&src->locale, String);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        
        ret = writeKey(ctx, "Text", UA_TRUE);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        ret |= ENCODE_DIRECT(&src->text, String);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        
        WRITE(ObjEnd);
    } else {
        /* For the non-reversible form, LocalizedText value shall 
         * be encoded as a JSON string containing the Text component.*/
        ret |= ENCODE_DIRECT(&src->text, String);
    }
    return ret;
}

ENCODE_JSON(QualifiedName) {
    if(!src){
        return writeNull(ctx);
    }
    
    status ret = UA_STATUSCODE_GOOD;

    //commaNeeded = UA_FALSE;

    ret = WRITE(ObjStart);
    if (ret != UA_STATUSCODE_GOOD)
        return ret;
    ret = writeKey(ctx, "Name", UA_FALSE);
    
    if (ret != UA_STATUSCODE_GOOD)
        return ret;
    ret |= ENCODE_DIRECT(&src->name, String);
    if (ret != UA_STATUSCODE_GOOD)
        return ret;
    
    if (useReversible) {
        if (src->namespaceIndex != 0) {
            ret = writeKey(ctx, "Uri", UA_TRUE);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
        }
    } else {

        /*For the non-reversible form, the NamespaceUri associated with the NamespaceIndex portion of
         * the QualifiedName is encoded as JSON string unless the NamespaceIndex is 1 or if
         * NamespaceUri is unknown. In these cases, the NamespaceIndex is encoded as a JSON number.
         */

        if (src->namespaceIndex == 1) {
            ret = writeKey(ctx, "Uri", UA_TRUE);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
        } else {
            ret |= writeKey(ctx, "Uri", UA_TRUE);
            
             //Check if Namespace given and in range
            if(src->namespaceIndex < ctx->namespacesSize 
                    && ctx->namespaces != NULL){
                
                UA_String namespaceEntry = ctx->namespaces[src->namespaceIndex];
                ret |= ENCODE_DIRECT(&namespaceEntry, String);
            }else{
                //if not encode as Number
                ret |= ENCODE_DIRECT(&src->namespaceIndex, UInt16);
                if (ret != UA_STATUSCODE_GOOD)
                    return ret;
            }
        }
    }

    ret |= WRITE(ObjEnd);
    return ret;
}

ENCODE_JSON(StatusCode) {
    if(!src){
        return writeNull(ctx);
    }
    status ret = UA_STATUSCODE_GOOD;

    if (!useReversible) {
        if(*src != 0){
            ret |= WRITE(ObjStart);
            //commaNeeded = UA_FALSE;
            ret |= writeKey(ctx, "Code", UA_FALSE);
            ret |= ENCODE_DIRECT(src, UInt32);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            ret |= writeKey(ctx, "Symbol", UA_TRUE);
            /* encode the full name of error */
            UA_String statusDescription = UA_String_fromChars(UA_StatusCode_name(*src));
            if(statusDescription.data == NULL && statusDescription.length == 0){
                return UA_STATUSCODE_BADENCODINGERROR;
            }
            ret |= ENCODE_DIRECT(&statusDescription, String);
            UA_String_deleteMembers(&statusDescription);

            ret |= WRITE(ObjEnd);
        }else{
            /* A StatusCode of Good (0) is treated like a NULL and not encoded. */
            ret |= writeNull(ctx);
        }
    } else {
        ret |= ENCODE_DIRECT(src, UInt32);
    }

    return ret;
}

/* ExtensionObject */
ENCODE_JSON(ExtensionObject) {
    if(!src){
        return writeNull(ctx);
    }
    
    u8 encoding = (u8) src->encoding;
    
    if(encoding == UA_EXTENSIONOBJECT_ENCODED_NOBODY){
        return writeNull(ctx);
    }
    
    status ret = UA_STATUSCODE_GOOD;
    UA_Boolean commaNeeded = UA_FALSE;
    /* already encoded content.*/
    if (encoding <= UA_EXTENSIONOBJECT_ENCODED_XML) {
        ret |= WRITE(ObjStart);

        if(useReversible){
            ret |= writeKey(ctx, "TypeId", commaNeeded);
            commaNeeded = true;
            ret |= ENCODE_DIRECT(&src->content.encoded.typeId, NodeId);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
        }
        
        switch (src->encoding) {
            case UA_EXTENSIONOBJECT_ENCODED_BYTESTRING:
            {
                if(useReversible){
                    UA_Byte jsonEncodingField = 1;
                    ret |= writeKey(ctx, "Encoding", commaNeeded);
                    commaNeeded = true;
                    ret |= ENCODE_DIRECT(&jsonEncodingField, Byte);
                }
                ret |= writeKey(ctx, "Body", commaNeeded);
                ret |= ENCODE_DIRECT(&src->content.encoded.body, ByteString);
                break;
            }
            case UA_EXTENSIONOBJECT_ENCODED_XML:
            {
                if(useReversible){
                    UA_Byte jsonEncodingField = 2;
                    ret |= writeKey(ctx, "Encoding", commaNeeded);
                    commaNeeded = true;
                    ret |= ENCODE_DIRECT(&jsonEncodingField, Byte);
                }
                ret |= writeKey(ctx, "Body", commaNeeded);
                ret |= ENCODE_DIRECT(&src->content.encoded.body, String);
                break;
            }
            default:
                ret = UA_STATUSCODE_BADINTERNALERROR;
        }

        ret |= WRITE(ObjEnd);
    }else{ 
        
        /* Cannot encode with no type description */
        if (!src->content.decoded.type)
            return UA_STATUSCODE_BADENCODINGERROR;

        if(!src->content.decoded.data){
            return writeNull(ctx);
        }
        
        UA_NodeId typeId = src->content.decoded.type->typeId;
        if (typeId.identifierType != UA_NODEIDTYPE_NUMERIC)
            return UA_STATUSCODE_BADENCODINGERROR;
        
        if (useReversible) {
            //-----------REVERSIBLE-------------------
            ret |= WRITE(ObjStart);

            ret |= writeKey(ctx, "TypeId", UA_FALSE);
            ret |= ENCODE_DIRECT(&typeId, NodeId);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            
            //Encoding field is omitted if the value is 0.
            const UA_DataType *contentType = src->content.decoded.type;

            /* Encode the content */
            ret |= writeKey(ctx, "Body", UA_TRUE);
            ret |= encodeJsonInternal(src->content.decoded.data, contentType, ctx, useReversible);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;

            ret |= WRITE(ObjEnd);
        } else {
            //-----------NON-REVERSIBLE-------------------
            /* For the non-reversible form, ExtensionObject values 
             * shall be encoded as a JSON object containing only the 
             * value of the Body field. The TypeId and Encoding fields are dropped.
             * 
             * TODO: Does this mean there is a "Body" key in the ExtensionObject?
             */
            ret |= WRITE(ObjStart);
            const UA_DataType *contentType = src->content.decoded.type;
            ret |= writeKey(ctx, "Body", UA_FALSE);
            ret |= encodeJsonInternal(src->content.decoded.data, contentType, ctx, useReversible);
            ret |= WRITE(ObjEnd);
        }
    }
    return ret;
}

static status
Variant_encodeJsonWrapExtensionObject(const UA_Variant *src, const bool isArray, Ctx *ctx, UA_Boolean useReversible) {
    //TODO length
    size_t length = 1;

    status ret = UA_STATUSCODE_GOOD;
    if (isArray) {
        if (src->arrayLength > UA_INT32_MAX)
            return UA_STATUSCODE_BADENCODINGERROR;
        
        length = src->arrayLength;
    }

    /* Set up the ExtensionObject */
    UA_ExtensionObject eo;
    UA_ExtensionObject_init(&eo);
    eo.encoding = UA_EXTENSIONOBJECT_DECODED;
    eo.content.decoded.type = src->type;
    const u16 memSize = src->type->memSize;
    uintptr_t ptr = (uintptr_t) src->data;

    if(length > 1){
        WRITE(ArrayStart);;
    }
    
    UA_Boolean commaNeeded = false;
    
    /* Iterate over the array */
    for (size_t i = 0; i <  length && ret == UA_STATUSCODE_GOOD; ++i) {
        if (commaNeeded) {
            WRITE(Comma);
        }
        
        eo.content.decoded.data = (void*) ptr;
        ret = encodeJsonInternal(&eo, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT], ctx, useReversible);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        ptr += memSize;
        
        commaNeeded = true;
    }
    
    if(length > 1){
        WRITE(ArrayEnd);
    }
    return ret;
}

static status
addMatrixContentJSON(Ctx *ctx, void* array, const UA_DataType *type, size_t *index, UA_UInt32 *arrayDimensions, size_t dimensionIndex, size_t dimensionSize, UA_Boolean useReversible) {
    status ret = UA_STATUSCODE_GOOD;
    
    /* Check the recursion limit */
    if (ctx->depth > UA_ENCODING_MAX_RECURSION)
        return UA_STATUSCODE_BADENCODINGERROR;
    ctx->depth++;
    
    if (dimensionIndex == (dimensionSize - 1)) {
        //Stop recursion: The inner Arrays are written
        UA_Boolean commaNeeded = UA_FALSE;

        ret |= WRITE(ArrayStart);

        for (size_t i = 0; i < arrayDimensions[dimensionIndex]; i++) {
            if (commaNeeded) {
                ret |= WRITE(Comma);
            }

            ret |= encodeJsonInternal(((u8*)array) + (type->memSize * *index), type, ctx, useReversible);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            
            commaNeeded = UA_TRUE;
            (*index)++;
        }
        ret |= WRITE(ArrayEnd);

    } else {
        //We have to go deeper
        UA_UInt32 currentDimensionSize = arrayDimensions[dimensionIndex];
        dimensionIndex++;

        UA_Boolean commaNeeded = UA_FALSE;
        ret |= WRITE(ArrayStart);
        for (size_t i = 0; i < currentDimensionSize; i++) {
            if (commaNeeded) {
                ret |= WRITE(Comma);
            }
            ret |= addMatrixContentJSON(ctx, array, type, index, arrayDimensions, dimensionIndex, dimensionSize, useReversible);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
            commaNeeded = UA_TRUE;
        }

        ret |= WRITE(ArrayEnd);
    }
    
    ctx->depth--;
    
    return ret;
}

ENCODE_JSON(Variant) {
    /* Quit early for the empty variant */
    //u8 encoding = 0;
    if(!src){
        return writeNull(ctx);
    }
    
    status ret = UA_STATUSCODE_GOOD;
    if (!src->type){
        return writeNull(ctx);
    }
        
    /* Set the content type in the encoding mask */
    const bool isBuiltin = src->type->builtin;
    const bool isAlias = src->type->membersSize == 1
            && UA_TYPES[src->type->members[0].memberTypeIndex].builtin;
    
    /* Set the array type in the encoding mask */
    const bool isArray = src->arrayLength > 0 || src->data <= UA_EMPTY_ARRAY_SENTINEL;
    const bool hasDimensions = isArray && src->arrayDimensionsSize > 0;
    
    if (useReversible) {
        ret |= WRITE(ObjStart);

        /* Encode the encoding byte */
        ret = UA_STATUSCODE_GOOD; //ENCODE_DIRECT(&encoding, Byte);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;

        /* Encode the content */
        if (!isBuiltin && !isAlias){
            //-------REVERSIBLE:  NOT BUILTIN, can it be encoded? Wrap in extension object .------------
            ret |= writeKey(ctx, "Type", UA_FALSE);
            ret |= ENCODE_DIRECT(&UA_TYPES[UA_TYPES_EXTENSIONOBJECT].typeId.identifier.numeric, UInt32);
            ret |= writeKey(ctx, "Body", UA_TRUE);
            ret |= Variant_encodeJsonWrapExtensionObject(src, isArray, ctx,useReversible);
        } else if (!isArray) {
            //-------REVERSIBLE:  BUILTIN, single value.------------
            ret |= writeKey(ctx, "Type", UA_FALSE);
            ret |= ENCODE_DIRECT(&src->type->typeId.identifier.numeric, UInt32);
            ret |= writeKey(ctx, "Body", UA_TRUE);
            ret |= encodeJsonInternal(src->data, src->type, ctx, useReversible);
        } else {
            //-------REVERSIBLE:   BUILTIN, array.------------
            ret |= writeKey(ctx, "Type", UA_FALSE);
            ret |= ENCODE_DIRECT(&src->type->typeId.identifier.numeric, UInt32);
            ret |= writeKey(ctx, "Body", UA_TRUE);
            ret |= Array_encodeJson(src->data, src->arrayLength, src->type, ctx, UA_TRUE, useReversible);
        }
        
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
        
        /* REVERSIBLE:  Encode the array dimensions */
        if (hasDimensions && ret == UA_STATUSCODE_GOOD) {
            //TODO: Variant Dimension
            ret |= writeKey(ctx, "Dimension", UA_TRUE);
            ret |= Array_encodeJson(src->arrayDimensions, src->arrayDimensionsSize, &UA_TYPES[UA_TYPES_INT32], ctx, UA_FALSE, useReversible);
            if (ret != UA_STATUSCODE_GOOD)
                return ret;
        }

        ret |= WRITE(ObjEnd);
    } else { //NON-REVERSIBLE

        /* 
         * For the non-reversible form, Variant values shall be encoded as a JSON object containing only
         * the value of the Body field. The Type and Dimensions fields are dropped. Multi-dimensional
         * arrays are encoded as a multi dimensional JSON array as described in 5.4.5.
         */
        
        if (!isBuiltin && !isAlias){
            //-------NON REVERSIBLE:  NOT BUILTIN, can it be encoded? Wrap in extension object .------------
            if (src->arrayDimensionsSize > 1) {
                return UA_STATUSCODE_BADNOTIMPLEMENTED;
            }
            
            ret |= WRITE(ObjStart);
            ret |= writeKey(ctx, "Body", UA_FALSE);
            ret |= Variant_encodeJsonWrapExtensionObject(src, isArray, ctx, useReversible);
            ret |= WRITE(ObjEnd);
        } else if (!isArray) {
            //-------NON REVERSIBLE:   BUILTIN, single value.------------
            ret |= WRITE(ObjStart);
            ret |= writeKey(ctx, "Body", UA_FALSE);
            ret |= encodeJsonInternal(src->data, src->type, ctx, useReversible);
            ret |= WRITE(ObjEnd);
        } else {
            //-------NON REVERSIBLE:   BUILTIN, array.------------
            size_t dimensionSize = src->arrayDimensionsSize;
            
            ret |= WRITE(ObjStart);
            ret |= writeKey(ctx, "Body", UA_FALSE);
            
            if (dimensionSize > 1) {
                //nonreversible multidimensional array
                size_t index = 0;  size_t dimensionIndex = 0;
                void *ptr = src->data;
                const UA_DataType *arraytype = src->type;
                ret |= addMatrixContentJSON(ctx, ptr, arraytype, &index, src->arrayDimensions, dimensionIndex, dimensionSize, useReversible);
            } else {
                //nonreversible simple array
                ret |=  Array_encodeJson(src->data, src->arrayLength, src->type, ctx, UA_TRUE, useReversible);
            }
            ret |= WRITE(ObjEnd);
        }
    }
    return ret;
}

/* DataValue */
ENCODE_JSON(DataValue) {
    if(!src){
        return writeNull(ctx);
    }
    
    if(!src->hasServerPicoseconds &&
            !src->hasServerTimestamp &&
            !src->hasSourcePicoseconds &&
            !src->hasSourceTimestamp &&
            !src->hasStatus &&
            !src->hasValue){
        //no element, encode as null
        return writeNull(ctx);
    }
    
    status ret = UA_STATUSCODE_GOOD; 
    ret |= WRITE(ObjStart);
    UA_Boolean commaNeeded = UA_FALSE;
    
    if (src->hasValue) {
        ret |= writeKey(ctx, "Value", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->value, Variant);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    if (src->hasStatus) {
        ret |= writeKey(ctx, "Status", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->status, StatusCode);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    
    if (src->hasSourceTimestamp) {
        ret |= writeKey(ctx, "SourceTimestamp", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->sourceTimestamp, DateTime);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    
    if (src->hasSourcePicoseconds) {
        ret |= writeKey(ctx, "SourcePicoseconds", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->sourcePicoseconds, UInt16);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    
    if (src->hasServerTimestamp) {
        ret |= writeKey(ctx, "ServerTimestamp", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->serverTimestamp, DateTime);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    
    if (src->hasServerPicoseconds) {
        ret |= writeKey(ctx, "ServerPicoseconds", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->serverPicoseconds, UInt16);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    ret |= WRITE(ObjEnd);
    return ret;
}

/* DiagnosticInfo */
ENCODE_JSON(DiagnosticInfo) {
    if(!src){
        return writeNull(ctx);
    }
 
    status ret = UA_STATUSCODE_GOOD;
    if(!src->hasSymbolicId 
            && !src->hasNamespaceUri 
            && !src->hasLocalizedText
            && !src->hasLocale
            && !src->hasAdditionalInfo
            && !src->hasInnerDiagnosticInfo
            && !src->hasInnerStatusCode){
        //no element present, encode as null.
        return writeNull(ctx);
    }
    
    UA_Boolean commaNeeded = UA_FALSE;
    ret |= WRITE(ObjStart);
    
    if (src->hasSymbolicId) {
        ret |= writeKey(ctx, "SymbolicId", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->symbolicId, UInt32);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    if (src->hasNamespaceUri) {
        ret |= writeKey(ctx, "NamespaceUri", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->namespaceUri, UInt32);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    
    if (src->hasLocalizedText) {
        ret |= writeKey(ctx, "LocalizedText", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->localizedText, UInt32);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    
    if (src->hasLocale) {
        ret |= writeKey(ctx, "Locale", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->locale, UInt32);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }
    
    if (src->hasAdditionalInfo) {
        ret |= writeKey(ctx, "AdditionalInfo", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->additionalInfo, String);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    if (src->hasInnerStatusCode) {
        ret |= writeKey(ctx, "InnerStatusCode", commaNeeded);
        commaNeeded = UA_TRUE;
        ret |= ENCODE_DIRECT(&src->innerStatusCode, StatusCode);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    if (src->hasInnerDiagnosticInfo) {
        ret |= writeKey(ctx, "InnerDiagnosticInfo", commaNeeded);
        commaNeeded = UA_TRUE;
        //Check recursion depth in encodeJsonInternal
        ret |= encodeJsonInternal(src->innerDiagnosticInfo, &UA_TYPES[UA_TYPES_DIAGNOSTICINFO], ctx, useReversible);
        if (ret != UA_STATUSCODE_GOOD)
            return ret;
    }

    ret |= WRITE(ObjEnd);
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
    (encodeJsonSignature) QualifiedName_encodeJson, /* QualifiedName */
    (encodeJsonSignature) LocalizedText_encodeJson,
    (encodeJsonSignature) ExtensionObject_encodeJson,
    (encodeJsonSignature) DataValue_encodeJson,
    (encodeJsonSignature) Variant_encodeJson,
    (encodeJsonSignature) DiagnosticInfo_encodeJson,
    (encodeJsonSignature) encodeJsonInternal,
};

static status
encodeJsonInternal(const void *src, const UA_DataType *type, Ctx *ctx, UA_Boolean useReversible) {
    if(!type || !ctx){
        return UA_STATUSCODE_BADENCODINGERROR;
    }
    
    status ret = UA_STATUSCODE_GOOD; 
    
    /* Check the recursion limit */
    if (ctx->depth > UA_ENCODING_MAX_RECURSION)
        return UA_STATUSCODE_BADENCODINGERROR;
    ctx->depth++;

    if (!type->builtin) {
        ret |= WRITE(ObjStart);
    }

    UA_Boolean commaNeeded = UA_FALSE;

    uintptr_t ptr = (uintptr_t) src;
    u8 membersSize = type->membersSize;
    const UA_DataType * typelists[2] = {UA_TYPES, &type[-type->typeIndex]};
    for (size_t i = 0; i < membersSize && ret == UA_STATUSCODE_GOOD; ++i) {
        const UA_DataTypeMember *member = &type->members[i];
        const UA_DataType *membertype = &typelists[!member->namespaceZero][member->memberTypeIndex];

        if (member->memberName != NULL && *member->memberName != 0) {
            writeKey(ctx, member->memberName, commaNeeded);
            commaNeeded = UA_TRUE;
        }

        if (!member->isArray) {
            ptr += member->padding;
            size_t encode_index = membertype->builtin ? membertype->typeIndex : UA_BUILTIN_TYPES_COUNT;
            size_t memSize = membertype->memSize;
            ret = encodeJsonJumpTable[encode_index]((const void*) ptr, membertype, ctx, useReversible);
            ptr += memSize;
            if (ret != UA_STATUSCODE_GOOD) {
                //TODO cleanup
                return ret;
            }
        } else {
            ptr += member->padding;
            const size_t length = *((const size_t*) ptr);
            ptr += sizeof (size_t);
            ret = Array_encodeJson(*(void *UA_RESTRICT const *) ptr, length, membertype, ctx, UA_FALSE, useReversible);
            if (ret != UA_STATUSCODE_GOOD) {
                //TODO cleanup
                return ret;
            }
            ptr += sizeof (void*);
        }
    }

    if (!type->builtin) {
        ret |= WRITE(ObjEnd);
    }
    
    //commaNeeded = true;

    UA_assert(ret != UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED);
    ctx->depth--;
    return ret;
}

status
UA_encodeJson(const void *src, const UA_DataType *type,
        u8 **bufPos, const u8 **bufEnd, UA_String *namespaces, size_t namespaceSize, UA_Boolean useReversible) {
    /* Set up the context */
    Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pos = *bufPos;
    ctx.end = *bufEnd;
    ctx.depth = 0;
    ctx.namespaces = namespaces;
    ctx.namespacesSize = namespaceSize;

    /* Encode */
    status ret = encodeJsonInternal(src, type, &ctx, useReversible);
    if(ret == UA_STATUSCODE_BADENCODINGLIMITSEXCEEDED){
        //TODO: retry?
    }
    
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
decodeJsonInternal(void *dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken);

static status
Array_decodeJson(void *dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken);

static status
Variant_decodeJsonUnwrapExtensionObject(UA_Variant *dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken);

jsmntype_t getJsmnType(const ParseCtx *parseCtx){
    return parseCtx->tokenArray[*parseCtx->index].type;
}

 UA_Boolean isJsonNull(const Ctx *ctx, const ParseCtx *parseCtx){
    if(parseCtx->tokenArray[*parseCtx->index].type != JSMN_PRIMITIVE){
        return false;
    }
    char* elem = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    return (elem[0] == 'n' && elem[1] == 'u' && elem[2] == 'l' && elem[3] == 'l');
}

static int equalCount = 0;
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    equalCount++;
    if(tok){
       if (tok->type == JSMN_STRING) {
            if((int) strlen(s) == tok->end - tok->start ){
                if(strncmp(json + tok->start, s, (size_t)(tok->end - tok->start)) == 0){
                    return 0;
                }   
            }
        }
    }
    
    return -1;
}

DECODE_JSON(Boolean) {
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE && tokenType != JSMN_STRING){ //TODO: HACK for DataSetWriterIdString!
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
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

DECODE_JSON(Float){
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    UA_Float d = 0;
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    char string[size+1];
    memset(string, 0, size+1);
    memcpy(string, data, size);
    
    //TODO, Parameter, prec
    d = (UA_Float)__floatscan(string, 0, 0);
    memcpy(dst, &d, 4);
    return 0;
}

DECODE_JSON(Double){
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    UA_Double d = 0;
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    char* data = (char*)(ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    
    char string[size+1];
    memset(string, 0, size+1);
    memcpy(string, data, size);
    
    //TODO, Parameter, prec one or two for double?
    d = (UA_Double)__floatscan(string, 2, 0);
    memcpy(dst, &d, 8);
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

#undef jsonstringdecodeonheap
DECODE_JSON(String) {
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_STRING && tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    
    size_t size = (size_t)(parseCtx->tokenArray[*parseCtx->index].end - parseCtx->tokenArray[*parseCtx->index].start);
    UA_Byte* stringRef = (ctx->pos + parseCtx->tokenArray[*parseCtx->index].start);
    
#ifdef jsonstringdecodeonheap
    //Allocate
    dst->data = (UA_Byte*)malloc(size * sizeof(UA_Byte));
    if(dst->data == NULL){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    memcpy(dst->data, stringRef, size);
#else
    //Store as Reference
    dst->data = stringRef;
#endif
    
    dst->length = size;

    if(moveToken)
        (*parseCtx->index)++; // String is one element

    return ret;
}

DECODE_JSON(ByteString) {
    jsmntype_t tokenType = getJsmnType(parseCtx);
    if(tokenType != JSMN_STRING && tokenType != JSMN_PRIMITIVE){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    
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
    if(dstData == NULL){
        ret = UA_STATUSCODE_BADDECODINGERROR;
        goto cleanup;
    }
    memcpy(dstData, output, actualLength);
    dst->data = (u8*)dstData;
    dst->length = actualLength;
    
    if(moveToken)
        (*parseCtx->index)++; // String is one element

    cleanup:
        free(output);
    
    return ret;
}



DECODE_JSON(LocalizedText) {
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    const char* fieldNames[2] = {"Locale", "Text"};
    void *fieldPointer[2] = {&dst->locale, &dst->text};
    decodeJsonSignature functions[2] = {(decodeJsonSignature) String_decodeJson, (decodeJsonSignature) String_decodeJson};
    UA_Boolean found[2] = {UA_FALSE, UA_FALSE};
    DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 2};
    
    ret = decodeFields(ctx, parseCtx, &decodeCtx, type);
    return ret ;
}

DECODE_JSON(QualifiedName) {
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    UA_StatusCode ret = UA_STATUSCODE_GOOD;
    const char* fieldNames[] = {"Name", "Uri"};
    void *fieldPointer[] = {&dst->name, &dst->namespaceIndex};
    decodeJsonSignature functions[] = {(decodeJsonSignature) String_decodeJson, (decodeJsonSignature) UInt16_decodeJson};
    UA_Boolean found[2] = {UA_FALSE, UA_FALSE};
    DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 2};
    ret = decodeFields(ctx, parseCtx, &decodeCtx, type);
    return ret;
}

static status searchObjectForKeyRec(char* s, Ctx *ctx, ParseCtx *parseCtx, size_t *resultIndex, UA_UInt16 depth){
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
    
    //TODO: Better Status Code
    return UA_STATUSCODE_GOOD;
}

static status prepareDecodeNodeIdJson(UA_NodeId *dst, Ctx *ctx, ParseCtx *parseCtx, u8 *fieldCount, const char* fieldNames[], decodeJsonSignature *functions, void* fieldPointer[]){
    
    /* possible keys */
    char* idString = "Id";
    char* idTypeString = "IdType";
    
       /* Id must alway be present */
    fieldNames[*fieldCount] = idString;
    
    /* IdType */
    UA_Boolean hasIdType = UA_FALSE;
    size_t searchResult = 0;
    UA_String searchKey = UA_STRING("IdType"); 
    lookAheadForKey(searchKey, ctx, parseCtx, &searchResult);
    if(searchResult != 0){
         hasIdType = UA_TRUE;
    }
    
    
    if(hasIdType){
        
        size_t size = (size_t)(parseCtx->tokenArray[searchResult].end - parseCtx->tokenArray[searchResult].start);
        if(size < 1){
            return UA_STATUSCODE_BADDECODINGERROR;
        }

        char *idType = (char*)(ctx->pos + parseCtx->tokenArray[searchResult].start);
      
        if(idType[0] == '2'){
            dst->identifierType = UA_NODEIDTYPE_GUID;
            fieldPointer[*fieldCount] = &dst->identifier.guid;
            functions[*fieldCount] = (decodeJsonSignature) Guid_decodeJson;
        }else if(idType[0] == '1'){
            dst->identifierType = UA_NODEIDTYPE_STRING;
            fieldPointer[*fieldCount] = &dst->identifier.string;
            functions[*fieldCount] = (decodeJsonSignature) String_decodeJson;
        }else if(idType[0] == '3'){
            dst->identifierType = UA_NODEIDTYPE_BYTESTRING;
            fieldPointer[*fieldCount] = &dst->identifier.byteString;
            functions[*fieldCount] = (decodeJsonSignature) ByteString_decodeJson;
        }else{
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        //Id alway present
        (*fieldCount)++;
        
        fieldNames[*fieldCount] = idTypeString;
        fieldPointer[*fieldCount] = NULL;
        functions[*fieldCount] = NULL;
        
        //IdType
        (*fieldCount)++;
    }else{
        dst->identifierType = UA_NODEIDTYPE_NUMERIC;
        fieldPointer[*fieldCount] = &dst->identifier.numeric;
        functions[*fieldCount] = (decodeJsonSignature) UInt32_decodeJson;
        
        //Id alway present
       (*fieldCount)++;  
    }
    
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(NodeId) {
    
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    /* NameSpace */
    char* namespaceString = "Namespace";
    UA_Boolean hasNamespace = UA_FALSE;
    size_t searchResultNamespace = 0;
    UA_String searchKeyNamespace = UA_STRING(namespaceString);    
    lookAheadForKey(searchKeyNamespace, ctx, parseCtx, &searchResultNamespace);
    if(searchResultNamespace == 0){
        dst->namespaceIndex = 0;
    } else{
        hasNamespace = UA_TRUE;
    }
    
    /* Setup fields, max 3 keys */
    const char* fieldNames[3];
    decodeJsonSignature functions[3];
    void *fieldPointer[3];
    
    /* Keep track over number of keys present, incremented if key found */
    u8 fieldCount = 0;
    
    prepareDecodeNodeIdJson(dst, ctx, parseCtx, &fieldCount, fieldNames, functions, fieldPointer);
    
    if(hasNamespace){
      fieldNames[fieldCount] = namespaceString;
      fieldPointer[fieldCount] = &dst->namespaceIndex;
      functions[fieldCount] = (decodeJsonSignature) UInt16_decodeJson;
      fieldCount++;  
    }
    
    UA_Boolean found[3] = {UA_FALSE, UA_FALSE, UA_FALSE};
    DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 3};
    status ret = decodeFields(ctx, parseCtx, &decodeCtx, type);
    return ret;
}

DECODE_JSON(ExpandedNodeId) {
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    /* possible keys */
    //char* idString = "Id";
    char* namespaceString = "Namespace";
    char* serverUriString = "ServerUri";
    //char* idTypeString = "IdType";
    
    /* Keep track over number of keys present, incremented if key found */
    u8 fieldCount = 0;
    
    /* ServerUri */
    UA_Boolean hasServerUri = UA_FALSE;
    size_t searchResultServerUri = 0;
    UA_String searchKeyServerUri = UA_STRING("ServerUri");   
    lookAheadForKey(searchKeyServerUri, ctx, parseCtx, &searchResultServerUri);
    if(searchResultServerUri == 0){
        //TODO: Lookup
        dst->namespaceUri = UA_STRING(""); 
    } else{
        hasServerUri = UA_TRUE;
    }
    
    /* NameSpace */
    UA_Boolean hasNamespace = UA_FALSE;
    size_t searchResultNamespace = 0;
    UA_String searchKeyNamespace = UA_STRING("Namespace");    
    lookAheadForKey(searchKeyNamespace, ctx, parseCtx, &searchResultNamespace);
    if(searchResultNamespace == 0){
        dst->serverIndex = 0;
    } else{
        hasNamespace = UA_TRUE;
    }
    
    /* Setup fields, max 4 keys */
    const char* fieldNames[4];
    decodeJsonSignature functions[4];
    void *fieldPointer[4];
    
    prepareDecodeNodeIdJson(&dst->nodeId, ctx, parseCtx, &fieldCount, fieldNames, functions, fieldPointer);
    
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
    
    UA_Boolean found[4] = {UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE};
    DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, fieldCount};
    status ret = decodeFields(ctx, parseCtx, &decodeCtx, type);
    
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
    
    //TODO: proper ISO 8601:2004 parsing, musl strptime!
    //DateTime  ISO 8601:2004 without milli is 20 Characters, with millis 24
    if(size != 20 && size != 24){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    //sanity check
    if(input[4] != '-' || input[7] != '-' || input[10] != 'T' 
            || input[13] != ':' || input[16] != ':' || !(input[19] == 'Z' || input[19] == '.')){
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
    
    UA_UInt64 msec = 0;
    if(size == 24){
        UA_atoi(&input[20], 3, &msec);
    }
    
    long long sinceunix = __tm_to_secs(&dts);
    UA_DateTime dt = (UA_DateTime)((UA_UInt64)(sinceunix*UA_DATETIME_SEC + UA_DATETIME_UNIX_EPOCH) + (UA_UInt64)(UA_DATETIME_MSEC * msec)); //TODO msec
    memcpy(dst, &dt, 8);
  
    if(moveToken)
        (*parseCtx->index)++; // DateTime is one element
    return UA_STATUSCODE_GOOD;
}

DECODE_JSON(StatusCode) {
    UA_UInt32 d;
    status ret = DECODE_DIRECT(&d, UInt32);
    if(ret != UA_STATUSCODE_GOOD)
        return ret;
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
    status ret = UA_STATUSCODE_GOOD;
    
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    size_t searchResultType = 0;
    UA_String searchKeyType = UA_STRING("Type");
 
    lookAheadForKey(searchKeyType, ctx, parseCtx, &searchResultType);
    
    //TODO: Better way of not found condition.
    if(searchResultType != 0){  
        size_t size = (size_t)(parseCtx->tokenArray[searchResultType].end - parseCtx->tokenArray[searchResultType].start);
        if(size < 1){
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        /* Does the variant contain an array? */
        UA_Boolean isArray = UA_FALSE;
        
        UA_Boolean hasDimension = UA_FALSE;
        
        //Is the Body an Array?
        size_t searchResultBody = 0;
        UA_String searchKeyBody = UA_STRING("Body");
        lookAheadForKey(searchKeyBody, ctx, parseCtx, &searchResultBody);
        if(searchResultBody != 0){
            jsmntok_t bodyToken = parseCtx->tokenArray[searchResultBody];
            if(bodyToken.type == JSMN_ARRAY){
                isArray = UA_TRUE;
                
                size_t arraySize = 0;
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
            size_t dimensionSize = 0;
            dimensionSize = (size_t)parseCtx->tokenArray[searchResultDim].size;
            dst->arrayDimensionsSize = dimensionSize;
        }

        //Parse the type
        UA_UInt64 idTypeDecoded;
        char *idTypeEncoded = (char*)(ctx->pos + parseCtx->tokenArray[searchResultType].start);
        UA_atoi(idTypeEncoded, size, &idTypeDecoded);
        
        /* Get the datatype of the content. The type must be a builtin data type.
        * All not-builtin types are wrapped in an ExtensionObject. */
        if(idTypeDecoded > UA_TYPES_DIAGNOSTICINFO)
            return UA_STATUSCODE_BADDECODINGERROR;

        /* A variant cannot contain a variant. But it can contain an array of
            * variants */
        if(idTypeDecoded == UA_TYPES_VARIANT && !isArray)
            return UA_STATUSCODE_BADDECODINGERROR;
        
        
        //Set the type, TODO: Get the Type by nodeID!
        UA_NodeId typeNodeId = UA_NODEID_NUMERIC(0, (UA_UInt32)idTypeDecoded);
        const UA_DataType *BodyType = UA_findDataType(&typeNodeId);
        
        if(BodyType == NULL){
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        //const UA_DataType *BodyType = &dataTypeByTypeNodeId;
        dst->type = BodyType;
        
        if(isArray){
              if(!hasDimension){
                const char* fieldNames[] = {"Type", "Body"};

                void *fieldPointer[] = {NULL, &dst->data};
                decodeJsonSignature functions[] = {NULL, (decodeJsonSignature) Array_decodeJson};
                UA_Boolean found[] = {UA_FALSE, UA_FALSE};
                DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 2};
                ret = decodeFields(ctx, parseCtx, &decodeCtx, BodyType);
            }else{
                const char* fieldNames[] = {"Type", "Body", "Dimension"};
                void *fieldPointer[] = {NULL, &dst->data, &dst->arrayDimensions};
                decodeJsonSignature functions[] = {NULL, (decodeJsonSignature) Array_decodeJson, (decodeJsonSignature) VariantDimension_decodeJson};
                UA_Boolean found[] = {UA_FALSE, UA_FALSE, UA_FALSE};
                DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 3};
                ret = decodeFields(ctx, parseCtx, &decodeCtx, BodyType);
            }
            
        }else if(idTypeDecoded != UA_TYPES_EXTENSIONOBJECT){
            //Allocate Memory for Body
            void* bodyPointer = UA_new(BodyType);
            memcpy(&dst->data, &bodyPointer, sizeof(void*)); //Copy new Pointer do dest
            
            const char* fieldNames[] = {"Type", "Body"};
            void *fieldPointer[] = {NULL, bodyPointer};
            decodeJsonSignature functions[] = {NULL, (decodeJsonSignature) decodeJsonInternal};
            UA_Boolean found[] = {UA_FALSE, UA_FALSE};
            DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 2};
            ret = decodeFields(ctx, parseCtx, &decodeCtx, BodyType);
        }else {
            const char* fieldNames[] = {"Type", "Body"};
            void *fieldPointer[] = {NULL, dst};
            decodeJsonSignature functions[] = {NULL, (decodeJsonSignature) Variant_decodeJsonUnwrapExtensionObject};
            UA_Boolean found[] = {UA_FALSE, UA_FALSE};
            //ret = Variant_decodeJsonUnwrapExtensionObject(dst, ctx, parseCtx, isArray, hasDimension, searchResultBody);
            DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 2};
            ret = decodeFields(ctx, parseCtx, &decodeCtx, BodyType);
        }
    }
    
    return ret;
}

DECODE_JSON(DataValue) {
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        if(isJsonNull(ctx, parseCtx)){
            //TODO set datavalue NULL
            dst = NULL;
            return UA_STATUSCODE_GOOD;
        }
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    status ret = UA_STATUSCODE_GOOD;
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
    DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 6};
    ret = decodeFields(ctx, parseCtx, &decodeCtx, type);
    dst->hasValue = found[0];
    dst->hasStatus = found[1];
    dst->hasSourceTimestamp = found[2];
    dst->hasSourcePicoseconds = found[3];
    dst->hasServerTimestamp = found[4];
    dst->hasServerPicoseconds = found[5];
    return ret;
}

DECODE_JSON(ExtensionObject) {
    if(isJsonNull(ctx, parseCtx)){
        /* 
        * TODO:
        * If the Body is empty, the ExtensionObject is NULL and is omitted or encoded as a JSON null.
        */   
        dst = NULL;
        return UA_STATUSCODE_GOOD;
    }
    
    if(getJsmnType(parseCtx) != JSMN_OBJECT){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    status ret = UA_STATUSCODE_GOOD;
    
    
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
        ret = NodeId_decodeJson(&typeId, &UA_TYPES[UA_TYPES_NODEID], ctx, parseCtx, UA_TRUE);
        if(ret != UA_STATUSCODE_GOOD)
            return ret;
        
        //restore
        *parseCtx->index = index;
        
        //TODO: OR this one??? const UA_DataType *typeOfBody = UA_findDataTypeByBinaryInternal(&typeId, ctx);
        const UA_DataType *typeOfBody = UA_findDataType(&typeId);
        
        if(!typeOfBody){
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        //Set Found Type
        dst->content.decoded.type = typeOfBody;
        
        
        if( searchTypeIdResult != 0){
            
            const char* fieldNames[] = {"TypeId", "Body"};
            UA_Boolean found[] = {UA_FALSE, UA_FALSE};
            dst->content.decoded.data = UA_new(type);
            if(!dst->content.decoded.data)
                return UA_STATUSCODE_BADOUTOFMEMORY;
            
            //TODO: This has to be parsed regular to step over tokens in Object
            UA_NodeId typeId_dummy;
            void *fieldPointer[] = {
                &typeId_dummy, 
                dst->content.decoded.data
            };

            size_t decode_index = typeOfBody->builtin ? typeOfBody->typeIndex : UA_BUILTIN_TYPES_COUNT;
            decodeJsonSignature functions[] = {
                (decodeJsonSignature) NodeId_decodeJson, 
                decodeJsonJumpTable[decode_index]};

            DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 2};
            return decodeFields(ctx, parseCtx, &decodeCtx, typeOfBody);
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
            UA_Boolean found[] = {UA_FALSE, UA_FALSE, UA_FALSE};
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
            
            DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 3};
            return decodeFields(ctx, parseCtx, &decodeCtx, type);
        } else if(encoding == 2) {
            /* XmlElement in Json Body */
            dst->encoding = UA_EXTENSIONOBJECT_ENCODED_XML;
            
            const char* fieldNames[] = {"Encoding", "Body", "TypeId"};
            UA_Boolean found[] = {UA_FALSE, UA_FALSE, UA_FALSE};
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
            
            DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 3};
            return decodeFields(ctx, parseCtx, &decodeCtx, type);
        } else {
            //UA_NodeId_deleteMembers(&typeId);
            return UA_STATUSCODE_BADDECODINGERROR;
        }
    }
    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

static status
Variant_decodeJsonUnwrapExtensionObject(UA_Variant *dst, const UA_DataType *type, Ctx *ctx, ParseCtx *parseCtx, UA_Boolean moveToken) {
    /* Save the position in the ByteString. If unwrapping is not possible, start
     * from here to decode a normal ExtensionObject. */
    UA_UInt16 old_index = *parseCtx->index;
    
    status ret = UA_STATUSCODE_GOOD;
    
    /* Decode the DataType */
    UA_NodeId typeId;
    UA_NodeId_init(&typeId);
    {
        size_t searchTypeIdResult = 0;
        UA_String searchTypeIdKey = UA_STRING("TypeId");
        lookAheadForKey(searchTypeIdKey, ctx, parseCtx, &searchTypeIdResult);  

        if(searchTypeIdResult == 0){
            //No Typeid if extensionobject found, error
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        /* parse the nodeid */
        *parseCtx->index = (UA_UInt16)searchTypeIdResult;
        ret = NodeId_decodeJson(&typeId, &UA_TYPES[UA_TYPES_NODEID], ctx, parseCtx, UA_TRUE);
        if(ret != UA_STATUSCODE_GOOD){
            return UA_STATUSCODE_BADDECODINGERROR;
        }
        
        //restore index, Variant position
        *parseCtx->index = old_index;
    }

    /* ---Decode the EncodingByte--- */
    UA_Boolean isStructure = UA_FALSE;
    {
        //Search for Encoding
        size_t searchEncodingResult = 0;
        UA_String searchEncodingKey = UA_STRING("Encoding");
        lookAheadForKey(searchEncodingKey, ctx, parseCtx, &searchEncodingResult);

        //restore, Variant
        *parseCtx->index = old_index;

        //If no encoding found it is Structure encoding
        if(searchEncodingResult == 0){
            isStructure = UA_TRUE;
        }
    }

    const UA_DataType *typeOfBody = UA_findDataType(&typeId);
    //const UA_DataType *typeOfBody = UA_findDataTypeByBinaryInternal(&typeId, ctx);
    
    if(typeOfBody != NULL && isStructure){
        /* Found a valid type and it is structure encoded so it can be unwrapped */
        dst->type = typeOfBody;
        
        /* Allocate memory for type*/
        dst->data = UA_new(dst->type);
        if(!dst->data)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        
        /* Decode the content */
        size_t decode_index = dst->type->builtin ? dst->type->typeIndex : UA_BUILTIN_TYPES_COUNT;

        UA_NodeId dummy;
        const char* fieldNames[] = {"TypeId", "Body"};
        void *fieldPointer[] = {&dummy, dst->data};
        decodeJsonSignature functions[] = {(decodeJsonSignature) NodeId_decodeJson, decodeJsonJumpTable[decode_index]};
        UA_Boolean found[] = {UA_FALSE, UA_FALSE};
        
        DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 2};
        ret = decodeFields(ctx, parseCtx, &decodeCtx, type);

    }else{
        /* decode as ExtensionObject */
        dst->type = &UA_TYPES[UA_TYPES_EXTENSIONOBJECT];
        
        /* Allocate memory for extensionobject*/
        dst->data = UA_new(dst->type);
        if(!dst->data)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        
        ret = DECODE_DIRECT(dst->data, ExtensionObject);
    }
    //TODO Check?
    return ret;
}
status DiagnosticInfoInner_decodeJson(UA_DiagnosticInfo* dst, const UA_DataType* type, Ctx* ctx, ParseCtx* parseCtx);

DECODE_JSON(DiagnosticInfo) {
    status ret = UA_STATUSCODE_GOOD;
    const char* fieldNames[] = {"SymbolicId", "LocalizedText", "Locale", "AdditionalInfo", "InnerStatusCode", "InnerDiagnosticInfo"};
    void *fieldPointer[] = {&dst->symbolicId, &dst->localizedText, &dst->locale, &dst->additionalInfo, &dst->innerStatusCode, &dst->innerDiagnosticInfo};
    decodeJsonSignature functions[] = {(decodeJsonSignature) Int32_decodeJson, (decodeJsonSignature) Int32_decodeJson,(decodeJsonSignature) Int32_decodeJson,(decodeJsonSignature) String_decodeJson,(decodeJsonSignature) StatusCode_decodeJson, (decodeJsonSignature) DiagnosticInfoInner_decodeJson};
    UA_Boolean found[] = {UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE, UA_FALSE};
    DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, 6};
    ret = decodeFields(ctx, parseCtx, &decodeCtx, type);
    dst->hasSymbolicId = found[0];
    dst->hasLocalizedText = found[1];
    dst->hasLocale = found[2];
    dst->hasAdditionalInfo = found[3];
    dst->hasInnerStatusCode = found[4];
    dst->hasInnerDiagnosticInfo = found[5];
    return ret;
}

status DiagnosticInfoInner_decodeJson(UA_DiagnosticInfo* dst, const UA_DataType* type, Ctx* ctx, ParseCtx* parseCtx){
    UA_DiagnosticInfo *inner = (UA_DiagnosticInfo*)UA_calloc(1, sizeof(UA_DiagnosticInfo));
    if(inner == NULL){
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    memcpy(dst, &inner, sizeof(UA_DiagnosticInfo*)); //Copy new Pointer do dest
    return DiagnosticInfo_decodeJson(inner, type, ctx, parseCtx, UA_TRUE);
}

status 
decodeFields(/*TODO const*/ Ctx *ctx, ParseCtx *parseCtx, DecodeContext *decodeContext, const UA_DataType *type) {
    size_t objectCount = (size_t)(parseCtx->tokenArray[(*parseCtx->index)].size);
    status ret = UA_STATUSCODE_GOOD;
    
    if(decodeContext->memberSize == 1){ // TODO
        if(*decodeContext->fieldNames[0]  == 0){ //No MemberName
            return decodeContext->functions[0](decodeContext->fieldPointer[0], type, ctx, parseCtx, UA_TRUE); //ENCODE DIRECT
        }
    }else if(decodeContext->memberSize == 0){
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    (*parseCtx->index)++; //go to first key

    for (size_t currentObjectCount = 0; currentObjectCount < objectCount && *parseCtx->index < parseCtx->tokenCount; currentObjectCount++) {

        // start searching at the index of currentObjectCount
        for (size_t i = currentObjectCount; i < decodeContext->memberSize + currentObjectCount; i++) {
            //Search for KEY, if found outer loop will be one less. Best case is objectCount if in order!
            
            size_t index = i % decodeContext->memberSize;
            
            if (jsoneq((char*) ctx->pos, &parseCtx->tokenArray[*parseCtx->index], decodeContext->fieldNames[index]) != 0)
                continue;

            if (decodeContext->found && decodeContext->found[index]) {
                //Duplicate Key found, abort.
                return UA_STATUSCODE_BADDECODINGERROR;
            }
            if (decodeContext->found != NULL) {
                decodeContext->found[index] = UA_TRUE;
            }

            (*parseCtx->index)++; //goto value
            if (decodeContext->functions[index] != NULL) {
                ret = decodeContext->functions[index](decodeContext->fieldPointer[index], type, ctx, parseCtx, UA_TRUE); //Move Token True
                if (ret != UA_STATUSCODE_GOOD) {
                    return ret;
                }
            } else {
                //TODO overstep single value, this will not work if object or array
                //Only used not to double parse pre looked up type, but it has to be overstepped
                (*parseCtx->index)++;
            }

            break;
        }
    }

    /* TODO needed? DataValue with missing
     * if(memberSize != foundCount){
        return UA_STATUSCODE_BADDECODINGERROR;
    }*/
    return ret;
}

decodeJsonSignature getDecodeSignature(u8 index){
    return decodeJsonJumpTable[index];
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
    (decodeJsonSignature)Float_decodeJson,
    (decodeJsonSignature)Double_decodeJson,
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
    if(length == 0) {
        dst = UA_EMPTY_ARRAY_SENTINEL;
        return UA_STATUSCODE_GOOD;
    }

    /* Allocate memory */
    void* mem = UA_calloc(length, type->memSize);
    if(dst == NULL)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    
    memcpy(dst, &mem, sizeof(void*)); //Copy new Pointer do dest
    
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
    UA_STACKARRAY(const char*, fieldNames, membersSize);
    UA_STACKARRAY(void *, fieldPointer, membersSize);
    UA_STACKARRAY(decodeJsonSignature, functions, membersSize);
    UA_STACKARRAY(UA_Boolean, found, membersSize);
    memset(found, 0, membersSize);
    
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
    
    DecodeContext decodeCtx = {fieldNames, fieldPointer, functions, found, membersSize};
    ret = decodeFields(ctx, parseCtx, &decodeCtx, type);

    ctx->depth--;
    return ret;
}

status tokenize(ParseCtx *parseCtx, Ctx *ctx, const UA_ByteString *src, UA_UInt16 *tokenIndex){
     /* Set up the context */
    ctx->pos = &src->data[0];
    ctx->end = &src->data[src->length];
    ctx->depth = 0;
    parseCtx->tokenCount = 0;
    parseCtx->index = tokenIndex;

    //Set up tokenizer jsmn
    jsmn_parser p;
    jsmn_init(&p);
    parseCtx->tokenCount = (UA_Int32)jsmn_parse(&p, (char*)src->data, src->length, parseCtx->tokenArray, TOKENCOUNT);
    
    if (parseCtx->tokenCount < 0) {
        return UA_STATUSCODE_BADDECODINGERROR;
    }
    
    return UA_STATUSCODE_GOOD;
}

status
UA_decodeJson(const UA_ByteString *src, size_t *offset, void *dst,
                const UA_DataType *type, size_t customTypesSize,
                const UA_DataType *customTypes) {
    /* Set up the context */
    Ctx ctx;
    ParseCtx parseCtx;
    parseCtx.tokenArray = (jsmntok_t*)malloc(sizeof(jsmntok_t) * TOKENCOUNT);
    memset(parseCtx.tokenArray, 0, sizeof(jsmntok_t) * TOKENCOUNT);
    ctx.customTypesArraySize = customTypesSize;
    ctx.customTypesArray = customTypes;

    UA_UInt16 tokenIndex = 0;
    status ret = tokenize(&parseCtx, &ctx, src, &tokenIndex);
    if(ret != UA_STATUSCODE_GOOD){
        return ret;
    }

    /* Assume the top-level element is an object */
    if (parseCtx.tokenCount < 1 || parseCtx.tokenArray[0].type != JSMN_OBJECT) {
        //printf("Object expected\n");
        
        if(parseCtx.tokenCount == 1){
            if(parseCtx.tokenArray[0].type == JSMN_PRIMITIVE || parseCtx.tokenArray[0].type == JSMN_STRING){
                            /* Decode */
               memset(dst, 0, type->memSize); /* Initialize the value */
               ret = decodeJsonInternal(dst, type, &ctx, &parseCtx, UA_TRUE);
               goto cleanup;
            }
        }
        
        ret = UA_STATUSCODE_BADDECODINGERROR;
        goto cleanup;
    }

    /* Decode */
    memset(dst, 0, type->memSize); /* Initialize the value */
    ret = decodeJsonInternal(dst, type, &ctx, &parseCtx, UA_TRUE);

    cleanup:
    free(parseCtx.tokenArray);
    return ret;
}