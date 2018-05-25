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

    UA_exchangeEncodeBuffer exchangeBufferCallback;
    void *exchangeBufferCallbackHandle;
} Ctx;
    

status writeKey(Ctx *ctx, const char* key);
status encodingJsonStartObject(Ctx *ctx);
size_t encodingJsonEndObject(Ctx *ctx);
status encodingJsonStartArray(Ctx *ctx);
size_t encodingJsonEndArray(Ctx *ctx);
status writeComma(Ctx *ctx);



typedef struct {
    jsmntok_t tokenArray[128];
    UA_Int32 tokenCount;
    UA_UInt16 *index;
} ParseCtx;

typedef status(*encodeJsonSignature)(const void *UA_RESTRICT src, const UA_DataType *type,
        Ctx *UA_RESTRICT ctx);
typedef status (*decodeJsonSignature)(void *UA_RESTRICT dst, const UA_DataType *type,
                                        Ctx *UA_RESTRICT ctx, ParseCtx *parseCtx, UA_Boolean moveToken);

status 
decodeFields(Ctx *ctx, ParseCtx *parseCtx, u8 memberSize, const char* fieldNames[], decodeJsonSignature functions[], void *fieldPointer[], const UA_DataType *type, UA_Boolean found[]);

/* workaround: TODO generate functions for UA_xxx_decodeJson */
decodeJsonSignature getDecodeSignature(u8 index);
status lookAheadForKey(UA_String search, Ctx *ctx, ParseCtx *parseCtx, size_t *resultIndex);

jsmntype_t getJsmnType(const ParseCtx *parseCtx);
status tokenize(ParseCtx *parseCtx, Ctx *ctx, const UA_ByteString *src, UA_UInt16 *tokenIndex);
//typedef UA_StatusCode (*UA_exchangeEncodeBuffer)(void *handle, UA_Byte **bufPos,
//                                                 const UA_Byte **bufEnd);

/* Encodes the scalar value described by type in the binary encoding. Encoding
 * is thread-safe if thread-local variables are enabled. Encoding is also
 * reentrant and can be safely called from signal handlers or interrupts.
 *
 * @param src The value. Must not be NULL.
 * @param type The value type. Must not be NULL.
 * @param bufPos Points to a pointer to the current position in the encoding
 *        buffer. Must not be NULL. The pointer is advanced by the number of
 *        encoded bytes, or, if the buffer is exchanged, to the position in the
 *        new buffer.
 * @param bufEnd Points to a pointer to the end of the encoding buffer (encoding
 *        always stops before *buf_end). Must not be NULL. The pointer is
 *        changed when the buffer is exchanged.
 * @param exchangeCallback Called when the end of the buffer is reached. This is
          used to send out a message chunk before continuing with the encoding.
          Is ignored if NULL.
 * @param exchangeHandle Custom data passed into the exchangeCallback.
 * @return Returns a statuscode whether encoding succeeded. */
UA_StatusCode
UA_encodeJson(const void *src, const UA_DataType *type,
                UA_Byte **bufPos, const UA_Byte **bufEnd,
                UA_exchangeEncodeBuffer exchangeCallback,
                void *exchangeHandle) UA_FUNC_ATTR_WARN_UNUSED_RESULT;

/* Decodes a scalar value described by type from binary encoding. Decoding
 * is thread-safe if thread-local variables are enabled. Decoding is also
 * reentrant and can be safely called from signal handlers or interrupts.
 *
 * @param src The buffer with the binary encoded value. Must not be NULL.
 * @param offset The current position in the buffer. Must not be NULL. The value
 *        is advanced as decoding progresses.
 * @param dst The target value. Must not be NULL. The target is assumed to have
 *        size type->memSize. The value is reset to zero before decoding. If
 *        decoding fails, members are deleted and the value is reset (zeroed)
 *        again.
 * @param type The value type. Must not be NULL.
 * @param customTypesSize The number of non-standard datatypes contained in the
 *        customTypes array.
 * @param customTypes An array of non-standard datatypes (not included in
 *        UA_TYPES). Can be NULL if customTypesSize is zero.
 * @return Returns a statuscode whether decoding succeeded. */
UA_StatusCode
UA_decodeJson(const UA_ByteString *src, size_t *offset, void *dst,
                const UA_DataType *type, size_t customTypesSize,
                const UA_DataType *customTypes) UA_FUNC_ATTR_WARN_UNUSED_RESULT;

/* Returns the number of bytes the value p takes in binary encoding. Returns
 * zero if an error occurs. UA_calcSizeJson is thread-safe and reentrant since
 * it does not access global (thread-local) variables. */
size_t
UA_calcSizeJson(const void *p, const UA_DataType *type);

const UA_DataType *
UA_findDataTypeByJson(const UA_NodeId *typeId);

#ifdef __cplusplus
}
#endif

#endif /* UA_TYPES_ENCODING_JSON_H_ */
