/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "ua_types.h"
#include "ua_types_encoding_binary.h"
#include "ua_types_encoding_json.h"
#include "ua_types_generated.h"
#include "ua_types_generated_handling.h"
#include "ua_types_generated_encoding_binary.h"
#include "ua_util.h"
#include "check.h"

/* copied here from encoding_binary.c */
enum UA_VARIANT_ENCODINGMASKTYPE_enum {
    UA_VARIANT_ENCODINGMASKTYPE_TYPEID_MASK = 0x3F,            // bits 0:5
    UA_VARIANT_ENCODINGMASKTYPE_DIMENSIONS  = (0x01 << 6),     // bit 6
    UA_VARIANT_ENCODINGMASKTYPE_ARRAY       = (0x01 << 7)      // bit 7
};

START_TEST(UA_Byte_decodeShallCopyAndAdvancePosition) {
    // given
    UA_Byte dst;
    UA_Byte data[] = { 0x08 };
    UA_ByteString src = { 1, data };
    size_t pos = 0;

    // when
    UA_StatusCode retval = UA_Byte_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(pos, 1);
    ck_assert_uint_eq(pos, UA_calcSizeBinary(&dst, &UA_TYPES[UA_TYPES_BYTE]));
    ck_assert_uint_eq(dst, 0x08);
}
END_TEST

START_TEST(UA_Byte_decodeShallModifyOnlyCurrentPosition) {
    // given
    UA_Byte dst[]  = { 0xFF, 0xFF, 0xFF };
    UA_Byte data[] = { 0x08 };
    UA_ByteString src = { 1, data };
    size_t pos = 0;
    // when
    UA_StatusCode retval = UA_Byte_decodeBinary(&src, &pos, &dst[1]);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 1);
    ck_assert_uint_eq(dst[0], 0xFF);
    ck_assert_uint_eq(dst[1], 0x08);
    ck_assert_uint_eq(dst[2], 0xFF);
}
END_TEST

START_TEST(UA_Int16_decodeShallAssumeLittleEndian) {
    // given
    size_t pos = 0;
    UA_Byte data[] = {
            0x01, 0x00,     // 1
            0x00, 0x01      // 256
    };
    UA_ByteString src = { 4, data };
    // when
    UA_Int16 val_01_00, val_00_01;
    UA_StatusCode retval = UA_Int16_decodeBinary(&src, &pos, &val_01_00);
    retval |= UA_Int16_decodeBinary(&src, &pos, &val_00_01);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(val_01_00, 1);
    ck_assert_int_eq(val_00_01, 256);
    ck_assert_int_eq(pos, 4);
}
END_TEST

START_TEST(UA_Int16_decodeShallRespectSign) {
    // given
    size_t pos = 0;
    UA_Byte data[] = {
            0xFF, 0xFF,     // -1
            0x00, 0x80      // -32768
    };
    UA_ByteString src = { 4, data };
    // when
    UA_Int16 val_ff_ff, val_00_80;
    UA_StatusCode retval = UA_Int16_decodeBinary(&src, &pos, &val_ff_ff);
    retval |= UA_Int16_decodeBinary(&src, &pos, &val_00_80);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(val_ff_ff, -1);
    ck_assert_int_eq(val_00_80, -32768);
}
END_TEST

START_TEST(UA_UInt16_decodeShallNotRespectSign) {
    // given
    size_t pos = 0;
    UA_Byte data[] = {
            0xFF, 0xFF,     // (2^16)-1
            0x00, 0x80      // (2^15)
    };
    UA_ByteString src = { 4, data };
    // when
    UA_UInt16     val_ff_ff, val_00_80;
    UA_StatusCode retval = UA_UInt16_decodeBinary(&src, &pos, &val_ff_ff);
    retval |= UA_UInt16_decodeBinary(&src, &pos, &val_00_80);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 4);
    ck_assert_uint_eq(val_ff_ff, (0x01 << 16)-1);
    ck_assert_uint_eq(val_00_80, (0x01 << 15));
}
END_TEST

START_TEST(UA_Int32_decodeShallAssumeLittleEndian) {
    // given
    size_t pos = 0;
    UA_Byte data[] = {
            0x01, 0x00, 0x00, 0x00,     // 1
            0x00, 0x01, 0x00, 0x00      // 256
    };
    UA_ByteString src = { 8, data };

    // when
    UA_Int32 val_01_00, val_00_01;
    UA_StatusCode retval = UA_Int32_decodeBinary(&src, &pos, &val_01_00);
    retval |= UA_Int32_decodeBinary(&src, &pos, &val_00_01);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(val_01_00, 1);
    ck_assert_int_eq(val_00_01, 256);
    ck_assert_int_eq(pos, 8);
}
END_TEST

START_TEST(UA_Int32_decodeShallRespectSign) {
    // given
    size_t pos = 0;
    UA_Byte data[] = {
            0xFF, 0xFF, 0xFF, 0xFF,     // -1
            0x00, 0x80, 0xFF, 0xFF      // -32768
    };
    UA_ByteString src = { 8, data };

    // when
    UA_Int32 val_ff_ff, val_00_80;
    UA_StatusCode retval = UA_Int32_decodeBinary(&src, &pos, &val_ff_ff);
    retval |= UA_Int32_decodeBinary(&src, &pos, &val_00_80);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(val_ff_ff, -1);
    ck_assert_int_eq(val_00_80, -32768);
}
END_TEST

START_TEST(UA_UInt32_decodeShallNotRespectSign) {
    // given
    size_t pos = 0;
    UA_Byte data[] = {
            0xFF, 0xFF, 0xFF, 0xFF,     // (2^32)-1
            0x00, 0x00, 0x00, 0x80      // (2^31)
    };
    UA_ByteString src = { 8, data };

    // when
    UA_UInt32 val_ff_ff, val_00_80;
    UA_StatusCode retval = UA_UInt32_decodeBinary(&src, &pos, &val_ff_ff);
    retval |= UA_UInt32_decodeBinary(&src, &pos, &val_00_80);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 8);
    ck_assert_uint_eq(val_ff_ff, (UA_UInt32)( (0x01LL << 32 ) - 1 ));
    ck_assert_uint_eq(val_00_80, (UA_UInt32)(0x01) << 31);
}
END_TEST

START_TEST(UA_UInt64_decodeShallNotRespectSign) {
    // given
    UA_ByteString rawMessage;
    UA_UInt64     expectedVal = 0xFF;
    expectedVal = expectedVal << 56;
    UA_Byte mem[8] = { 00, 00, 00, 00, 0x00, 0x00, 0x00, 0xFF };
    rawMessage.data   = mem;
    rawMessage.length = 8;
    size_t pos = 0;
    UA_UInt64 val;
    // when
    UA_UInt64_decodeBinary(&rawMessage, &pos, &val);
    // then
    ck_assert_uint_eq(val, expectedVal);
}
END_TEST

START_TEST(UA_Int64_decodeShallRespectSign) {
    // given
    UA_ByteString rawMessage;
    UA_UInt64 expectedVal = (UA_UInt64)0xFF << 56;
    UA_Byte  mem[8]      = { 00, 00, 00, 00, 0x00, 0x00, 0x00, 0xFF };
    rawMessage.data   = mem;
    rawMessage.length = 8;

    size_t pos = 0;
    UA_Int64 val;
    // when
    UA_Int64_decodeBinary(&rawMessage, &pos, &val);
    //then
    ck_assert_uint_eq(val, expectedVal);
}
END_TEST

START_TEST(UA_Float_decodeShallWorkOnExample) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { 0x00, 0x00, 0xD0, 0xC0 }; // -6.5
    UA_ByteString src = { 4, data };
    UA_Float dst;
    // when
    UA_StatusCode retval = UA_Float_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 4);
    ck_assert(-6.5000001 < dst);
    ck_assert(dst < -6.49999999999);
}
END_TEST

START_TEST(UA_Double_decodeShallGiveOne) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F }; // 1
    UA_ByteString src = { 8, data }; // 1
    UA_Double dst;
    // when
    UA_StatusCode retval = UA_Double_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 8);
    ck_assert(0.9999999 < dst);
    ck_assert(dst < 1.00000000001);
}
END_TEST

START_TEST(UA_Double_decodeShallGiveZero) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    UA_ByteString src = { 8, data }; // 1
    UA_Double dst;
    // when
    UA_StatusCode retval = UA_Double_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 8);
    ck_assert(-0.00000001 < dst);
    ck_assert(dst < 0.000000001);
}
END_TEST

START_TEST(UA_Double_decodeShallGiveMinusTwo) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0 }; // -2
    UA_ByteString src = { 8, data };
    UA_Double dst;
    // when
    UA_StatusCode retval = UA_Double_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 8);
    ck_assert(-1.9999999 > dst);
    ck_assert(dst > -2.00000000001);
}
END_TEST

START_TEST(UA_Double_decodeShallGive2147483648) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x41 }; //2147483648
    UA_ByteString src = { 8, data }; // 1
    UA_Double dst;
    // when
    UA_StatusCode retval = UA_Double_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 8);
    ck_assert(2147483647.9999999 <= dst);
    ck_assert(dst <= 2147483648.00000001);
}
END_TEST

START_TEST(UA_String_decodeShallAllocateMemoryAndCopyString) {
    // given
    size_t pos = 0;
    UA_Byte data[] =
    { 0x08, 0x00, 0x00, 0x00, 'A', 'C', 'P', 'L', 'T', ' ', 'U', 'A', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    UA_ByteString src = { 16, data };
    UA_String dst;
    // when
    UA_StatusCode retval = UA_String_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(dst.length, 8);
    ck_assert_int_eq(dst.data[3], 'L');
    ck_assert_uint_eq(pos, UA_calcSizeBinary(&dst, &UA_TYPES[UA_TYPES_STRING]));
    // finally
    UA_String_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_String_decodeWithNegativeSizeShallNotAllocateMemoryAndNullPtr) {
    // given
    size_t pos = 0;
    UA_Byte data[] =
    { 0xFF, 0xFF, 0xFF, 0xFF, 'A', 'C', 'P', 'L', 'T', ' ', 'U', 'A', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    UA_ByteString src = { 16, data };
    UA_String dst;
    // when
    UA_StatusCode retval = UA_String_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(dst.length, 0);
    ck_assert_ptr_eq(dst.data, NULL);
}
END_TEST

START_TEST(UA_String_decodeWithZeroSizeShallNotAllocateMemoryAndNullPtr) {
    // given
    size_t pos = 0;
    UA_Byte data[] =
    { 0x00, 0x00, 0x00, 0x00, 'A', 'C', 'P', 'L', 'T', ' ', 'U', 'A', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    UA_ByteString src = { 17, data };
    UA_String dst;
    // when
    UA_StatusCode retval = UA_String_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(dst.length, 0);
    ck_assert_ptr_eq(dst.data, UA_EMPTY_ARRAY_SENTINEL);
}
END_TEST

START_TEST(UA_NodeId_decodeTwoByteShallReadTwoBytesAndSetNamespaceToZero) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { 0 /* UA_NODEIDTYPE_TWOBYTE */, 0x10 };
    UA_ByteString src    = { 2, data };
    UA_NodeId dst;
    // when
    UA_StatusCode retval = UA_NodeId_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 2);
    ck_assert_uint_eq(pos, UA_calcSizeBinary(&dst, &UA_TYPES[UA_TYPES_NODEID]));
    ck_assert_int_eq(dst.identifierType, UA_NODEIDTYPE_NUMERIC);
    ck_assert_int_eq(dst.identifier.numeric, 16);
    ck_assert_int_eq(dst.namespaceIndex, 0);
}
END_TEST

START_TEST(UA_NodeId_decodeFourByteShallReadFourBytesAndRespectNamespace) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { 1 /* UA_NODEIDTYPE_FOURBYTE */, 0x01, 0x00, 0x01 };
    UA_ByteString src = { 4, data };
    UA_NodeId dst;
    // when
    UA_StatusCode retval = UA_NodeId_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 4);
    ck_assert_uint_eq(pos, UA_calcSizeBinary(&dst, &UA_TYPES[UA_TYPES_NODEID]));
    ck_assert_int_eq(dst.identifierType, UA_NODEIDTYPE_NUMERIC);
    ck_assert_int_eq(dst.identifier.numeric, 256);
    ck_assert_int_eq(dst.namespaceIndex, 1);
}
END_TEST

