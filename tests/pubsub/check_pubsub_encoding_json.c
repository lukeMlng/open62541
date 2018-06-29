/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017 - 2018 Fraunhofer IOSB (Author: Tino Bischoff)
 */

#include "ua_types.h"
#include "ua_client.h"
#include "ua_util.h"
#include "ua_pubsub_networkmessage.h"
#include "check.h"


//-----------------------------------PubSub---------------------------------------


START_TEST(UA_PubSub_EnDecode) {
    UA_NetworkMessage m;
    memset(&m, 0, sizeof(UA_NetworkMessage));
    m.version = 1;
    m.networkMessageType = UA_NETWORKMESSAGE_DATASET;
    m.payloadHeaderEnabled = true;
    m.payloadHeader.dataSetPayloadHeader.count = 2;
    UA_UInt16 dsWriter1 = 4;
    UA_UInt16 dsWriter2 = 7;
    m.payloadHeader.dataSetPayloadHeader.dataSetWriterIds = (UA_UInt16 *)UA_Array_new(m.payloadHeader.dataSetPayloadHeader.count, &UA_TYPES[UA_TYPES_UINT16]);
    m.payloadHeader.dataSetPayloadHeader.dataSetWriterIds[0] = dsWriter1;
    m.payloadHeader.dataSetPayloadHeader.dataSetWriterIds[1] = dsWriter2;

    size_t memsize = m.payloadHeader.dataSetPayloadHeader.count * sizeof(UA_DataSetMessage);
    m.payload.dataSetPayload.dataSetMessages = (UA_DataSetMessage*)UA_malloc(memsize);
    memset(m.payload.dataSetPayload.dataSetMessages, 0, memsize);

    //UA_DataSetMessage dmkf;
    //memset(&dmkf, 0, sizeof(UA_DataSetMessage));
    m.payload.dataSetPayload.dataSetMessages[0].header.dataSetMessageValid = true;
    m.payload.dataSetPayload.dataSetMessages[0].header.fieldEncoding = UA_FIELDENCODING_VARIANT;
    m.payload.dataSetPayload.dataSetMessages[0].header.dataSetMessageType = UA_DATASETMESSAGE_DATAKEYFRAME;
    UA_UInt16 fieldCountDS1 = 1;
    m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.fieldCount = fieldCountDS1;
    m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields =
        (UA_DataValue*)UA_Array_new(m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.fieldCount, &UA_TYPES[UA_TYPES_DATAVALUE]);
    UA_DataValue_init(&m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0]);

    UA_UInt32 iv = 27;
    UA_Variant_setScalarCopy(&m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].value, &iv, &UA_TYPES[UA_TYPES_UINT32]);
    m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].hasValue = true;

    m.payload.dataSetPayload.dataSetMessages[1].header.dataSetMessageValid = true;
    m.payload.dataSetPayload.dataSetMessages[1].header.fieldEncoding = UA_FIELDENCODING_DATAVALUE;
    m.payload.dataSetPayload.dataSetMessages[1].header.dataSetMessageType = UA_DATASETMESSAGE_DATADELTAFRAME;
    UA_UInt16 fieldCountDS2 = 2;
    m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.fieldCount = fieldCountDS2;
    memsize = sizeof(UA_DataSetMessage_DeltaFrameField) * m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.fieldCount;
    m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields = (UA_DataSetMessage_DeltaFrameField*)UA_malloc(memsize);

    UA_Guid gv = UA_Guid_random();
    UA_UInt16 fieldIndex1 = 2;
    m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldIndex = fieldIndex1;
    UA_DataValue_init(&m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue);
    UA_Variant_setScalar(&m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.value, &gv, &UA_TYPES[UA_TYPES_GUID]);
    m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.hasValue = true;

    UA_UInt16 fieldIndex2 = 5;
    m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldIndex = fieldIndex2;
    UA_DataValue_init(&m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue);
    UA_Int64 iv64 = 152478978534;
    UA_Variant_setScalar(&m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.value, &iv64, &UA_TYPES[UA_TYPES_INT64]);
    m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.hasValue = true;

    UA_StatusCode rv = UA_STATUSCODE_UNCERTAININITIALVALUE;
    UA_ByteString buffer;
    size_t msgSize = 1000;
    rv = UA_ByteString_allocBuffer(&buffer, msgSize);
    ck_assert_int_eq(rv, UA_STATUSCODE_GOOD);

    UA_Byte *bufPos = buffer.data;
    memset(bufPos, 0, msgSize);
    const UA_Byte *bufEnd = &(buffer.data[buffer.length]);
    
    UA_String a = UA_STRING("a");
    UA_String b = UA_STRING("b");
    UA_String** dataSetMessageFields[2];
    
    UA_String* dataSetMessageFieldNames[2];
    dataSetMessageFields[0] = dataSetMessageFieldNames;
    dataSetMessageFieldNames[0] = &a;
    dataSetMessageFieldNames[1] = &b;
    
    dataSetMessageFields[1] = dataSetMessageFieldNames;
    rv = UA_NetworkMessage_encodeJson(&m, &bufPos, bufEnd, UA_TRUE, dataSetMessageFields, 0);
    *bufPos = 0;
    // then
    ck_assert_int_eq(rv, UA_STATUSCODE_GOOD);
    //char* result = "{\"MessageId\":\"D4195B44-2E0A-8D5B-46F4-BF9B1CB1BB0B\",\"MessageType\":\"ua-data\",\"Messages\":[{\"DataSetWriterId\":\"4\",\"Payload\":[{\"Type\":8,\"Body\":27}]},{\"DataSetWriterId\":\"7\",\"Payload\":[{\"Value\":{\"Type\":13,\"Body\":\"B7E9851D-2E4D-E71F-7107-A02AF23F5375\"}},{\"Value\":{\"Type\":7,\"Body\":152478978534}}]}]}";
    //ck_assert_str_eq(result, (char*)buffer.data);
    //"{\"MessageId\":\"D4195B44-2E0A-8D5B-46F4-BF9B1CB1BB0B\",\"MessageType\":\"ua-data\",\"Messages\":[{\"DataSetWriterId\":\"0\",\"Payload\":{\"a\":{\"Type\":7,\"Body\":27}}},{\"DataSetWriterId\":\"0\",\"Payload\":{\"a\":{\"Value\":{\"Type\":13,\"Body\":\"B7E9851D-2E4D-E71F-7107-A02AF23F5375\"}},\"b\":{\"Value\":{\"Type\":7,\"Body\":152478978534}}}}]}"
    
    //---------------
    UA_NetworkMessage m2;
    memset(&m2, 0, sizeof(UA_NetworkMessage));
    rv = NetworkMessage_decodeJson(&m2, &buffer);
    ck_assert_int_eq(rv, UA_STATUSCODE_GOOD);
    // Version not in json? ck_assert(m.version == m2.version);  
    ck_assert(m.networkMessageType == m2.networkMessageType);
    ck_assert(m.timestampEnabled == m2.timestampEnabled);
    ck_assert(m.dataSetClassIdEnabled == m2.dataSetClassIdEnabled);
    ck_assert(m.groupHeaderEnabled == m2.groupHeaderEnabled);
    ck_assert(m.picosecondsEnabled == m2.picosecondsEnabled);
    ck_assert(m.promotedFieldsEnabled == m2.promotedFieldsEnabled);
    ck_assert(m.publisherIdEnabled == m2.publisherIdEnabled);
    ck_assert(m.securityEnabled == m2.securityEnabled);
    ck_assert(m.chunkMessage == m2.chunkMessage);
    ck_assert(m.payloadHeaderEnabled == m2.payloadHeaderEnabled);
    //TODO: ck_assert_uint_eq(m2.payloadHeader.dataSetPayloadHeader.dataSetWriterIds[0], dsWriter1);
    //TODO: ck_assert_uint_eq(m2.payloadHeader.dataSetPayloadHeader.dataSetWriterIds[1], dsWriter2);
    ck_assert(m.payload.dataSetPayload.dataSetMessages[0].header.dataSetMessageValid == m2.payload.dataSetPayload.dataSetMessages[0].header.dataSetMessageValid);
    ck_assert(m.payload.dataSetPayload.dataSetMessages[0].header.fieldEncoding == m2.payload.dataSetPayload.dataSetMessages[0].header.fieldEncoding);
    ck_assert_int_eq(m2.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.fieldCount, fieldCountDS1);
    ck_assert(m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].hasValue == m2.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].hasValue);
    ck_assert_uint_eq((uintptr_t)m2.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].value.type, (uintptr_t)&UA_TYPES[UA_TYPES_UINT32]);
    ck_assert_uint_eq(*(UA_UInt32 *)m2.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].value.data, iv);
    ck_assert(m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].hasSourceTimestamp == m2.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].hasSourceTimestamp);

    ck_assert(m.payload.dataSetPayload.dataSetMessages[1].header.dataSetMessageValid == m2.payload.dataSetPayload.dataSetMessages[1].header.dataSetMessageValid);
    ck_assert(m.payload.dataSetPayload.dataSetMessages[1].header.fieldEncoding == m2.payload.dataSetPayload.dataSetMessages[1].header.fieldEncoding);
    ck_assert_int_eq(m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.fieldCount, fieldCountDS2);

    //TODO: ck_assert(m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.hasValue == m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.hasValue);
    //ck_assert_uint_eq(m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldIndex, fieldIndex1);
    //ck_assert_uint_eq((uintptr_t)m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.value.type, (uintptr_t)&UA_TYPES[UA_TYPES_GUID]);
    //SEGF ck_assert(UA_Guid_equal((UA_Guid*)m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.value.data, &gv) == true);
    ck_assert(m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.hasSourceTimestamp == m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[0].fieldValue.hasSourceTimestamp);

    //ck_assert(m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.hasValue == m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.hasValue);
    //ck_assert_uint_eq(m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldIndex, fieldIndex2);
    //ck_assert_uint_eq((uintptr_t)m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.value.type, (uintptr_t)&UA_TYPES[UA_TYPES_INT64]);
    //ck_assert_int_eq(*(UA_Int64 *)m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.value.data, iv64);
    //ck_assert(m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.hasSourceTimestamp == m2.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields[1].fieldValue.hasSourceTimestamp);

    //UA_Array_delete(m.payloadHeader.dataSetPayloadHeader.dataSetWriterIds, m.payloadHeader.dataSetPayloadHeader.count, &UA_TYPES[UA_TYPES_UINT16]);
    //UA_Array_delete(m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields, m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.fieldCount, &UA_TYPES[UA_TYPES_DATAVALUE]);
    //UA_free(m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields);
    
    //----
    //UA_NetworkMessage_deleteMembers(&m);
    UA_ByteString_deleteMembers(&buffer);
    
    //UA_Array_delete(dmkf.data.keyFrameData.dataSetFields, dmkf.data.keyFrameData.fieldCount, &UA_TYPES[UA_TYPES_DATAVALUE]);
    UA_free(m.payloadHeader.dataSetPayloadHeader.dataSetWriterIds);
    UA_free(m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].value.data);
    UA_free(m.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields);
    UA_free(m.payload.dataSetPayload.dataSetMessages[1].data.deltaFrameData.deltaFrameFields);
    UA_free(m.payload.dataSetPayload.dataSetMessages);
}
END_TEST


