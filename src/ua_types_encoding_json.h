/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *    Copyright 2014-2017 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 *    Copyright 2015 (c) Sten Grüner
 *    Copyright 2014, 2017 (c) Florian Palm
 *    Copyright 2017 (c) Stefan Profanter, fortiss GmbH
 *    Copyright 2017 (c) Mark Giraud, Fraunhofer IOSB
 */

#ifndef UA_TYPES_ENCODING_JSON_H_
#define UA_TYPES_ENCODING_JSON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_util.h"
#include "ua_types_encoding_binary.h"
#include "ua_types_encoding_json.h"
#include "ua_types.h"
#include "../deps/jsmn/jsmn.h"

typedef struct {
    /* Pointers to the current position and the last position in the buffer */
    u8 *pos;
    const u8 *end;

    u16 depth; /* How often did we en-/decoding recurse? */

    size_t customTypesArraySize;
    const UA_DataType *customTypesArray;

    size_t namespacesSize;
    UA_String *namespaces;
    
    UA_exchangeEncodeBuffer exchangeBufferCallback;
    void *exchangeBufferCallbackHandle;
} Ctx;


status writeKey_UA_String(Ctx *ctx, UA_String *key, UA_Boolean commaNeeded);
status writeKey(Ctx *ctx, const char* key, UA_Boolean commaNeeded);
status encodingJsonStartObject(Ctx *ctx);
size_t encodingJsonEndObject(Ctx *ctx);
status encodingJsonStartArray(Ctx *ctx);
size_t encodingJsonEndArray(Ctx *ctx);
status writeComma(Ctx *ctx, UA_Boolean commaNeeded);
status writeNull(Ctx *ctx);

#define TOKENCOUNT 1000
typedef struct {
    jsmntok_t *tokenArray;
    UA_Int32 tokenCount;
    UA_UInt16 *index;
} ParseCtx;

typedef status(*encodeJsonSignature)(const void *UA_RESTRICT src, const UA_DataType *type,
        Ctx *UA_RESTRICT ctx, UA_Boolean useReversible);
typedef status (*decodeJsonSignature)(void *UA_RESTRICT dst, const UA_DataType *type,
                                        Ctx *UA_RESTRICT ctx, ParseCtx *parseCtx, UA_Boolean moveToken);

typedef struct {
    const char ** fieldNames;
    void ** fieldPointer;
    decodeJsonSignature * functions;
    UA_Boolean * found;
    u8 memberSize;
} DecodeContext;


status 
decodeFields(Ctx *ctx, ParseCtx *parseCtx, DecodeContext *decodeContext, const UA_DataType *type);

/* workaround: TODO generate functions for UA_xxx_decodeJson */
decodeJsonSignature getDecodeSignature(u8 index);
status lookAheadForKey(UA_String search, Ctx *ctx, ParseCtx *parseCtx, size_t *resultIndex);

jsmntype_t getJsmnType(const ParseCtx *parseCtx);
status tokenize(ParseCtx *parseCtx, Ctx *ctx, const UA_ByteString *src, UA_UInt16 *tokenIndex);
UA_Boolean isJsonNull(const Ctx *ctx, const ParseCtx *parseCtx);

status
UA_encodeJson(const void *src, const UA_DataType *type,
        u8 **bufPos, 
        const u8 **bufEnd, 
        UA_String *namespaces, 
        size_t namespaceSize, 
        UA_Boolean useReversible) UA_FUNC_ATTR_WARN_UNUSED_RESULT;

UA_StatusCode
UA_decodeJson(const UA_ByteString *src, size_t *offset, void *dst,
                const UA_DataType *type, size_t customTypesSize,
                const UA_DataType *customTypes) UA_FUNC_ATTR_WARN_UNUSED_RESULT;

size_t
UA_calcSizeJson(const void *p, const UA_DataType *type);

#ifdef __cplusplus
}
#endif

#endif /* UA_TYPES_ENCODING_JSON_H_ */