START_TEST(UA_NodeId_decodeStringShallAllocateMemory) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { UA_NODEIDTYPE_STRING, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 'P', 'L', 'T' };
    UA_ByteString src = { 10, data };
    UA_NodeId dst;
    // when
    UA_StatusCode retval = UA_NodeId_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 10);
    ck_assert_uint_eq(pos, UA_calcSizeBinary(&dst, &UA_TYPES[UA_TYPES_NODEID]));
    ck_assert_int_eq(dst.identifierType, UA_NODEIDTYPE_STRING);
    ck_assert_int_eq(dst.namespaceIndex, 1);
    ck_assert_int_eq(dst.identifier.string.length, 3);
    ck_assert_int_eq(dst.identifier.string.data[1], 'L');
    // finally
    UA_NodeId_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_Variant_decodeWithOutArrayFlagSetShallSetVTAndAllocateMemoryForArray) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { (UA_Byte)UA_TYPES[UA_TYPES_INT32].typeId.identifier.numeric, 0xFF, 0x00, 0x00, 0x00 };
    UA_ByteString src = { 5, data };
    UA_Variant dst;
    // when
    UA_StatusCode retval = UA_Variant_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_uint_eq(pos, 5);
    ck_assert_uint_eq(pos, UA_calcSizeBinary(&dst, &UA_TYPES[UA_TYPES_VARIANT]));
    //ck_assert_ptr_eq((const void *)dst.type, (const void *)&UA_TYPES[UA_TYPES_INT32]); //does not compile in gcc 4.6
    ck_assert_int_eq((uintptr_t)dst.type, (uintptr_t)&UA_TYPES[UA_TYPES_INT32]);
    ck_assert_int_eq(dst.arrayLength, 0);
    ck_assert_int_ne((uintptr_t)dst.data, 0);
    UA_assert(dst.data != NULL); /* repeat the previous argument so that clang-analyzer is happy */
    ck_assert_int_eq(*(UA_Int32 *)dst.data, 255);
    // finally
    UA_Variant_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_Variant_decodeWithArrayFlagSetShallSetVTAndAllocateMemoryForArray) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { (UA_Byte)(UA_TYPES[UA_TYPES_INT32].typeId.identifier.numeric |
                                 UA_VARIANT_ENCODINGMASKTYPE_ARRAY),
                       0x02, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF,
                       0xFF, 0xFF };
    UA_ByteString src = { 13, data };
    UA_Variant dst;
    // when
    UA_StatusCode retval = UA_Variant_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(pos, 1+4+2*4);
    ck_assert_uint_eq(pos, UA_calcSizeBinary(&dst, &UA_TYPES[UA_TYPES_VARIANT]));
    //ck_assert_ptr_eq((const (void*))dst.type, (const void*)&UA_TYPES[UA_TYPES_INT32]); //does not compile in gcc 4.6
    ck_assert_int_eq((uintptr_t)dst.type,(uintptr_t)&UA_TYPES[UA_TYPES_INT32]);
    ck_assert_int_eq(dst.arrayLength, 2);
    ck_assert_int_eq(((UA_Int32 *)dst.data)[0], 255);
    ck_assert_int_eq(((UA_Int32 *)dst.data)[1], -1);
    // finally
    UA_Variant_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_Variant_decodeSingleExtensionObjectShallSetVTAndAllocateMemory){
    /* // given */
    /* size_t pos = 0; */
    /* UA_Variant dst; */
    /* UA_NodeId tmpNodeId; */

    /* UA_NodeId_init(&tmpNodeId); */
    /* tmpNodeId.identifier.numeric = 22; */
    /* tmpNodeId.namespaceIndex = 2; */
    /* tmpNodeId.identifierType = UA_NODEIDTYPE_NUMERIC; */

    /* UA_ExtensionObject tmpExtensionObject; */
    /* UA_ExtensionObject_init(&tmpExtensionObject); */
    /* tmpExtensionObject.encoding = UA_EXTENSIONOBJECT_ENCODED_BYTESTRING; */
    /* tmpExtensionObject.content.encoded.body = UA_ByteString_withSize(3); */
    /* tmpExtensionObject.content.encoded.body.data[0]= 10; */
    /* tmpExtensionObject.content.encoded.body.data[1]= 20; */
    /* tmpExtensionObject.content.encoded.body.data[2]= 30; */
    /* tmpExtensionObject.content.encoded.typeId = tmpNodeId; */

    /* UA_Variant tmpVariant; */
    /* UA_Variant_init(&tmpVariant); */
    /* tmpVariant.arrayDimensions = NULL; */
    /* tmpVariant.arrayDimensionsSize = -1; */
    /* tmpVariant.arrayLength = -1; */
    /* tmpVariant.storageType = UA_VARIANT_DATA_NODELETE; */
    /* tmpVariant.type = &UA_TYPES[UA_TYPES_EXTENSIONOBJECT]; */
    /* tmpVariant.data = &tmpExtensionObject; */

    /* UA_ByteString srcByteString = UA_ByteString_withSize(200); */
    /* pos = 0; */
    /* UA_Variant_encodeBinary(&tmpVariant,&srcByteString,pos); */

    /* // when */
    /* pos = 0; */
    /* UA_StatusCode retval = UA_Variant_decodeBinary(&srcByteString, &pos, &dst); */
    /* // then */
    /* ck_assert_int_eq(retval, UA_STATUSCODE_GOOD); */
    /* // TODO!! */
    /* /\* ck_assert_int_eq(dst.encoding, UA_EXTENSIONOBJECT_DECODED); *\/ */
    /* /\* ck_assert_int_eq((uintptr_t)dst.content.decoded.type, (uintptr_t)&UA_TYPES[UA_TYPES_EXTENSIONOBJECT]); *\/ */
    /* /\* ck_assert_int_eq(dst.arrayLength, -1); *\/ */
    /* /\* ck_assert_int_eq(((UA_ExtensionObject *)dst.data)->body.data[0], 10); *\/ */
    /* /\* ck_assert_int_eq(((UA_ExtensionObject *)dst.data)->body.data[1], 20); *\/ */
    /* /\* ck_assert_int_eq(((UA_ExtensionObject *)dst.data)->body.data[2], 30); *\/ */
    /* /\* ck_assert_int_eq(((UA_ExtensionObject *)dst.data)->body.length, 3); *\/ */


    /* // finally */
    /* UA_Variant_deleteMembers(&dst); */
    /* UA_ByteString_deleteMembers(&srcByteString); */
    /* UA_ExtensionObject_deleteMembers(&tmpExtensionObject); */

}
END_TEST

START_TEST(UA_Variant_decodeWithOutDeleteMembersShallFailInCheckMem) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { (UA_Byte)(UA_TYPES[UA_TYPES_INT32].typeId.identifier.numeric |
                                 UA_VARIANT_ENCODINGMASKTYPE_ARRAY),
                       0x02, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
    UA_ByteString src = { 13, data };
    UA_Variant dst;
    // when
    UA_StatusCode retval = UA_Variant_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    // finally
    UA_Variant_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_Variant_decodeWithTooSmallSourceShallReturnWithError) {
    // given
    size_t pos = 0;
    UA_Byte data[] = { (UA_Byte)(UA_TYPES[UA_TYPES_INT32].typeId.identifier.numeric |
                                 UA_VARIANT_ENCODINGMASKTYPE_ARRAY),
                       0x02, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
    UA_ByteString src = { 4, data };

    UA_Variant dst;
    // when
    UA_StatusCode retval = UA_Variant_decodeBinary(&src, &pos, &dst);
    // then
    ck_assert_int_ne(retval, UA_STATUSCODE_GOOD);
    // finally
    UA_Variant_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_Byte_encode_test) {
    // given
    UA_Byte src       = 8;
    UA_Byte data[]    = { 0x00, 0xFF };
    UA_ByteString dst = { 2, data };
    ck_assert_uint_eq(dst.data[1], 0xFF);

    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];
    UA_StatusCode retval = UA_Byte_encodeBinary(&src, &pos, end);

    ck_assert_uint_eq(dst.data[0], 0x08);
    ck_assert_uint_eq(dst.data[1], 0xFF);
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 1);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    // Test2
    // given
    src = 0xFF;
    dst.data[1] = 0x00;
    pos = dst.data;
    retval      = UA_Byte_encodeBinary(&src, &pos, end);

    ck_assert_int_eq(dst.data[0], 0xFF);
    ck_assert_int_eq(dst.data[1], 0x00);
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 1);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

}
END_TEST

START_TEST(UA_UInt16_encodeNegativeShallEncodeLittleEndian) {
    // given
    UA_UInt16     src    = -1;
    UA_Byte       data[] = { 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst    = { 4, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_UInt16_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 2);
    ck_assert_int_eq(dst.data[0], 0xFF);
    ck_assert_int_eq(dst.data[1], 0xFF);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    // when test 2
    src    = -32768;
    retval = UA_UInt16_encodeBinary(&src, &pos, end);
    // then test 2
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 4);
    ck_assert_int_eq(dst.data[2], 0x00);
    ck_assert_int_eq(dst.data[3], 0x80);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_UInt16_encodeShallEncodeLittleEndian) {
    // given
    UA_UInt16     src    = 0;
    UA_Byte       data[] = {  0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst    = { 4, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_UInt16_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 2);
    ck_assert_int_eq(dst.data[0], 0x00);
    ck_assert_int_eq(dst.data[1], 0x00);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    // when test 2
    src    = 32767;
    retval = UA_UInt16_encodeBinary(&src, &pos, end);
    // then test 2
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 4);
    ck_assert_int_eq(dst.data[2], 0xFF);
    ck_assert_int_eq(dst.data[3], 0x7F);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_UInt32_encodeShallEncodeLittleEndian) {
    // given
    UA_UInt32     src    = -1;
    UA_Byte       data[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst    = { 8, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_UInt32_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 4);
    ck_assert_int_eq(dst.data[0], 0xFF);
    ck_assert_int_eq(dst.data[1], 0xFF);
    ck_assert_int_eq(dst.data[2], 0xFF);
    ck_assert_int_eq(dst.data[3], 0xFF);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    // when test 2
    src    = 0x0101FF00;
    retval = UA_UInt32_encodeBinary(&src, &pos, end);
    // then test 2
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 8);
    ck_assert_int_eq(dst.data[4], 0x00);
    ck_assert_int_eq(dst.data[5], 0xFF);
    ck_assert_int_eq(dst.data[6], 0x01);
    ck_assert_int_eq(dst.data[7], 0x01);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_Int32_encodeShallEncodeLittleEndian) {
    // given
    UA_Int32 src    = 1;
    UA_Byte  data[]   = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst = { 8, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_Int32_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 4);
    ck_assert_int_eq(dst.data[0], 0x01);
    ck_assert_int_eq(dst.data[1], 0x00);
    ck_assert_int_eq(dst.data[2], 0x00);
    ck_assert_int_eq(dst.data[3], 0x00);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    // when test 2
    src    = 0x7FFFFFFF;
    retval = UA_Int32_encodeBinary(&src, &pos, end);
    // then test 2
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 8);
    ck_assert_int_eq(dst.data[4], 0xFF);
    ck_assert_int_eq(dst.data[5], 0xFF);
    ck_assert_int_eq(dst.data[6], 0xFF);
    ck_assert_int_eq(dst.data[7], 0x7F);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_Int32_encodeNegativeShallEncodeLittleEndian) {
    // given
    UA_Int32 src    = -1;
    UA_Byte  data[]   = {  0x55, 0x55,    0x55,  0x55, 0x55,  0x55,    0x55,  0x55 };
    UA_ByteString dst = { 8, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_Int32_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 4);
    ck_assert_int_eq(dst.data[0], 0xFF);
    ck_assert_int_eq(dst.data[1], 0xFF);
    ck_assert_int_eq(dst.data[2], 0xFF);
    ck_assert_int_eq(dst.data[3], 0xFF);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_UInt64_encodeShallWorkOnExample) {
    // given
    UA_UInt64     src    = -1;
    UA_Byte       data[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                             0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst    = { 16, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_UInt64_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 8);
    ck_assert_int_eq(dst.data[0], 0xFF);
    ck_assert_int_eq(dst.data[1], 0xFF);
    ck_assert_int_eq(dst.data[2], 0xFF);
    ck_assert_int_eq(dst.data[3], 0xFF);
    ck_assert_int_eq(dst.data[4], 0xFF);
    ck_assert_int_eq(dst.data[5], 0xFF);
    ck_assert_int_eq(dst.data[6], 0xFF);
    ck_assert_int_eq(dst.data[7], 0xFF);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    // when test 2
    src    = 0x7F0033AA44EE6611;
    retval = UA_UInt64_encodeBinary(&src, &pos, end);
    // then test 2
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 16);
    ck_assert_int_eq(dst.data[8], 0x11);
    ck_assert_int_eq(dst.data[9], 0x66);
    ck_assert_int_eq(dst.data[10], 0xEE);
    ck_assert_int_eq(dst.data[11], 0x44);
    ck_assert_int_eq(dst.data[12], 0xAA);
    ck_assert_int_eq(dst.data[13], 0x33);
    ck_assert_int_eq(dst.data[14], 0x00);
    ck_assert_int_eq(dst.data[15], 0x7F);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_Int64_encodeShallEncodeLittleEndian) {
    // given
    UA_Int64 src    = 0x7F0033AA44EE6611;
    UA_Byte  data[]   = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                          0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst = { 16, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_Int64_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 8);
    ck_assert_int_eq(dst.data[0], 0x11);
    ck_assert_int_eq(dst.data[1], 0x66);
    ck_assert_int_eq(dst.data[2], 0xEE);
    ck_assert_int_eq(dst.data[3], 0x44);
    ck_assert_int_eq(dst.data[4], 0xAA);
    ck_assert_int_eq(dst.data[5], 0x33);
    ck_assert_int_eq(dst.data[6], 0x00);
    ck_assert_int_eq(dst.data[7], 0x7F);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_Int64_encodeNegativeShallEncodeLittleEndian) {
    // given
    UA_Int64 src    = -1;
    UA_Byte  data[]   = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                          0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst = { 16, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_Int64_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 8);
    ck_assert_int_eq(dst.data[0], 0xFF);
    ck_assert_int_eq(dst.data[1], 0xFF);
    ck_assert_int_eq(dst.data[2], 0xFF);
    ck_assert_int_eq(dst.data[3], 0xFF);
    ck_assert_int_eq(dst.data[4], 0xFF);
    ck_assert_int_eq(dst.data[5], 0xFF);
    ck_assert_int_eq(dst.data[6], 0xFF);
    ck_assert_int_eq(dst.data[7], 0xFF);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_Float_encodeShallWorkOnExample) {
#define UA_FLOAT_TESTS 9
    /* use -NAN since the UA standard expected specific values for NAN with the
       negative bit set */
    UA_Float src[UA_FLOAT_TESTS] = {27.5f, -6.5f, 0.0f, -0.0f, -NAN, FLT_MAX, FLT_MIN, INFINITY, -INFINITY};
    UA_Byte result[UA_FLOAT_TESTS][4] = {
        {0x00, 0x00, 0xDC, 0x41}, // 27.5
        {0x00, 0x00, 0xD0, 0xC0}, // -6.5
        {0x00, 0x00, 0x00, 0x00}, // 0.0
        {0x00, 0x00, 0x00, 0x80}, // -0.0
        {0x00, 0x00, 0xC0, 0xFF}, // -NAN
        {0xFF, 0xFF, 0x7F, 0x7F}, // FLT_MAX
        {0x00, 0x00, 0x80, 0x00}, // FLT_MIN
        {0x00, 0x00, 0x80, 0x7F}, // INF
        {0x00, 0x00, 0x80, 0xFF} // -INF
    };
#if defined(_WIN32) || defined(__TINYC__)
    // on WIN32 or TinyCC -NAN is encoded differently
    result[4][3] = 127;
#endif

    UA_Byte data[] = {0x55, 0x55, 0x55,  0x55};
    UA_ByteString dst = {4, data};
    const UA_Byte *end = &dst.data[dst.length];

    for(size_t i = 0; i < 7; i++) {
        UA_Byte *pos = dst.data;
        UA_Int32 retval = UA_Float_encodeBinary(&src[i], &pos, end);
        ck_assert_int_eq((uintptr_t)(pos - dst.data), 4);
        ck_assert_int_eq(dst.data[0], result[i][0]);
        ck_assert_int_eq(dst.data[1], result[i][1]);
        ck_assert_int_eq(dst.data[2], result[i][2]);
        ck_assert_int_eq(dst.data[3], result[i][3]);
        ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    }
}
END_TEST