START_TEST(UA_NetworkMessage_oneMessage_twoFields_json_decode) {
    // given
    UA_NetworkMessage out;
    UA_ByteString buf = UA_STRING("{\"MessageId\":\"5ED82C10-50BB-CD07-0120-22521081E8EE\",\"MessageType\":\"ua-data\",\"Messages\":[{\"DataSetWriterId\":\"62541\",\"MetaDataVersion\":{\"MajorVersion\":1478393530,\"MinorVersion\":0},\"SequenceNumber\":4711,\"Payload\":{\"Test\":{\"Type\":5,\"Body\":42},\"Server localtime\":{\"Type\":13,\"Body\":\"2018-06-05T05:58:36.000Z\"}}}]}");
    // when
    UA_StatusCode retval = NetworkMessage_decodeJson(&out, &buf);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.payload.dataSetPayload.dataSetMessages[0].header.dataSetMessageSequenceNr, 4711);
    //ck_assert_int_eq(out.payload.dataSetPayload.dataSetMessages[0].header.dataSetWriterId, 62541);
    ck_assert_int_eq(out.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].hasValue, 1);
    ck_assert_int_eq(*((UA_UInt16*)out.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].value.data), 42);
    ck_assert_int_eq(out.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[1].hasValue, 1);
    UA_DateTime *dt = (UA_DateTime*)out.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[1].value.data;
    UA_DateTimeStruct dts = UA_DateTime_toStruct(*dt);
    //2018-06-05T05:58:36
    ck_assert_int_eq(dts.year, 2018);
    ck_assert_int_eq(dts.month, 6);
    ck_assert_int_eq(dts.day, 5);
    ck_assert_int_eq(dts.hour, 5);
    ck_assert_int_eq(dts.min, 58);
    ck_assert_int_eq(dts.sec, 36);
    ck_assert_int_eq(dts.milliSec, 0);
    ck_assert_int_eq(dts.microSec, 0);
    ck_assert_int_eq(dts.nanoSec, 0);
    
    UA_NetworkMessage_deleteMembers(&out);
    //UA_free(out.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[0].value.data);
    //UA_free(out.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields[1].value.data);
    //UA_free(out.payload.dataSetPayload.dataSetMessages[0].data.keyFrameData.dataSetFields);
    //UA_free(out.payload.dataSetPayload.dataSetMessages);
}
END_TEST
 