START_TEST(UA_Double_encodeShallWorkOnExample) {
    // given
    UA_Double src = -6.5;
    UA_Byte data[] = { 0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,
                       0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55 };
    UA_ByteString dst = {16,data};
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when test 1
    UA_StatusCode retval = UA_Double_encodeBinary(&src, &pos, end);
    // then test 1
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 8);
    ck_assert_int_eq(dst.data[6], 0x1A);
    ck_assert_int_eq(dst.data[7], 0xC0);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_String_encodeShallWorkOnExample) {
    // given
    UA_String src;
    src.length = 11;
    UA_Byte mem[12] = "ACPLT OPCUA";
    src.data = mem;

    UA_Byte data[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst = { 24, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when
    UA_StatusCode retval = UA_String_encodeBinary(&src, &pos, end);
    // then
    ck_assert_int_eq((uintptr_t)(pos - dst.data), sizeof(UA_Int32)+11);
    ck_assert_uint_eq(sizeof(UA_Int32)+11, UA_calcSizeBinary(&src, &UA_TYPES[UA_TYPES_STRING]));
    ck_assert_int_eq(dst.data[0], 11);
    ck_assert_int_eq(dst.data[sizeof(UA_Int32)+0], 'A');
    ck_assert_int_eq(dst.data[sizeof(UA_Int32)+1], 'C');
    ck_assert_int_eq(dst.data[sizeof(UA_Int32)+2], 'P');
    ck_assert_int_eq(dst.data[sizeof(UA_Int32)+3], 'L');
    ck_assert_int_eq(dst.data[sizeof(UA_Int32)+4], 'T');
    ck_assert_int_eq(dst.data[sizeof(UA_Int32)+5], 0x20); //Space
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_ExpandedNodeId_encodeShallWorkOnExample) {
    // given
    UA_ExpandedNodeId src = UA_EXPANDEDNODEID_NUMERIC(0, 15);
    src.namespaceUri = UA_STRING("testUri");

    UA_Byte data[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst = { 32, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when
    UA_StatusCode retval = UA_ExpandedNodeId_encodeBinary(&src, &pos, end);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 13);
    ck_assert_uint_eq(13, UA_calcSizeBinary(&src, &UA_TYPES[UA_TYPES_EXPANDEDNODEID]));
    ck_assert_int_eq(dst.data[0], 0x80); // namespaceuri flag
}
END_TEST

START_TEST(UA_DataValue_encodeShallWorkOnExampleWithoutVariant) {
    // given
    UA_DataValue src;
    UA_DataValue_init(&src);
    src.serverTimestamp = 80;
    src.hasServerTimestamp = true;

    UA_Byte data[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
                       0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    UA_ByteString dst = { 24, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when
    UA_StatusCode retval = UA_DataValue_encodeBinary(&src, &pos, end);
    // then
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 9);
    ck_assert_uint_eq(9, UA_calcSizeBinary(&src, &UA_TYPES[UA_TYPES_DATAVALUE]));
    ck_assert_int_eq(dst.data[0], 0x08); // encodingMask
    ck_assert_int_eq(dst.data[1], 80);   // 8 Byte serverTimestamp
    ck_assert_int_eq(dst.data[2], 0);
    ck_assert_int_eq(dst.data[3], 0);
    ck_assert_int_eq(dst.data[4], 0);
    ck_assert_int_eq(dst.data[5], 0);
    ck_assert_int_eq(dst.data[6], 0);
    ck_assert_int_eq(dst.data[7], 0);
    ck_assert_int_eq(dst.data[8], 0);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_DataValue_encodeShallWorkOnExampleWithVariant) {
    // given
    UA_DataValue src;
    UA_DataValue_init(&src);
    src.serverTimestamp    = 80;
    src.hasValue = true;
    src.hasServerTimestamp = true;
    src.value.type = &UA_TYPES[UA_TYPES_INT32];
    src.value.arrayLength  = 0; // one element (encoded as not an array)
    UA_Int32 vdata = 45;
    src.value.data = (void *)&vdata;

    UA_Byte data[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    UA_ByteString dst = { 24, data };
    UA_Byte *pos = dst.data;
    const UA_Byte *end = &dst.data[dst.length];

    // when
    UA_StatusCode retval = UA_DataValue_encodeBinary(&src, &pos, end);
    // then
    ck_assert_int_eq((uintptr_t)(pos - dst.data), 1+(1+4)+8);           // represents the length
    ck_assert_uint_eq(1+(1+4)+8, UA_calcSizeBinary(&src, &UA_TYPES[UA_TYPES_DATAVALUE]));
    ck_assert_int_eq(dst.data[0], 0x08 | 0x01); // encodingMask
    ck_assert_int_eq(dst.data[1], 0x06);        // Variant's Encoding Mask - INT32
    ck_assert_int_eq(dst.data[2], 45);          // the single value
    ck_assert_int_eq(dst.data[3], 0);
    ck_assert_int_eq(dst.data[4], 0);
    ck_assert_int_eq(dst.data[5], 0);
    ck_assert_int_eq(dst.data[6], 80);  // the server timestamp
    ck_assert_int_eq(dst.data[7], 0);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_DateTime_toStructShallWorkOnExample) {
    // given
    UA_DateTime src = 13974671891234567 + (11644473600 * 10000000); // ua counts since 1601, unix since 1970
    //1397467189... is Mon, 14 Apr 2014 09:19:49 GMT
    //...1234567 are the milli-, micro- and nanoseconds
    UA_DateTimeStruct dst;

    // when
    dst = UA_DateTime_toStruct(src);
    // then
    ck_assert_int_eq(dst.nanoSec, 700);
    ck_assert_int_eq(dst.microSec, 456);
    ck_assert_int_eq(dst.milliSec, 123);

    ck_assert_int_eq(dst.sec, 49);
    ck_assert_int_eq(dst.min, 19);
    ck_assert_int_eq(dst.hour, 9);

    ck_assert_int_eq(dst.day, 14);
    ck_assert_int_eq(dst.month, 4);
    ck_assert_int_eq(dst.year, 2014);
}
END_TEST

START_TEST(UA_ExtensionObject_copyShallWorkOnExample) {
    // given
    /* UA_Byte data[3] = { 1, 2, 3 }; */

    /* UA_ExtensionObject value, valueCopied; */
    /* UA_ExtensionObject_init(&value); */
    /* UA_ExtensionObject_init(&valueCopied); */

    //Todo!!
    /* value.typeId = UA_TYPES[UA_TYPES_BYTE].typeId; */
    /* value.encoding    = UA_EXTENSIONOBJECT_ENCODINGMASK_NOBODYISENCODED; */
    /* value.encoding    = UA_EXTENSIONOBJECT_ENCODINGMASK_BODYISBYTESTRING; */
    /* value.body.data   = data; */
    /* value.body.length = 3; */

    /* //when */
    /* UA_ExtensionObject_copy(&value, &valueCopied); */

    /* for(UA_Int32 i = 0;i < 3;i++) */
    /*     ck_assert_int_eq(valueCopied.body.data[i], value.body.data[i]); */

    /* ck_assert_int_eq(valueCopied.encoding, value.encoding); */
    /* ck_assert_int_eq(valueCopied.typeId.identifierType, value.typeId.identifierType); */
    /* ck_assert_int_eq(valueCopied.typeId.identifier.numeric, value.typeId.identifier.numeric); */

    /* //finally */
    /* value.body.data = NULL; // we cannot free the static string */
    /* UA_ExtensionObject_deleteMembers(&value); */
    /* UA_ExtensionObject_deleteMembers(&valueCopied); */
}
END_TEST

START_TEST(UA_Array_copyByteArrayShallWorkOnExample) {
    //given
    UA_String testString;
    UA_Byte  *dstArray;
    UA_Int32  size = 5;
    UA_Int32  i    = 0;
    testString.data = (UA_Byte*)UA_malloc(size);
    testString.data[0] = 'O';
    testString.data[1] = 'P';
    testString.data[2] = 'C';
    testString.data[3] = 'U';
    testString.data[4] = 'A';
    testString.length  = 5;

    //when
    UA_StatusCode retval;
    retval = UA_Array_copy((const void *)testString.data, 5, (void **)&dstArray, &UA_TYPES[UA_TYPES_BYTE]);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    //then
    for(i = 0;i < size;i++)
        ck_assert_int_eq(testString.data[i], dstArray[i]);

    //finally
    UA_String_deleteMembers(&testString);
    UA_free((void *)dstArray);

}
END_TEST

START_TEST(UA_Array_copyUA_StringShallWorkOnExample) {
    // given
    UA_Int32   i, j;
    UA_String *srcArray = (UA_String*)UA_Array_new(3, &UA_TYPES[UA_TYPES_STRING]);
    UA_String *dstArray;

    srcArray[0] = UA_STRING_ALLOC("open");
    srcArray[1] = UA_STRING_ALLOC("62541");
    srcArray[2] = UA_STRING_ALLOC("opc ua");
    //when
    UA_StatusCode retval;
    retval = UA_Array_copy((const void *)srcArray, 3, (void **)&dstArray, &UA_TYPES[UA_TYPES_STRING]);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    //then
    for(i = 0;i < 3;i++) {
        for(j = 0;j < 3;j++)
            ck_assert_int_eq(srcArray[i].data[j], dstArray[i].data[j]);
        ck_assert_int_eq(srcArray[i].length, dstArray[i].length);
    }
    //finally
    UA_Array_delete(srcArray, 3, &UA_TYPES[UA_TYPES_STRING]);
    UA_Array_delete(dstArray, 3, &UA_TYPES[UA_TYPES_STRING]);
}
END_TEST

START_TEST(UA_DiagnosticInfo_copyShallWorkOnExample) {
    //given
    UA_DiagnosticInfo value, innerValue, copiedValue;
    UA_String testString = (UA_String){5, (UA_Byte*)"OPCUA"};

    UA_DiagnosticInfo_init(&value);
    UA_DiagnosticInfo_init(&innerValue);
    value.hasInnerDiagnosticInfo = true;
    value.innerDiagnosticInfo = &innerValue;
    value.hasAdditionalInfo = true;
    value.additionalInfo = testString;

    //when
    UA_DiagnosticInfo_copy(&value, &copiedValue);

    //then
    for(size_t i = 0;i < testString.length;i++)
        ck_assert_int_eq(copiedValue.additionalInfo.data[i], value.additionalInfo.data[i]);
    ck_assert_int_eq(copiedValue.additionalInfo.length, value.additionalInfo.length);

    ck_assert_int_eq(copiedValue.hasInnerDiagnosticInfo, value.hasInnerDiagnosticInfo);
    ck_assert_int_eq(copiedValue.innerDiagnosticInfo->locale, value.innerDiagnosticInfo->locale);
    ck_assert_int_eq(copiedValue.innerStatusCode, value.innerStatusCode);
    ck_assert_int_eq(copiedValue.locale, value.locale);
    ck_assert_int_eq(copiedValue.localizedText, value.localizedText);
    ck_assert_int_eq(copiedValue.namespaceUri, value.namespaceUri);
    ck_assert_int_eq(copiedValue.symbolicId, value.symbolicId);

    //finally
    value.additionalInfo.data = NULL; // do not delete the static string
    value.innerDiagnosticInfo = NULL; // do not delete the static innerdiagnosticinfo
    UA_DiagnosticInfo_deleteMembers(&value);
    UA_DiagnosticInfo_deleteMembers(&copiedValue);

}
END_TEST

START_TEST(UA_ApplicationDescription_copyShallWorkOnExample) {
    //given

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_String appString = (UA_String){3, (UA_Byte*)"APP"};
    UA_String discString = (UA_String){4, (UA_Byte*)"DISC"};
    UA_String gateWayString = (UA_String){7, (UA_Byte*)"GATEWAY"};

    UA_String srcArray[3];
    srcArray[0] = (UA_String){ 6, (UA_Byte*)"__open" };
    srcArray[1] = (UA_String){ 6, (UA_Byte*)"_62541" };
    srcArray[2] = (UA_String){ 6, (UA_Byte*)"opc ua" };

    UA_ApplicationDescription value, copiedValue;
    UA_ApplicationDescription_init(&value);
    value.applicationUri = appString;
    value.discoveryProfileUri = discString;
    value.gatewayServerUri = gateWayString;
    value.discoveryUrlsSize = 3;
    value.discoveryUrls     = srcArray;

    //when
    retval = UA_ApplicationDescription_copy(&value, &copiedValue);

    //then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    for(size_t i = 0; i < appString.length; i++)
        ck_assert_int_eq(copiedValue.applicationUri.data[i], value.applicationUri.data[i]);
    ck_assert_int_eq(copiedValue.applicationUri.length, value.applicationUri.length);

    for(size_t i = 0; i < discString.length; i++)
        ck_assert_int_eq(copiedValue.discoveryProfileUri.data[i], value.discoveryProfileUri.data[i]);
    ck_assert_int_eq(copiedValue.discoveryProfileUri.length, value.discoveryProfileUri.length);

    for(size_t i = 0; i < gateWayString.length; i++)
        ck_assert_int_eq(copiedValue.gatewayServerUri.data[i], value.gatewayServerUri.data[i]);
    ck_assert_int_eq(copiedValue.gatewayServerUri.length, value.gatewayServerUri.length);

    //String Array Test
    for(UA_Int32 i = 0;i < 3;i++) {
        for(UA_Int32 j = 0;j < 6;j++)
            ck_assert_int_eq(value.discoveryUrls[i].data[j], copiedValue.discoveryUrls[i].data[j]);
        ck_assert_int_eq(value.discoveryUrls[i].length, copiedValue.discoveryUrls[i].length);
    }
    ck_assert_int_eq(copiedValue.discoveryUrls[0].data[2], 'o');
    ck_assert_int_eq(copiedValue.discoveryUrls[0].data[3], 'p');
    ck_assert_int_eq(copiedValue.discoveryUrlsSize, value.discoveryUrlsSize);

    //finally
    // UA_ApplicationDescription_deleteMembers(&value); // do not free the members as they are statically allocated
    UA_ApplicationDescription_deleteMembers(&copiedValue);
}
END_TEST

START_TEST(UA_QualifiedName_copyShallWorkOnInputExample) {
    // given
    UA_QualifiedName src = UA_QUALIFIEDNAME(5, "tEsT123!");
    UA_QualifiedName dst;

    // when
    UA_StatusCode ret = UA_QualifiedName_copy(&src, &dst);
    // then
    ck_assert_int_eq(ret, UA_STATUSCODE_GOOD);
    ck_assert_int_eq('E', dst.name.data[1]);
    ck_assert_int_eq('!', dst.name.data[7]);
    ck_assert_int_eq(8, dst.name.length);
    ck_assert_int_eq(5, dst.namespaceIndex);
    // finally
    UA_QualifiedName_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_Guid_copyShallWorkOnInputExample) {
    //given
    const UA_Guid src = {3, 45, 1222, {8, 7, 6, 5, 4, 3, 2, 1}};
    UA_Guid dst;

    //when
    UA_StatusCode ret = UA_Guid_copy(&src, &dst);

    //then
    ck_assert_int_eq(ret, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(src.data1, dst.data1);
    ck_assert_int_eq(src.data3, dst.data3);
    ck_assert_int_eq(src.data4[4], dst.data4[4]);
    //finally
}
END_TEST

START_TEST(UA_LocalizedText_copycstringShallWorkOnInputExample) {
    // given
    char src[8] = {'t', 'e', 'X', 't', '1', '2', '3', (char)0};
    const UA_LocalizedText dst = UA_LOCALIZEDTEXT("", src);

    // then
    ck_assert_int_eq('1', dst.text.data[4]);
    ck_assert_int_eq(0, dst.locale.length);
    ck_assert_int_eq(7, dst.text.length);
}
END_TEST

START_TEST(UA_DataValue_copyShallWorkOnInputExample) {
    // given
    UA_Variant srcVariant;
    UA_Variant_init(&srcVariant);
    UA_DataValue src;
    UA_DataValue_init(&src);
    src.hasSourceTimestamp = true;
    src.sourceTimestamp = 4;
    src.hasSourcePicoseconds = true;
    src.sourcePicoseconds = 77;
    src.hasServerPicoseconds = true;
    src.serverPicoseconds = 8;
    UA_DataValue dst;

    // when
    UA_StatusCode ret = UA_DataValue_copy(&src, &dst);
    // then
    ck_assert_int_eq(ret, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(4, dst.sourceTimestamp);
    ck_assert_int_eq(77, dst.sourcePicoseconds);
    ck_assert_int_eq(8, dst.serverPicoseconds);
}
END_TEST

START_TEST(UA_Variant_copyShallWorkOnSingleValueExample) {
    //given
    UA_String testString = (UA_String){5, (UA_Byte*)"OPCUA"};
    UA_Variant value, copiedValue;
    UA_Variant_init(&value);
    UA_Variant_init(&copiedValue);
    value.data = UA_malloc(sizeof(UA_String));
    *((UA_String*)value.data) = testString;
    value.type = &UA_TYPES[UA_TYPES_STRING];
    value.arrayLength = 1;

    //when
    UA_Variant_copy(&value, &copiedValue);

    //then
    UA_String copiedString = *(UA_String*)(copiedValue.data);
    for(UA_Int32 i = 0;i < 5;i++)
        ck_assert_int_eq(copiedString.data[i], testString.data[i]);
    ck_assert_int_eq(copiedString.length, testString.length);

    ck_assert_int_eq(value.arrayDimensionsSize, copiedValue.arrayDimensionsSize);
    ck_assert_int_eq(value.arrayLength, copiedValue.arrayLength);

    //finally
    ((UA_String*)value.data)->data = NULL; // the string is statically allocated. do not free it.
    UA_Variant_deleteMembers(&value);
    UA_Variant_deleteMembers(&copiedValue);
}
END_TEST

START_TEST(UA_Variant_copyShallWorkOnByteStringIndexRange) {
    UA_ByteString text = UA_BYTESTRING("My xml");
    UA_Variant src;
    UA_Variant_setScalar(&src, &text, &UA_TYPES[UA_TYPES_BYTESTRING]);

    UA_NumericRangeDimension d1 = {0, 8388607};
    UA_NumericRange nr;
    nr.dimensionsSize = 1;
    nr.dimensions = &d1;

    UA_Variant dst;
    UA_StatusCode retval = UA_Variant_copyRange(&src, &dst, nr);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    UA_Variant_deleteMembers(&dst);
}
END_TEST

START_TEST(UA_Variant_copyShallWorkOn1DArrayExample) {
    // given
    UA_String *srcArray = (UA_String*)UA_Array_new(3, &UA_TYPES[UA_TYPES_STRING]);
    srcArray[0] = UA_STRING_ALLOC("__open");
    srcArray[1] = UA_STRING_ALLOC("_62541");
    srcArray[2] = UA_STRING_ALLOC("opc ua");

    UA_UInt32 *dimensions;
    dimensions = (UA_UInt32*)UA_malloc(sizeof(UA_UInt32));
    dimensions[0] = 3;

    UA_Variant value, copiedValue;
    UA_Variant_init(&value);
    UA_Variant_init(&copiedValue);

    value.arrayLength = 3;
    value.data = (void *)srcArray;
    value.arrayDimensionsSize = 1;
    value.arrayDimensions = dimensions;
    value.type = &UA_TYPES[UA_TYPES_STRING];

    //when
    UA_Variant_copy(&value, &copiedValue);

    //then
    UA_Int32 i1 = value.arrayDimensions[0];
    UA_Int32 i2 = copiedValue.arrayDimensions[0];
    ck_assert_int_eq(i1, i2);

    for(UA_Int32 i = 0;i < 3;i++) {
        for(UA_Int32 j = 0;j < 6;j++) {
            ck_assert_int_eq(((UA_String *)value.data)[i].data[j],
                    ((UA_String *)copiedValue.data)[i].data[j]);
        }
        ck_assert_int_eq(((UA_String *)value.data)[i].length,
                ((UA_String *)copiedValue.data)[i].length);
    }
    ck_assert_int_eq(((UA_String *)copiedValue.data)[0].data[2], 'o');
    ck_assert_int_eq(((UA_String *)copiedValue.data)[0].data[3], 'p');
    ck_assert_int_eq(value.arrayDimensionsSize, copiedValue.arrayDimensionsSize);
    ck_assert_int_eq(value.arrayLength, copiedValue.arrayLength);

    //finally
    UA_Variant_deleteMembers(&value);
    UA_Variant_deleteMembers(&copiedValue);
}
END_TEST

START_TEST(UA_Variant_copyShallWorkOn2DArrayExample) {
    // given
    UA_Int32 *srcArray = (UA_Int32*)UA_Array_new(6, &UA_TYPES[UA_TYPES_INT32]);
    srcArray[0] = 0;
    srcArray[1] = 1;
    srcArray[2] = 2;
    srcArray[3] = 3;
    srcArray[4] = 4;
    srcArray[5] = 5;

    UA_UInt32 *dimensions = (UA_UInt32*)UA_Array_new(2, &UA_TYPES[UA_TYPES_INT32]);
    UA_UInt32 dim1 = 3;
    UA_UInt32 dim2 = 2;
    dimensions[0] = dim1;
    dimensions[1] = dim2;

    UA_Variant value, copiedValue;
    UA_Variant_init(&value);
    UA_Variant_init(&copiedValue);

    value.arrayLength = 6;
    value.data     = srcArray;
    value.arrayDimensionsSize = 2;
    value.arrayDimensions     = dimensions;
    value.type = &UA_TYPES[UA_TYPES_INT32];

    //when
    UA_Variant_copy(&value, &copiedValue);

    //then
    //1st dimension
    UA_Int32 i1 = value.arrayDimensions[0];
    UA_Int32 i2 = copiedValue.arrayDimensions[0];
    ck_assert_int_eq(i1, i2);
    ck_assert_int_eq(i1, dim1);


    //2nd dimension
    i1 = value.arrayDimensions[1];
    i2 = copiedValue.arrayDimensions[1];
    ck_assert_int_eq(i1, i2);
    ck_assert_int_eq(i1, dim2);


    for(UA_Int32 i = 0;i < 6;i++) {
        i1 = ((UA_Int32 *)value.data)[i];
        i2 = ((UA_Int32 *)copiedValue.data)[i];
        ck_assert_int_eq(i1, i2);
        ck_assert_int_eq(i2, i);
    }

    ck_assert_int_eq(value.arrayDimensionsSize, copiedValue.arrayDimensionsSize);
    ck_assert_int_eq(value.arrayLength, copiedValue.arrayLength);

    //finally
    UA_Variant_deleteMembers(&value);
    UA_Variant_deleteMembers(&copiedValue);
}
END_TEST

START_TEST(UA_ExtensionObject_encodeDecodeShallWorkOnExtensionObject) {
    /* UA_Int32 val = 42; */
    /* UA_VariableAttributes varAttr; */
    /* UA_VariableAttributes_init(&varAttr); */
    /* varAttr.dataType = UA_TYPES[UA_TYPES_INT32].typeId; */
    /* UA_Variant_init(&varAttr.value); */
    /* varAttr.value.type = &UA_TYPES[UA_TYPES_INT32]; */
    /* varAttr.value.data = &val; */
    /* varAttr.value.arrayLength = -1; */
    /* varAttr.userWriteMask = 41; */
    /* varAttr.specifiedAttributes |= UA_NODEATTRIBUTESMASK_DATATYPE; */
    /* varAttr.specifiedAttributes |= UA_NODEATTRIBUTESMASK_VALUE; */
    /* varAttr.specifiedAttributes |= UA_NODEATTRIBUTESMASK_USERWRITEMASK; */

    /* /\* wrap it into a extension object attributes *\/ */
    /* UA_ExtensionObject extensionObject; */
    /* UA_ExtensionObject_init(&extensionObject); */
    /* extensionObject.typeId = UA_TYPES[UA_TYPES_VARIABLEATTRIBUTES].typeId; */
    /* UA_Byte extensionData[50]; */
    /* extensionObject.body = (UA_ByteString){.data = extensionData, .length=50}; */
    /* size_t posEncode = 0; */
    /* UA_VariableAttributes_encodeBinary(&varAttr, &extensionObject.body, posEncode); */
    /* extensionObject.body.length = posEncode; */
    /* extensionObject.encoding = UA_EXTENSIONOBJECT_ENCODINGMASK_BODYISBYTESTRING; */

    /* UA_Byte data[50]; */
    /* UA_ByteString dst = {.data = data, .length=50}; */

    /* posEncode = 0; */
    /* UA_ExtensionObject_encodeBinary(&extensionObject, &dst, posEncode); */

    /* UA_ExtensionObject extensionObjectDecoded; */
    /* size_t posDecode = 0; */
    /* UA_ExtensionObject_decodeBinary(&dst, &posDecode, &extensionObjectDecoded); */

    /* ck_assert_int_eq(posEncode, posDecode); */
    /* ck_assert_int_eq(extensionObjectDecoded.body.length, extensionObject.body.length); */

    /* UA_VariableAttributes varAttrDecoded; */
    /* UA_VariableAttributes_init(&varAttrDecoded); */
    /* posDecode = 0; */
    /* UA_VariableAttributes_decodeBinary(&extensionObjectDecoded.body, &posDecode, &varAttrDecoded); */
    /* ck_assert_uint_eq(41, varAttrDecoded.userWriteMask); */
    /* ck_assert_int_eq(-1, varAttrDecoded.value.arrayLength); */

    /* // finally */
    /* UA_ExtensionObject_deleteMembers(&extensionObjectDecoded); */
    /* UA_Variant_deleteMembers(&varAttrDecoded.value); */
}
END_TEST


/* ------------------------------ENCODE---------------------------------------- */
START_TEST(UA_Boolean_true_json_encode) {
   
    UA_Boolean *src = UA_Boolean_new();
    *src = UA_TRUE;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_BOOLEAN];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "true";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Boolean_false_json_encode) {
   
    UA_Boolean *src = UA_Boolean_new();
    *src = UA_FALSE;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_BOOLEAN];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "false";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_String_json_encode) {
    // given
    UA_String src = UA_STRING("hello");
    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 10);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[10];
    // when
    status s = UA_encodeJson(&src, &UA_TYPES[UA_TYPES_STRING], &bufPos, &bufEnd, NULL, NULL);
    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "\"hello\"";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_UInt16_PosNumber_json_encode) {

    UA_UInt16 *src = UA_UInt16_new();
    *src = 12346;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_UINT16];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "12346";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST


START_TEST(UA_UInt32_PosNumber_json_encode) {

    UA_UInt32 *src = UA_UInt32_new();
    *src = 4294967295;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_UINT32];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);
    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "4294967295";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST


START_TEST(UA_UInt64_PosNumber_json_encode) {

    UA_UInt64 *src = UA_UInt64_new();
    *src = 1844674407370955161;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_UINT64];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "1844674407370955161";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Int16_NegNumber_json_encode) {
    
    UA_Int16 *src = UA_Int16_new();
    *src = -4242;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_INT16];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "-4242";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Int32_NegNumber_json_encode) {
    UA_Int32 *src = UA_Int32_new();
    *src = -2147483648;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_INT32];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "-2147483648";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Int64_NegNumber_json_encode) {
    UA_Int64 *src = UA_Int64_new();
    *src = ((int64_t)-9223372036854775807);
    const UA_DataType *type = &UA_TYPES[UA_TYPES_INT64];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "-9223372036854775807";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_LocText_json_encode) {

      UA_LocalizedText *src = UA_LocalizedText_new();
    src->locale = UA_STRING("asdf");
    src->text = UA_STRING("jklö");
    const UA_DataType *type = &UA_TYPES[UA_TYPES_LOCALIZEDTEXT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Locale\":\"asdf\",\"Text\":\"jklö\"}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Guid_json_encode) {
    UA_Guid *src = UA_Guid_new();
    *src = UA_Guid_random();
    const UA_DataType *type = &UA_TYPES[UA_TYPES_GUID];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);
    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "\"152CA78D-6003-027C-F3BF-BB7BEEFEEFBE\"";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_DateTime_json_encode) {
    UA_DateTime *src = UA_DateTime_new();
    *src = UA_DateTime_fromUnixTime(1234567);
    const UA_DataType *type = &UA_TYPES[UA_TYPES_DATETIME];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "\"1970-01-15T06:56:07Z\""; //TODO
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_StatusCode_json_encode) {
    UA_StatusCode *src = UA_StatusCode_new();
    *src = UA_STATUSCODE_BADAGGREGATECONFIGURATIONREJECTED;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_STATUSCODE];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

     *bufPos = 0;
    
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "2161770496";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_NodeId_Nummeric_json_encode) {
    UA_NodeId *src = UA_NodeId_new();
    *src = UA_NODEID_NUMERIC(0, 5555);
    const UA_DataType *type = &UA_TYPES[UA_TYPES_NODEID];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Id\":5555}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_NodeId_String_json_encode) {
    UA_NodeId *src = UA_NodeId_new();
    *src = UA_NODEID_STRING(0, "foobar");
    const UA_DataType *type = &UA_TYPES[UA_TYPES_NODEID];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"IdType\":1,\"Id\":\"foobar\"}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_NodeId_Guid_json_encode) {
    UA_NodeId *src = UA_NodeId_new();
    *src = UA_NODEID_GUID(0, UA_Guid_random());
    const UA_DataType *type = &UA_TYPES[UA_TYPES_NODEID];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"IdType\":2,\"Id\":\"1CD777E3-9590-A4E2-6DB6-4BE40EE09EB9\"}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_NodeId_ByteString_json_encode) {
    UA_NodeId *src = UA_NodeId_new();
    *src = UA_NODEID_BYTESTRING(0, "asdfasdf");
    const UA_DataType *type = &UA_TYPES[UA_TYPES_NODEID];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"IdType\":3,\"Id\":\"YXNkZmFzZGY=\"}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_DiagInfo_json_encode) {
    UA_DiagnosticInfo *innerDiag = UA_DiagnosticInfo_new();
    innerDiag->hasAdditionalInfo = UA_TRUE;
    innerDiag->additionalInfo = UA_STRING("INNER ADDITION INFO");
    innerDiag->hasInnerDiagnosticInfo = UA_FALSE;
    innerDiag->hasInnerStatusCode = UA_FALSE;
    innerDiag->hasLocale = UA_FALSE;
    innerDiag->hasSymbolicId = UA_FALSE;
    innerDiag->hasLocalizedText = UA_FALSE;
    innerDiag->hasNamespaceUri = UA_FALSE;

    UA_DiagnosticInfo *src = UA_DiagnosticInfo_new();

    src->hasAdditionalInfo = UA_TRUE;
    src->hasInnerDiagnosticInfo = UA_TRUE;
    src->hasInnerStatusCode = UA_TRUE;
    src->hasLocale = UA_TRUE;
    src->hasSymbolicId = UA_TRUE;
    src->hasLocalizedText = UA_TRUE;
    src->hasNamespaceUri = UA_FALSE;

    UA_StatusCode statusCode = UA_STATUSCODE_BADARGUMENTSMISSING;
    src->additionalInfo = UA_STRING("additionalInfo");
    src->innerDiagnosticInfo = innerDiag;
    src->innerStatusCode = statusCode;
    src->locale = 12;
    src->symbolicId = 13;
    src->localizedText = 14;

    const UA_DataType *type = &UA_TYPES[UA_TYPES_DIAGNOSTICINFO];


    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"SymbolicId\":13,\"LocalizedText\":14,\"Locale\":12,\"AdditionalInfo\":\"additionalInfo\",\"InnerStatusCode\":2155216896,\"InnerDiagnosticInfo\":{\"AdditionalInfo\":\"INNER ADDITION INFO\"}}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_ByteString_json_encode) {
    UA_ByteString *src = UA_ByteString_new();
    *src = UA_BYTESTRING("asdfasdf");
    const UA_DataType *type = &UA_TYPES[UA_TYPES_BYTESTRING];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "\"YXNkZmFzZGY=\"";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_QualName_json_encode) {
    UA_QualifiedName *src = UA_QualifiedName_new();
    src->name = UA_STRING("derName");
    src->namespaceIndex = 1;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_QUALIFIEDNAME];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Name\":\"derName\",\"Uri\":1}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_Bool_json_encode) {
    UA_Variant *src = UA_Variant_new();
    UA_Boolean variantContent = true;
    UA_Variant_setScalar(src, &variantContent, &UA_TYPES[UA_TYPES_BOOLEAN]);

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":0,\"Body\":true}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_Number_json_encode) {
    UA_Variant *src = UA_Variant_new();
    UA_UInt64 variantContent = 345634563456;
    UA_Variant_setScalar(src, &variantContent, &UA_TYPES[UA_TYPES_UINT64]);

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":8,\"Body\":345634563456}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST


START_TEST(UA_Variant_NodeId_json_encode) {
    UA_Variant *src = UA_Variant_new();
    UA_NodeId variantContent = UA_NODEID_STRING(1, "theID");
    UA_Variant_setScalar(src, &variantContent, &UA_TYPES[UA_TYPES_NODEID]);

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":16,\"Body\":{\"IdType\":1,\"Id\":\"theID\",\"Namespace\":1}}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_LocText_json_encode) {
    UA_Variant *src = UA_Variant_new();
    UA_LocalizedText variantContent;
    variantContent.locale = UA_STRING("localeString");
    variantContent.text = UA_STRING("textString");
    UA_Variant_setScalar(src, &variantContent, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":20,\"Body\":{\"Locale\":\"localeString\",\"Text\":\"textString\"}}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_QualName_json_encode) {
    UA_Variant *src = UA_Variant_new();
    
    UA_QualifiedName *variantContent = UA_QualifiedName_new();
    variantContent->name = UA_STRING("derName");
    variantContent->namespaceIndex = 1;
    
    UA_Variant_setScalar(src, variantContent, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":19,\"Body\":{\"Name\":\"derName\",\"Uri\":1}}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_ExtensionObject_json_encode) {
    UA_ExtensionObject *src = UA_ExtensionObject_new();
    UA_ExtensionObject_init(src);
    src->encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    src->content.decoded.type = &UA_TYPES[UA_TYPES_BOOLEAN];

    UA_Boolean b = UA_FALSE;
    src->content.decoded.data = &b;

    const UA_DataType *type = &UA_TYPES[UA_TYPES_EXTENSIONOBJECT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    //TODO
    char* result = "{\"TypeId\":{\"Id\":0},\"Encoding\":0,\"Body\":false}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_ExpandedNodeId_json_encode) {
    UA_ExpandedNodeId *src = UA_ExpandedNodeId_new();
    *src = UA_EXPANDEDNODEID_STRING(23, "testtestTest");
    src->namespaceUri = UA_STRING("asdf");
    src->serverIndex = 1345;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_EXPANDEDNODEID];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"IdType\":1,\"Id\":\"testtestTest\",\"Namespace\":23,\"Namespace\":\"\"asdf\"\",\"ServerUri\":1345}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_DataValue_json_encode) {
    UA_DataValue *src = UA_DataValue_new();
    src->hasServerPicoseconds = UA_TRUE;
    src->hasServerTimestamp = UA_TRUE;
    src->hasSourcePicoseconds = UA_TRUE;
    src->hasSourceTimestamp = UA_TRUE;
    src->hasStatus = UA_TRUE;
    src->hasValue = UA_TRUE;

    UA_DateTime srcts = UA_DateTime_fromUnixTime(1234567);
    UA_DateTime srvts = UA_DateTime_fromUnixTime(1234567);

    src->sourceTimestamp = srcts;
    src->serverTimestamp = srvts;
    src->sourcePicoseconds = 0;
    src->serverPicoseconds = 0;
    
    UA_Variant *variant = UA_Variant_new();
    UA_Boolean variantContent = true;
    UA_Variant_setScalar(variant, &variantContent, &UA_TYPES[UA_TYPES_BOOLEAN]);
    src->value = *variant;

    src->status = UA_STATUSCODE_BADAPPLICATIONSIGNATUREINVALID;
    const UA_DataType *type = &UA_TYPES[UA_TYPES_DATAVALUE];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Value\":{\"Type\":0,\"Body\":true},\"Status\":2153250816,\"SourceTimestamp\":\"1970-01-15T06:56:07Z\",\"SourcePicoseconds\":0,\"ServerTimestamp\":\"1970-01-15T06:56:07Z\",\"ServerPicoseconds\":0}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_MessageReadResponse_json_encode) {
    UA_ReadResponse src;
    
    UA_DiagnosticInfo *innerDiag = UA_DiagnosticInfo_new();
    innerDiag->hasAdditionalInfo = UA_TRUE;
    innerDiag->additionalInfo = UA_STRING("INNER ADDITION INFO");
    innerDiag->hasInnerDiagnosticInfo = UA_FALSE;
    innerDiag->hasInnerStatusCode = UA_FALSE;
    innerDiag->hasLocale = UA_FALSE;
    innerDiag->hasSymbolicId = UA_FALSE;
    innerDiag->hasLocalizedText = UA_FALSE;
    innerDiag->hasNamespaceUri = UA_FALSE;
    
    UA_DiagnosticInfo info[1];
    info[0] = *innerDiag;
    src.diagnosticInfos = info;
    src.diagnosticInfosSize = 1;
    
    UA_DataValue *dv = UA_DataValue_new();
    dv->hasServerPicoseconds = UA_TRUE;
    dv->hasServerTimestamp = UA_TRUE;
    dv->hasSourcePicoseconds = UA_TRUE;
    dv->hasSourceTimestamp = UA_TRUE;
    dv->hasStatus = UA_TRUE;
    dv->hasValue = UA_TRUE;

    UA_DateTime srcts = UA_DateTime_fromUnixTime(1234567);
    UA_DateTime srvts = UA_DateTime_fromUnixTime(1234567);

    dv->sourceTimestamp = srcts;
    dv->serverTimestamp = srvts;
    dv->sourcePicoseconds = 0;
    dv->serverPicoseconds = 0;
    
    UA_Variant *variant = UA_Variant_new();
    UA_Boolean variantContent = true;
    UA_Variant_setScalar(variant, &variantContent, &UA_TYPES[UA_TYPES_BOOLEAN]);
    dv->value = *variant;

    dv->status = UA_STATUSCODE_BADAPPLICATIONSIGNATUREINVALID;
    
    UA_DataValue values[1];
    values[0] = *dv;
    src.results = values;
    src.resultsSize = 1;
    
    
    UA_ResponseHeader rh;
    rh.stringTableSize = 0;
    rh.requestHandle = 123123;
    rh.serviceResult = UA_STATUSCODE_GOOD;
    rh.timestamp = UA_DateTime_fromUnixTime(1234567);
    
    
     UA_DiagnosticInfo *serverDiag = UA_DiagnosticInfo_new();
    serverDiag->hasAdditionalInfo = UA_TRUE;
    serverDiag->additionalInfo = UA_STRING("serverDiag");
    serverDiag->hasInnerDiagnosticInfo = UA_FALSE;
    serverDiag->hasInnerStatusCode = UA_FALSE;
    serverDiag->hasLocale = UA_FALSE;
    serverDiag->hasSymbolicId = UA_FALSE;
    serverDiag->hasLocalizedText = UA_FALSE;
    serverDiag->hasNamespaceUri = UA_FALSE;
    rh.serviceDiagnostics = *serverDiag;
    
    
     UA_ExtensionObject *e = UA_ExtensionObject_new();
    UA_ExtensionObject_init(e);
    e->encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    e->content.decoded.type = &UA_TYPES[UA_TYPES_BOOLEAN];

    UA_Boolean b = UA_FALSE;
    e->content.decoded.data = &b;
    
    rh.additionalHeader = *e;
            
    src.responseHeader = rh;
    
    const UA_DataType *type = &UA_TYPES[UA_TYPES_READRESPONSE];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) &src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"ResponseHeader\":{\"Timestamp\":\"1970-01-15T06:56:07Z\",\"RequestHandle\":123123,\"ServiceResult\":0,\"ServiceDiagnostics\":{\"AdditionalInfo\":\"serverDiag\"},\"StringTable\":[],\"AdditionalHeader\":{\"TypeId\":{\"Id\":0},\"Encoding\":0,\"Body\":false}},\"Results\":[{\"Value\":{\"Type\":0,\"Body\":true},\"Status\":2153250816,\"SourceTimestamp\":\"1970-01-15T06:56:07Z\",\"SourcePicoseconds\":0,\"ServerTimestamp\":\"1970-01-15T06:56:07Z\",\"ServerPicoseconds\":0}],\"DiagnosticInfos\":[{\"AdditionalInfo\":\"INNER ADDITION INFO\"}]}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_ViewDescription_json_encode) {
    UA_ViewDescription src;
    
    UA_DateTime srvts = UA_DateTime_fromUnixTime(1234567);
    src.timestamp = srvts;
    src.viewVersion = 1236;
    src.viewId = UA_NODEID_NUMERIC(0,99999);
    
    const UA_DataType *type = &UA_TYPES[UA_TYPES_VIEWDESCRIPTION];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) &src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"ViewId\":{\"Id\":99999},\"Timestamp\":\"1970-01-15T06:56:07Z\",\"ViewVersion\":1236}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_WriteRequest_json_encode) {
    UA_WriteRequest src;
    
    
    UA_RequestHeader rh;
    rh.returnDiagnostics = 1;
    rh.auditEntryId = UA_STRING("Auditentryid");
    rh.requestHandle = 123123;
    rh.authenticationToken = UA_NODEID_STRING(0,"authToken");
    rh.timestamp = UA_DateTime_fromUnixTime(1234567);
    rh.timeoutHint = 120;
    
    UA_ExtensionObject e;
    e.encoding = UA_EXTENSIONOBJECT_DECODED_NODELETE;
    e.content.decoded.type = &UA_TYPES[UA_TYPES_BOOLEAN];

    UA_Boolean b = UA_FALSE;
    e.content.decoded.data = &b;
    
    rh.additionalHeader = e;
   
    
    
    UA_DataValue dv;
    dv.hasServerPicoseconds = UA_TRUE;
    dv.hasServerTimestamp = UA_TRUE;
    dv.hasSourcePicoseconds = UA_TRUE;
    dv.hasSourceTimestamp = UA_TRUE;
    dv.hasStatus = UA_TRUE;
    dv.hasValue = UA_TRUE;

    UA_DateTime srcts = UA_DateTime_fromUnixTime(1234567);
    UA_DateTime srvts = UA_DateTime_fromUnixTime(1234567);

    dv.sourceTimestamp = srcts;
    dv.serverTimestamp = srvts;
    dv.sourcePicoseconds = 0;
    dv.serverPicoseconds = 0;
    
    UA_Variant variant;
    UA_Boolean variantContent = true;
    UA_Variant_setScalar(&variant, &variantContent, &UA_TYPES[UA_TYPES_BOOLEAN]);
    dv.value = variant;

    dv.status = UA_STATUSCODE_BADAPPLICATIONSIGNATUREINVALID;
    
    UA_DataValue dv2;
    dv2.hasServerPicoseconds = UA_TRUE;
    dv2.hasServerTimestamp = UA_TRUE;
    dv2.hasSourcePicoseconds = UA_TRUE;
    dv2.hasSourceTimestamp = UA_TRUE;
    dv2.hasStatus = UA_TRUE;
    dv2.hasValue = UA_TRUE;

    UA_DateTime srcts2 = UA_DateTime_fromUnixTime(1234567);
    UA_DateTime srvts2 = UA_DateTime_fromUnixTime(1234567);

    dv2.sourceTimestamp = srcts2;
    dv2.serverTimestamp = srvts2;
    dv2.sourcePicoseconds = 0;
    dv2.serverPicoseconds = 0;
    
    UA_Variant variant2;
    UA_Boolean variantContent2 = true;
    UA_Variant_setScalar(&variant2, &variantContent2, &UA_TYPES[UA_TYPES_BOOLEAN]);
    dv2.value = variant2;

    dv2.status = UA_STATUSCODE_BADAPPLICATIONSIGNATUREINVALID;
    
    UA_WriteValue value;
    value.value = dv;
    value.attributeId = 12;
    value.indexRange = UA_STRING("BLOAB");
    value.nodeId = UA_NODEID_STRING(0, "a1111");
    
    UA_WriteValue value2;
    value2.value = dv2;
    value2.attributeId = 12;
    value2.indexRange = UA_STRING("BLOAB");
    value2.nodeId = UA_NODEID_STRING(0, "a2222");
    
    UA_WriteValue values[2];
    values[0] = value;
    values[1] = value2;
    
    
    
    src.nodesToWrite = values;
    src.nodesToWriteSize = 2;
    src.requestHeader = rh;

    
    const UA_DataType *type = &UA_TYPES[UA_TYPES_WRITEREQUEST];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 2000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[2000];

    status s = UA_encodeJson((void *) &src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"RequestHeader\":{\"AuthenticationToken\":{\"IdType\":1,\"Id\":\"authToken\"},\"Timestamp\":\"1970-01-15T06:56:07Z\",\"RequestHandle\":123123,\"ReturnDiagnostics\":1,\"AuditEntryId\":\"Auditentryid\",\"TimeoutHint\":120,\"AdditionalHeader\":{\"TypeId\":{\"Id\":0},\"Encoding\":0,\"Body\":false}},\"NodesToWrite\":[{\"NodeId\":{\"IdType\":1,\"Id\":\"a1111\"},\"AttributeId\":12,\"IndexRange\":\"BLOAB\",\"Value\":{\"Value\":{\"Type\":0,\"Body\":true},\"Status\":2153250816,\"SourceTimestamp\":\"1970-01-15T06:56:07Z\",\"SourcePicoseconds\":0,\"ServerTimestamp\":\"1970-01-15T06:56:07Z\",\"ServerPicoseconds\":0}},{\"NodeId\":{\"IdType\":1,\"Id\":\"a2222\"},\"AttributeId\":12,\"IndexRange\":\"BLOAB\",\"Value\":{\"Value\":{\"Type\":0,\"Body\":true},\"Status\":2153250816,\"SourceTimestamp\":\"1970-01-15T06:56:07Z\",\"SourcePicoseconds\":0,\"ServerTimestamp\":\"1970-01-15T06:56:07Z\",\"ServerPicoseconds\":0}}]}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_Array_UInt16_json_encode) {
    UA_Variant *src = UA_Variant_new();
    UA_UInt16 zero[2] = {42,43};
    UA_Variant_setArray(src, zero, 2, &UA_TYPES[UA_TYPES_UINT16]);
   

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":4,\"Body\":[42,43]}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_Array_Byte_json_encode) {
    UA_Variant *src = UA_Variant_new();
    UA_Byte zero[2] = {42,43};
    UA_Variant_setArray(src, zero, 2, &UA_TYPES[UA_TYPES_BYTE]);
   

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":2,\"Body\":[42,43]}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_Array_String_json_encode) {
    UA_Variant *src = UA_Variant_new();
    UA_String zero[2] = {UA_STRING("eins"),UA_STRING("zwei")};
    UA_Variant_setArray(src, zero, 2, &UA_TYPES[UA_TYPES_STRING]);
   

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":11,\"Body\":[\"eins\",\"zwei\"]}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

START_TEST(UA_Variant_Matrix_UInt16_json_encode) {

     /* Set an array value */
    UA_Variant src;
    UA_UInt16 d[9] = {1, 2, 3,
                      4, 5, 6,
                      7, 8, 9};
    UA_Variant_setArrayCopy(&src, d, 9, &UA_TYPES[UA_TYPES_UINT16]);

    /* Set array dimensions */
    src.arrayDimensions = (UA_UInt32 *)UA_Array_new(2, &UA_TYPES[UA_TYPES_UINT32]);
    src.arrayDimensionsSize = 2;
    src.arrayDimensions[0] = 3;
    src.arrayDimensions[1] = 3;
    
   

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) &src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "{\"Type\":4,\"Body\":[1,2,3,4,5,6,7,8,9],\"Dimension\":[3,3]}";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST

/*
START_TEST(UA_Variant_Matrix_String_NonReversible_json_encode) {
    UA_Variant src;
    UA_String d[] = {UA_STRING("1"), UA_STRING("2"), UA_STRING("3"),
                      UA_STRING("4"), UA_STRING("5"), UA_STRING("6"),
                      UA_STRING("7"), UA_STRING("8")};
    UA_Variant_setArrayCopy(&src, d, 8, &UA_TYPES[UA_TYPES_STRING]);

    src.arrayDimensions = (UA_UInt32 *)UA_Array_new(4, &UA_TYPES[UA_TYPES_UINT32]);
    src.arrayDimensionsSize = 4;
    src.arrayDimensions[0] = 2;
    src.arrayDimensions[1] = 2;
    src.arrayDimensions[2] = 2;
    src.arrayDimensions[3] = 1;
    
   

    const UA_DataType *type = &UA_TYPES[UA_TYPES_VARIANT];

    UA_ByteString buf;

    UA_ByteString_allocBuffer(&buf, 1000);

    UA_Byte *bufPos = &buf.data[0];
    const UA_Byte *bufEnd = &buf.data[1000];

    status s = UA_encodeJson((void *) &src, type, &bufPos, &bufEnd, NULL, NULL);

    *bufPos = 0;
    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "[[[[\"1\"],[\"2\"]],[[\"3\"],[\"4\"]]],[[[\"5\"],[\"6\"]],[[\"7\"],[\"8\"]]]]";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST
*/

/*
START_TEST(UA__json_encode) {

    // then
    ck_assert_int_eq(s, UA_STATUSCODE_GOOD);
    char* result = "hello";
    ck_assert_str_eq(result, (char*)buf.data);
    UA_ByteString_deleteMembers(&buf);
}
END_TEST
 * */

/* ---------------------------DECODE-------------------------------------*/

START_TEST(UA_UInt16_json_decode) {
    // given
    UA_UInt16 out;
    UA_ByteString buf = UA_STRING("65535");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_UINT16], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out, 65535);
}
END_TEST

START_TEST(UA_UInt32_json_decode) {
    // given
    UA_UInt32 out;
    UA_ByteString buf = UA_STRING("4294967295");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_UINT32], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out, 4294967295);
}
END_TEST

START_TEST(UA_UInt64_json_decode) {
    // given
    UA_UInt64 out;
    UA_ByteString buf = UA_STRING("18446744073709551615");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_UINT64], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    //Compare 64bit with check?
    ck_assert_int_eq(((u8*)(&out))[0], 0xFF);
    ck_assert_int_eq(((u8*)(&out))[1], 0xFF);
    ck_assert_int_eq(((u8*)(&out))[2], 0xFF);
    ck_assert_int_eq(((u8*)(&out))[3], 0xFF);
    ck_assert_int_eq(((u8*)(&out))[4], 0xFF);
    ck_assert_int_eq(((u8*)(&out))[5], 0xFF);
    ck_assert_int_eq(((u8*)(&out))[6], 0xFF);
    ck_assert_int_eq(((u8*)(&out))[7], 0xFF);
}
END_TEST

START_TEST(UA_String_json_decode) {
    // given
    UA_String out;
    UA_ByteString buf = UA_STRING("abcdef");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_STRING], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.length, 6);
    ck_assert_int_eq(out.data[0], 'a');
    ck_assert_int_eq(out.data[1], 'b');
    ck_assert_int_eq(out.data[2], 'c');
    ck_assert_int_eq(out.data[3], 'd');
    ck_assert_int_eq(out.data[4], 'e');
    ck_assert_int_eq(out.data[5], 'f');
}
END_TEST