START_TEST(UA_NetworkMessage_MetaDataVersion_json_decode) {
    // given
    UA_NetworkMessage out;
    UA_ByteString buf = UA_STRING("{\"MessageId\":\"5ED82C10-50BB-CD07-0120-22521081E8EE\",\"MessageType\":\"ua-data\",\"Messages\":[{\"MetaDataVersion\":{\"MajorVersion\": 47, \"MinorVersion\": 47},\"DataSetWriterId\":\"62541\",\"Status\":22,\"SequenceNumber\":4711,\"Payload\":{\"Test\":{\"Type\":5,\"Body\":42},\"Server localtime\":{\"Type\":13,\"Body\":\"2018-06-05T05:58:36.000Z\"}}}]}");
    // when
    UA_StatusCode retval = NetworkMessage_decodeJson(&out, &buf);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    UA_NetworkMessage_deleteMembers(&out);
}
END_TEST

/*
START_TEST(UA_NetworkMessage_test_json_decode) {
    // given
    UA_NetworkMessage out;
    UA_ByteString buf = UA_STRING("{ \"MessageId\": \"32235546-05d9-4fd7-97df-ea3ff3408574\", \"MessageType\": \"ua-data\", \"PublisherId\": \"MQTT-Localhost\", \"DataSetClassId\": \"2dc07ece-cab9-4470-8f8a-2c1ead207e0e\", \"Messages\": [ "
            "{ \"DataSetWriterId\": \"1\", \"SequenceNumber\": 224, \"MetaDataVersion\": { \"MajorVersion\": 1, \"MinorVersion\": 1 }, \"Payload\": "
            "{\"BoilerId\": { \"Value\": { \"Type\": 11,\"Body\": \"Boiler #1\"},\"SourceTimestamp\": \"2018-03-25T13:32:20.000Z\"},"
            "\"DrumLevel\": { \"Value\": { \"Type\": 6,\"Body\": 99},\"SourceTimestamp\": \"2018-03-25T13:32:20.000Z\"},"
            "\"DrumLevel.EURange\": { \"Value\": { \"Type\": 21,\"Body\": {\"TypeId\": { \"Id\": 0 }, \"Body\": true } },\"SourceTimestamp\": \"2018-03-25T13:07:36.000Z\"},"
            "\"DrumLevel.EngineeringUnits\": { \"Value\": { \"Type\": 0,\"Body\": true }, \"SourceTimestamp\": \"2018-03-25T13:07:36.000Z\"},"
            "\"FlowSetPoint\": { \"Value\": { \"Type\": 6,\"Body\": 2},\"SourceTimestamp\": \"2018-03-25T13:31:43.000Z\"},\"LevelSetPoint\": { \"Value\": { \"Type\": 6,\"Body\": 2},\"SourceTimestamp\": \"2018-03-25T13:31:29Z\"},"
            "\"InputPipeFlow\": { \"Value\": { \"Type\": 6,\"Body\": 75},\"SourceTimestamp\": \"2018-03-25T13:32:19.000Z\"},"
            "\"OutputPipeFlow\": { \"Value\": { \"Type\": 6,\"Body\": 85},\"SourceTimestamp\": \"2018-03-25T13:32:19.000Z\"} } } ] }");
    // when
    UA_StatusCode retval = NetworkMessage_decodeJson(&out, &buf);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    
    
    
}
END_TEST
*/