START_TEST(UA_ByteString_json_decode) {
    // given
    UA_ByteString out;
    UA_ByteString buf = UA_STRING("YXNkZmFzZGY=");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_BYTESTRING], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.length, 8);
    ck_assert_int_eq(out.data[0], 'a');
    ck_assert_int_eq(out.data[1], 's');
    ck_assert_int_eq(out.data[2], 'd');
    ck_assert_int_eq(out.data[3], 'f');
    ck_assert_int_eq(out.data[4], 'a');
    ck_assert_int_eq(out.data[5], 's');
    ck_assert_int_eq(out.data[6], 'd');
    ck_assert_int_eq(out.data[7], 'f');
}
END_TEST

START_TEST(UA_Guid_json_decode) {
    // given
    UA_Guid out;
    UA_ByteString buf = UA_STRING("00000001-0002-0003-0405-060708090A0B");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_GUID], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.data1, 1);
    ck_assert_int_eq(out.data2, 2);
    ck_assert_int_eq(out.data3, 3);
    ck_assert_int_eq(out.data4[0], 4);
    ck_assert_int_eq(out.data4[1], 5);
    ck_assert_int_eq(out.data4[2], 6);
    ck_assert_int_eq(out.data4[3], 7);
    ck_assert_int_eq(out.data4[4], 8);
    ck_assert_int_eq(out.data4[5], 9);
    ck_assert_int_eq(out.data4[6], 10);
    ck_assert_int_eq(out.data4[7], 11);
}
END_TEST