START_TEST(UA_Networkmessage_json_decode) {
    // given
    
    UA_NetworkMessage out;
    memset(&out, 0, sizeof(UA_NetworkMessage));
    UA_ByteString buf = UA_STRING("{ \"MessageId\": \"32235546-05d9-4fd7-97df-ea3ff3408574\",  \"MessageType\": \"ua-data\",  \"PublisherId\": \"MQTT-Localhost\",  \"DataSetClassId\": \"00000005-cab9-4470-8f8a-2c1ead207e0e\",  \"Messages\": [    {      \"DataSetWriterId\": \"1\",      \"SequenceNumber\": 224,     \"MetaDataVersion\": {        \"MajorVersion\": 1,        \"MinorVersion\": 1      },\"Payload\":null}]}");

    // when
    UA_StatusCode retval = NetworkMessage_decodeJson(&out, &buf);
    // then
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    ck_assert_int_eq(out.dataSetClassId.data1, 5);
    ck_assert_int_eq(out.payload.dataSetPayload.dataSetMessages->header.dataSetMessageSequenceNr, 224);
    ck_assert_ptr_eq(out.payload.dataSetPayload.dataSetMessages->data.keyFrameData.dataSetFields, NULL);

    UA_free(out.payload.dataSetPayload.dataSetMessages);
}   
END_TEST


static Suite *testSuite_networkmessage(void) {
    Suite *s = suite_create("Built-in Data Types 62541-6 Json");
    TCase *tc_json_networkmessage = tcase_create("networkmessage_json");
    
    tcase_add_test(tc_json_networkmessage, UA_NetworkMessage_oneMessage_twoFields_json_decode);
    tcase_add_test(tc_json_networkmessage, UA_Networkmessage_json_decode);
    //tcase_add_test(tc_json_decode, UA_NetworkMessage_test_json_decode);
    tcase_add_test(tc_json_networkmessage, UA_PubSub_EnDecode);
    tcase_add_test(tc_json_networkmessage, UA_NetworkMessage_MetaDataVersion_json_decode);
    
    
    suite_add_tcase(s, tc_json_networkmessage);
    return s;
}



int main(void) {
    int      number_failed = 0;
    Suite   *s;
    SRunner *sr;

    s  = testSuite_networkmessage();
    sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed += srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