START_TEST(UA_DateTime_json_decode) {
    // given
    UA_DateTime out;
    UA_ByteString buf = UA_STRING("\"1970-01-02T01:02:03Z\"");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_DATETIME], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    UA_DateTimeStruct dts = UA_DateTime_toStruct(out);
    ck_assert_int_eq(dts.year, 1970);
    ck_assert_int_eq(dts.month, 1);
    ck_assert_int_eq(dts.day, 2);
    ck_assert_int_eq(dts.hour, 1);
    ck_assert_int_eq(dts.min, 2);
    ck_assert_int_eq(dts.sec, 3);
    ck_assert_int_eq(dts.milliSec, 0);
    ck_assert_int_eq(dts.microSec, 0);
    ck_assert_int_eq(dts.nanoSec, 0);
}
END_TEST


START_TEST(UA_QualifiedName_json_decode) {
    // given
    UA_QualifiedName out;
    UA_ByteString buf = UA_STRING("{\"Name\":\"derName\",\"Uri\":1}");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_QUALIFIEDNAME], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.name.data[1], 'e');
    ck_assert_int_eq(out.name.data[6], 'e');
    ck_assert_int_eq(out.namespaceIndex, 1);
}
END_TEST

START_TEST(UA_LocalizedText_json_decode) {
    // given
    UA_LocalizedText out;
    UA_ByteString buf = UA_STRING("{\"Locale\":\"t1\",\"Text\":\"t2\"}");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.locale.data[1], '1');
    ck_assert_int_eq(out.text.data[1], '2');
}
END_TEST

START_TEST(UA_ViewDescription_json_decode) {
    // given
    UA_ViewDescription out;
    UA_ByteString buf = UA_STRING("{\"Timestamp\":\"1970-01-15T06:56:07Z\",\"ViewVersion\":1236,\"ViewId\":{\"Id\":\"00000009-0002-027C-F3BF-BB7BEEFEEFBE\",\"IdType\":2}}");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_VIEWDESCRIPTION], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.viewVersion, 1236);
    ck_assert_int_eq(out.viewId.identifierType, UA_NODEIDTYPE_GUID);
    UA_DateTimeStruct dts = UA_DateTime_toStruct(out.timestamp);
    ck_assert_int_eq(dts.year, 1970);
    ck_assert_int_eq(dts.month, 1);
    ck_assert_int_eq(dts.day, 15);
    ck_assert_int_eq(dts.hour, 6);
    ck_assert_int_eq(dts.min, 56);
    ck_assert_int_eq(dts.sec, 7);
    ck_assert_int_eq(dts.milliSec, 0);
    ck_assert_int_eq(dts.microSec, 0);
    ck_assert_int_eq(dts.nanoSec, 0);
    ck_assert_int_eq(out.viewId.identifier.guid.data1, 9);
    ck_assert_int_eq(out.viewId.identifier.guid.data2, 2);
}
END_TEST

START_TEST(UA_NodeId_Nummeric_json_decode) {
    // given
    UA_NodeId out;
    UA_ByteString buf = UA_STRING("{\"Id\":42}");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_NODEID], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.identifier.numeric, 42);
}
END_TEST

START_TEST(UA_DataTypeAttributes_json_decode) {
    // given
    UA_DataTypeAttributes out;
    UA_ByteString buf = UA_STRING("{\"SpecifiedAttributes\":1,"
            "\"DisplayName\":{\"Locale\":\"t1\",\"Text\":\"t2\"},"
            "\"Description\":{\"Locale\":\"t3\",\"Text\":\"t4\"},"
            "\"WriteMask\":53,"
            "\"UserWriteMask\":63,"
            "\"IsAbstract\":false}");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_DATATYPEATTRIBUTES], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.isAbstract, 0);
    ck_assert_int_eq(out.writeMask, 53);
    ck_assert_int_eq(out.userWriteMask, 63);
    ck_assert_int_eq(out.specifiedAttributes, 1);
    ck_assert_int_eq(out.displayName.locale.data[1], '1');
    ck_assert_int_eq(out.displayName.text.data[1], '2');
    ck_assert_int_eq(out.description.locale.data[1], '3');
    ck_assert_int_eq(out.description.text.data[1], '4');
}
END_TEST


START_TEST(UA_DiagnosticInfo_json_decode) {
    // given
    
    UA_DiagnosticInfo out;
    out.innerDiagnosticInfo = NULL;
    UA_ByteString buf = UA_STRING("{\"SymbolicId\":13,"
            "\"LocalizedText\":14,"
            "\"Locale\":12,"
            "\"AdditionalInfo\":\"additionalInfo\","
            "\"InnerStatusCode\":2155216896,"
            "\"InnerDiagnosticInfo\":{\"AdditionalInfo\":\"INNER ADDITION INFO\"}}");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_DIAGNOSTICINFO], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.locale, 12);
    ck_assert_int_eq(out.symbolicId, 13);
    ck_assert_int_eq(out.localizedText, 14);
    ck_assert_int_eq(out.innerStatusCode, 2155216896);
    ck_assert_int_eq(out.additionalInfo.length, 14);
    ck_assert_int_eq(out.innerDiagnosticInfo->additionalInfo.length, 19);
}
END_TEST



START_TEST(UA_VariantBool_json_decode) {
    // given
    UA_Variant out;
    UA_ByteString buf = UA_STRING("{\"Type\":0,\"Body\":false}");
    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_VARIANT], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.type->typeIndex, 0);
    ck_assert_uint_eq(*((UA_Boolean*)out.data), 0);
}
END_TEST

START_TEST(UA_VariantStringArray_json_decode) {
    // given
    
    UA_Variant out;
    UA_Variant_init(&out);
    UA_ByteString buf = UA_STRING("{\"Type\":11,\"Body\":[\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\",\"8\"],\"Dimension\":[2,4]}");
    //UA_ByteString buf = UA_STRING("{\"SymbolicId\":13,\"LocalizedText\":14,\"Locale\":12,\"AdditionalInfo\":\"additionalInfo\",\"InnerStatusCode\":2155216896}");

    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_VARIANT], 0, 0);

    UA_String *testArray;
    testArray = (UA_String*)(out.data);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq((char)testArray[0].data[0], '1');
    ck_assert_int_eq((char)testArray[1].data[0], '2');
    ck_assert_int_eq((char)testArray[2].data[0], '3');
    ck_assert_int_eq((char)testArray[3].data[0], '4');
    ck_assert_int_eq((char)testArray[4].data[0], '5');
    ck_assert_int_eq((char)testArray[5].data[0], '6');
    ck_assert_int_eq((char)testArray[6].data[0], '7');
    ck_assert_int_eq((char)testArray[7].data[0], '8');
    ck_assert_int_eq(out.arrayDimensionsSize, 2);
    ck_assert_int_eq(out.arrayDimensions[0], 2);
    ck_assert_int_eq(out.arrayDimensions[1], 4);
    ck_assert_int_eq(out.arrayLength, 8);
    ck_assert_int_eq(out.type->typeIndex, 11);
}
END_TEST

START_TEST(UA_DataValue_json_decode) {
    // given
    
    UA_DataValue out;
    UA_DataValue_init(&out);
    UA_ByteString buf = UA_STRING("{\"Value\":{\"Type\":0,\"Body\":true},\"Status\":2153250816,\"SourceTimestamp\":\"1970-01-15T06:56:07Z\",\"SourcePicoseconds\":0,\"ServerTimestamp\":\"1970-01-15T06:56:07Z\",\"ServerPicoseconds\":0}");

    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_DATAVALUE], 0, 0);
    //UA_DiagnosticInfo inner = *out.innerDiagnosticInfo;

    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST

START_TEST(UA_ExtensionObject_json_decode) {
    // given
    
    UA_ExtensionObject out;
    UA_ExtensionObject_init(&out);
    UA_ByteString buf = UA_STRING("{\"TypeId\":{\"Id\":0},\"Body\":true}");

    // when
    size_t offset = 0;
    UA_StatusCode retval = UA_decodeJson(&buf, &offset, &out, &UA_TYPES[UA_TYPES_EXTENSIONOBJECT], 0, 0);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
}
END_TEST
         
static Suite *testSuite_builtin(void) {
    Suite *s = suite_create("Built-in Data Types 62541-6 Table 1");

    TCase *tc_decode = tcase_create("decode");
    tcase_add_test(tc_decode, UA_Byte_decodeShallCopyAndAdvancePosition);
    tcase_add_test(tc_decode, UA_Byte_decodeShallModifyOnlyCurrentPosition);
    tcase_add_test(tc_decode, UA_Int16_decodeShallAssumeLittleEndian);
    tcase_add_test(tc_decode, UA_Int16_decodeShallRespectSign);
    tcase_add_test(tc_decode, UA_UInt16_decodeShallNotRespectSign);
    tcase_add_test(tc_decode, UA_Int32_decodeShallAssumeLittleEndian);
    tcase_add_test(tc_decode, UA_Int32_decodeShallRespectSign);
    tcase_add_test(tc_decode, UA_UInt32_decodeShallNotRespectSign);
    tcase_add_test(tc_decode, UA_UInt64_decodeShallNotRespectSign);
    tcase_add_test(tc_decode, UA_Int64_decodeShallRespectSign);
    tcase_add_test(tc_decode, UA_Float_decodeShallWorkOnExample);
    tcase_add_test(tc_decode, UA_Double_decodeShallGiveOne);
    tcase_add_test(tc_decode, UA_Double_decodeShallGiveZero);
    tcase_add_test(tc_decode, UA_Double_decodeShallGiveMinusTwo);
    tcase_add_test(tc_decode, UA_Double_decodeShallGive2147483648);
    tcase_add_test(tc_decode, UA_Byte_encode_test);
    tcase_add_test(tc_decode, UA_String_decodeShallAllocateMemoryAndCopyString);
    tcase_add_test(tc_decode, UA_String_decodeWithNegativeSizeShallNotAllocateMemoryAndNullPtr);
    tcase_add_test(tc_decode, UA_String_decodeWithZeroSizeShallNotAllocateMemoryAndNullPtr);
    tcase_add_test(tc_decode, UA_NodeId_decodeTwoByteShallReadTwoBytesAndSetNamespaceToZero);
    tcase_add_test(tc_decode, UA_NodeId_decodeFourByteShallReadFourBytesAndRespectNamespace);
    tcase_add_test(tc_decode, UA_NodeId_decodeStringShallAllocateMemory);
    tcase_add_test(tc_decode, UA_Variant_decodeSingleExtensionObjectShallSetVTAndAllocateMemory);
    tcase_add_test(tc_decode, UA_Variant_decodeWithOutArrayFlagSetShallSetVTAndAllocateMemoryForArray);
    tcase_add_test(tc_decode, UA_Variant_decodeWithArrayFlagSetShallSetVTAndAllocateMemoryForArray);
    tcase_add_test(tc_decode, UA_Variant_decodeWithOutDeleteMembersShallFailInCheckMem);
    tcase_add_test(tc_decode, UA_Variant_decodeWithTooSmallSourceShallReturnWithError);
    suite_add_tcase(s, tc_decode);

    TCase *tc_encode = tcase_create("encode");
    tcase_add_test(tc_encode, UA_Byte_encode_test);
    tcase_add_test(tc_encode, UA_UInt16_encodeNegativeShallEncodeLittleEndian);
    tcase_add_test(tc_encode, UA_UInt16_encodeShallEncodeLittleEndian);
    tcase_add_test(tc_encode, UA_UInt32_encodeShallEncodeLittleEndian);
    tcase_add_test(tc_encode, UA_Int32_encodeShallEncodeLittleEndian);
    tcase_add_test(tc_encode, UA_Int32_encodeNegativeShallEncodeLittleEndian);
    tcase_add_test(tc_encode, UA_UInt64_encodeShallWorkOnExample);
    tcase_add_test(tc_encode, UA_Int64_encodeNegativeShallEncodeLittleEndian);
    tcase_add_test(tc_encode, UA_Int64_encodeShallEncodeLittleEndian);
    tcase_add_test(tc_encode, UA_Float_encodeShallWorkOnExample);
    tcase_add_test(tc_encode, UA_Double_encodeShallWorkOnExample);
    tcase_add_test(tc_encode, UA_String_encodeShallWorkOnExample);
    tcase_add_test(tc_encode, UA_ExpandedNodeId_encodeShallWorkOnExample);
    tcase_add_test(tc_encode, UA_DataValue_encodeShallWorkOnExampleWithoutVariant);
    tcase_add_test(tc_encode, UA_DataValue_encodeShallWorkOnExampleWithVariant);
    tcase_add_test(tc_encode, UA_ExtensionObject_encodeDecodeShallWorkOnExtensionObject);
    suite_add_tcase(s, tc_encode);

    TCase *tc_convert = tcase_create("convert");
    tcase_add_test(tc_convert, UA_DateTime_toStructShallWorkOnExample);
    suite_add_tcase(s, tc_convert);

    TCase *tc_copy = tcase_create("copy");
    tcase_add_test(tc_copy, UA_Array_copyByteArrayShallWorkOnExample);
    tcase_add_test(tc_copy, UA_Array_copyUA_StringShallWorkOnExample);
    tcase_add_test(tc_copy, UA_ExtensionObject_copyShallWorkOnExample);
    tcase_add_test(tc_copy, UA_Variant_copyShallWorkOnSingleValueExample);
    tcase_add_test(tc_copy, UA_Variant_copyShallWorkOn1DArrayExample);
    tcase_add_test(tc_copy, UA_Variant_copyShallWorkOn2DArrayExample);
    tcase_add_test(tc_copy, UA_Variant_copyShallWorkOnByteStringIndexRange);

    tcase_add_test(tc_copy, UA_DiagnosticInfo_copyShallWorkOnExample);
    tcase_add_test(tc_copy, UA_ApplicationDescription_copyShallWorkOnExample);
    tcase_add_test(tc_copy, UA_QualifiedName_copyShallWorkOnInputExample);
    tcase_add_test(tc_copy, UA_Guid_copyShallWorkOnInputExample);
    tcase_add_test(tc_copy, UA_LocalizedText_copycstringShallWorkOnInputExample);
    tcase_add_test(tc_copy, UA_DataValue_copyShallWorkOnInputExample);
    suite_add_tcase(s, tc_copy);
    
    
    TCase *tc_json_encode = tcase_create("json_encode");
    tcase_add_test(tc_json_encode, UA_Boolean_true_json_encode);
    tcase_add_test(tc_json_encode, UA_Boolean_false_json_encode);
    tcase_add_test(tc_json_encode, UA_String_json_encode);
    tcase_add_test(tc_json_encode, UA_UInt16_PosNumber_json_encode);
    tcase_add_test(tc_json_encode, UA_UInt32_PosNumber_json_encode);
    tcase_add_test(tc_json_encode, UA_UInt64_PosNumber_json_encode);
    tcase_add_test(tc_json_encode, UA_Int16_NegNumber_json_encode);
    tcase_add_test(tc_json_encode, UA_Int32_NegNumber_json_encode);
    tcase_add_test(tc_json_encode, UA_Int64_NegNumber_json_encode);
    tcase_add_test(tc_json_encode, UA_LocText_json_encode);
    tcase_add_test(tc_json_encode, UA_Guid_json_encode);
    tcase_add_test(tc_json_encode, UA_DateTime_json_encode);
    tcase_add_test(tc_json_encode, UA_StatusCode_json_encode);
    tcase_add_test(tc_json_encode, UA_NodeId_Nummeric_json_encode);
    tcase_add_test(tc_json_encode, UA_NodeId_String_json_encode);
    tcase_add_test(tc_json_encode, UA_NodeId_Guid_json_encode);
    tcase_add_test(tc_json_encode, UA_NodeId_ByteString_json_encode);
    tcase_add_test(tc_json_encode, UA_DiagInfo_json_encode);
    tcase_add_test(tc_json_encode, UA_ByteString_json_encode);
    tcase_add_test(tc_json_encode, UA_QualName_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_Bool_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_Number_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_NodeId_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_LocText_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_QualName_json_encode);
    tcase_add_test(tc_json_encode, UA_ExtensionObject_json_encode);
    tcase_add_test(tc_json_encode, UA_ExpandedNodeId_json_encode);
    tcase_add_test(tc_json_encode, UA_DataValue_json_encode);
    tcase_add_test(tc_json_encode, UA_MessageReadResponse_json_encode);
    tcase_add_test(tc_json_encode, UA_ViewDescription_json_encode);
    tcase_add_test(tc_json_encode, UA_WriteRequest_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_Array_UInt16_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_Array_Byte_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_Array_String_json_encode);
    tcase_add_test(tc_json_encode, UA_Variant_Matrix_UInt16_json_encode);
    //tcase_add_test(tc_json_encode, UA_Variant_Matrix_String_NonReversible_json_encode);
    suite_add_tcase(s, tc_json_encode);
    
    TCase *tc_json_decode = tcase_create("json_decode");
    tcase_add_test(tc_json_decode, UA_UInt16_json_decode);
    tcase_add_test(tc_json_decode, UA_UInt32_json_decode);
    tcase_add_test(tc_json_decode, UA_UInt64_json_decode);
    tcase_add_test(tc_json_decode, UA_String_json_decode);
    tcase_add_test(tc_json_decode, UA_ByteString_json_decode);
    tcase_add_test(tc_json_decode, UA_DateTime_json_decode);
    tcase_add_test(tc_json_decode, UA_Guid_json_decode);
    tcase_add_test(tc_json_decode, UA_QualifiedName_json_decode);
    tcase_add_test(tc_json_decode, UA_LocalizedText_json_decode);
    tcase_add_test(tc_json_decode, UA_ViewDescription_json_decode);
    tcase_add_test(tc_json_decode, UA_NodeId_Nummeric_json_decode); 
    tcase_add_test(tc_json_decode, UA_DataTypeAttributes_json_decode);
    tcase_add_test(tc_json_decode, UA_DiagnosticInfo_json_decode);
    tcase_add_test(tc_json_decode, UA_VariantBool_json_decode);
    tcase_add_test(tc_json_decode, UA_VariantStringArray_json_decode);
    tcase_add_test(tc_json_decode, UA_DataValue_json_decode);
    tcase_add_test(tc_json_decode, UA_ExtensionObject_json_decode);
    suite_add_tcase(s, tc_json_decode);
    return s;
}

int main(void) {
    int      number_failed = 0;
    Suite   *s;
    SRunner *sr;

    s  = testSuite_builtin();
    sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed += srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
