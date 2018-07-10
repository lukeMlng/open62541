/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

/**
 * .. _pubsub-tutorial:
 *
 * Working with Publish/Subscribe
 * ------------------------------
 *
 * Work in progress:
 * This Tutorial will be continuously extended during the next PubSub batches. More details about
 * the PubSub extension and corresponding open62541 API are located here: :ref:`pubsub`.
 *
 * Publishing Fields
 * ^^^^^^^^^^^^^^^^^
 * The PubSub publish example demonstrate the simplest way to publish
 * informations from the information model over UDP multicast using
 * the UADP encoding.
 *
 * **Connection handling**
 * PubSubConnections can be created and deleted on runtime. More details about the system preconfiguration and
 * connection can be found in ``tutorial_pubsub_connection.c``.
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <signal.h>

#include <sys/time.h>

#include "ua_pubsub_networkmessage.h"
#include "ua_log_stdout.h"
#include "ua_server.h"
#include "ua_config_default.h"
#include "ua_pubsub.h"
#include "ua_network_pubsub_mqtt.h"
#include "src_generated/ua_types_generated.h"

#include <signal.h>
//#include "open62541.h"
#include "../../include/ua_plugin_mqtt.h"



UA_NodeId connectionIdent, publishedDataSetIdent, writerGroupIdent;


static void
addPubSubConnection(UA_Server *server){
    /* Details about the connection configuration and handling are located
     * in the pubsub connection tutorial */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("MQTT Connection 1");
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt-json");
    connectionConfig.enabled = UA_TRUE;    
    //UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL , UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL , UA_STRING("opc.udp://127.0.0.1:1883/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl, &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.numeric = UA_UInt32_random();
    
    UA_KeyValuePair connectionOptions[1];
    connectionOptions[0].key = UA_QUALIFIEDNAME(0, "mqttClientId");
    UA_String mqttClientId = UA_STRING("TESTCLIENTPUBMQTT");
    UA_Variant_setScalar(&connectionOptions[0].value, &mqttClientId, &UA_TYPES[UA_TYPES_STRING]);
    
    connectionConfig.connectionProperties = connectionOptions;
    connectionConfig.connectionPropertiesSize = 1;
    
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdent);
}

/**
 * **PublishedDataSet handling**
 * The PublishedDataSet (PDS) and PubSubConnection are the toplevel entities and can exist alone. The PDS contains
 * the collection of the published fields.
 * All other PubSub elements are directly or indirectly linked with the PDS or connection.
 */
static void
addPublishedDataSet(UA_Server *server) {
    /* The PublishedDataSetConfig contains all necessary public
    * informations for the creation of a new PublishedDataSet */
    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("Demo PDS");
    /* Create new PublishedDataSet based on the PublishedDataSetConfig. */
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig, &publishedDataSetIdent);
}

static void
addVariable(UA_Server *server) {
    // Define the attribute of the myInteger variable node 
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Int32 myInteger = 42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
    attr.description = UA_LOCALIZEDTEXT("en-US","the answer");
    attr.displayName = UA_LOCALIZEDTEXT("en-US","the answer");
    attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    // Add the variable node to the information model 
    UA_NodeId myIntegerNodeId = UA_NODEID_NUMERIC(1, 42);
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
}

/**
 * **DataSetField handling**
 * The DataSetField (DSF) is part of the PDS and describes exactly one published field.
 */
static void
addDataSetField(UA_Server *server) {
    /* Add a field to the previous created PublishedDataSet */
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    //FIeld key json? @@@
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("Server localtime");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
    UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_SERVERSTATUS_CURRENTTIME);
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dataSetFieldConfig, &dataSetFieldIdent);
    
    UA_NodeId dataSetFieldIdent2;
    UA_DataSetFieldConfig dataSetFieldConfig2;
    memset(&dataSetFieldConfig2, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig2.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    //FIeld key json? @@@
    dataSetFieldConfig2.field.variable.fieldNameAlias = UA_STRING("Test");
    dataSetFieldConfig2.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig2.field.variable.publishParameters.publishedVariable = UA_NODEID_NUMERIC(1, 42);

    dataSetFieldConfig2.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dataSetFieldConfig2, &dataSetFieldIdent2);
}

/**
 * **WriterGroup handling**
 * The WriterGroup (WG) is part of the connection and contains the primary configuration
 * parameters for the message creation.
 */
static void
addWriterGroup(UA_Server *server) {
    /* Now we create a new WriterGroupConfig and add the group to the existing PubSubConnection. */
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = 500;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.writerGroupId = 100;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_JSON;
    
    UA_BrokerWriterGroupTransportDataType brokerTransportSettings; //UA_BrokerConnectionTransportDataType_new();
    memset(&brokerTransportSettings, 0, sizeof(UA_BrokerWriterGroupTransportDataType));
    brokerTransportSettings.queueName = UA_STRING("customTopic");
    brokerTransportSettings.resourceUri = UA_STRING_NULL;
    brokerTransportSettings.authenticationProfileUri = UA_STRING_NULL;
    brokerTransportSettings.requestedDeliveryGuarantee = UA_BROKERTRANSPORTQUALITYOFSERVICE_BESTEFFORT;
    
    UA_ExtensionObject transportSettings;
    memset(&transportSettings, 0, sizeof(UA_ExtensionObject));
    transportSettings.encoding = UA_EXTENSIONOBJECT_DECODED;
    transportSettings.content.decoded.type = &UA_TYPES[UA_TYPES_BROKERWRITERGROUPTRANSPORTDATATYPE];
    transportSettings.content.decoded.data = &brokerTransportSettings;
    
    writerGroupConfig.transportSettings = transportSettings;
    /* The configuration flags for the messages are encapsulated inside the
     * message- and transport settings extension objects. These extension objects
     * are defined by the standard. e.g. UadpWriterGroupMessageDataType */
    UA_Server_addWriterGroup(server, connectionIdent, &writerGroupConfig, &writerGroupIdent);
}

/**
 * **DataSetWriter handling**
 * A DataSetWriter (DSW) is the glue between the WG and the PDS. The DSW is linked to exactly one
 * PDS and contains additional informations for the message generation.
 */
static void
addDataSetWriter(UA_Server *server) {
    /* We need now a DataSetWriter within the WriterGroup. This means we must
     * create a new DataSetWriterConfig and add call the addWriterGroup function. */
    UA_NodeId dataSetWriterIdent;
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = 62541;
    dataSetWriterConfig.keyFrameCount = 10;
    
    UA_JsonDataSetWriterMessageDataType jsonDswMd;
    jsonDswMd.dataSetMessageContentMask = (UA_JsonDataSetMessageContentMask)
            (UA_JSONDATASETMESSAGECONTENTMASK_DATASETWRITERID 
            | UA_JSONDATASETMESSAGECONTENTMASK_SEQUENCENUMBER
            | UA_JSONDATASETMESSAGECONTENTMASK_STATUS
            | UA_JSONDATASETMESSAGECONTENTMASK_METADATAVERSION
            | UA_JSONDATASETMESSAGECONTENTMASK_TIMESTAMP);
    
    UA_ExtensionObject messageSettings;
    messageSettings.encoding = UA_EXTENSIONOBJECT_DECODED;
    messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_JSONDATASETWRITERMESSAGEDATATYPE];
    messageSettings.content.decoded.data = &jsonDswMd;
    /*==  ||
        dataSetWriter->config.messageSettings.encoding == UA_EXTENSIONOBJECT_DECODED_NODELETE) &&
       (dataSetWriter->config.messageSettings.content.decoded.type == &UA_TYPES[UA_TYPES_JSONDATASETWRITERMESSAGEDATATYPE])) {
        jsonDataSetWriterMessageDataType = (UA_JsonDataSetWriterMessageDataType *)
            dataSetWriter->config.messageSettings.content.decoded.data;
        messageType = UA_TYPES_JSONDATASETWRITERMESSAGEDATATYPE;
    */
    
    dataSetWriterConfig.messageSettings = messageSettings;
    
    
    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

/**
 * That's it! You're now publishing the selected fields.
 * Open a packet inspection tool of trust e.g. wireshark and take a look on the outgoing packages.
 * The following graphic figures out the packages created by this tutorial.
 *
 * .. figure:: ua-wireshark-pubsub.png
 *     :figwidth: 100 %
 *     :alt: OPC UA PubSub communication in wireshark
 *
 * The open62541 subscriber API will be released later. If you want to process the the datagrams,
 * take a look on the ua_network_pubsub_networkmessage.c which already contains the decoding code for UADP messages.
 *
 * It follows the main server code, making use of the above definitions. */
UA_Boolean running = true;
static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

int subCount = 0;

static void
mqttPollingCallback(UA_Server *server, UA_PubSubConnection *connection) {
    
    /*UA_ByteString buffer;
    if (UA_ByteString_allocBuffer(&buffer, 512) != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                     "Message buffer allocation failed!");
        return;
    }

    buffer.length = 0;
    // Receive the message. Blocks for 5ms 
    UA_StatusCode retval =
        connection->channel->receive(connection->channel, &buffer, NULL, 5);
    if(retval != UA_STATUSCODE_GOOD || buffer.length == 0) {

        buffer.length = 512;
        //UA_ByteString_deleteMembers(&buffer);
        return;
    }else{
        subCount++;
        printf("%d", subCount);
        buffer.length = 512;
        //UA_ByteString_deleteMembers(&buffer);
    }*/

    connection->channel->yield(connection->channel);
}

static void callback(UA_ByteString *encodedBuffer, UA_ByteString *topic){
     UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "callback!");
     
     UA_ByteString_delete(encodedBuffer);
     UA_ByteString_delete(topic);
}

int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_ServerConfig *config = UA_ServerConfig_new_default();
    /* Details about the connection configuration and handling are located in the pubsub connection tutorial */
    config->pubsubTransportLayers = (UA_PubSubTransportLayer *) UA_malloc(1  * sizeof(UA_PubSubTransportLayer));
    if(!config->pubsubTransportLayers) {
        UA_ServerConfig_delete(config);
        return -1;
    }
    
    //config->pubsubTransportLayers[0] = UA_PubSubTransportLayerUDPMP();
    //config->pubsubTransportLayersSize++;
    config->pubsubTransportLayers[0] = UA_PubSubTransportLayerMQTT();
    config->pubsubTransportLayersSize++;
    
    UA_Server *server = UA_Server_new(config);

    addVariable(server);
    addPubSubConnection(server);
    addPublishedDataSet(server);
    addDataSetField(server);
    addWriterGroup(server);
    addDataSetWriter(server);


    /* Receive */
     UA_PubSubConnection *connection =
        UA_PubSubConnection_findConnectionbyId(server, connectionIdent);
    if(connection != NULL) {
        
        
        //Register Transport settings
        UA_BrokerWriterGroupTransportDataType brokerTransportSettings; //UA_BrokerConnectionTransportDataType_new();
        memset(&brokerTransportSettings, 0, sizeof(UA_BrokerWriterGroupTransportDataType));
        brokerTransportSettings.queueName = UA_STRING("customTopic");
        brokerTransportSettings.resourceUri = UA_STRING_NULL;
        brokerTransportSettings.authenticationProfileUri = UA_STRING_NULL;
        brokerTransportSettings.requestedDeliveryGuarantee = UA_BROKERTRANSPORTQUALITYOFSERVICE_BESTEFFORT;

        UA_ExtensionObject transportSettings;
        memset(&transportSettings, 0, sizeof(UA_ExtensionObject));
        transportSettings.encoding = UA_EXTENSIONOBJECT_DECODED;
        transportSettings.content.decoded.type = &UA_TYPES[UA_TYPES_BROKERWRITERGROUPTRANSPORTDATATYPE];
        transportSettings.content.decoded.data = &brokerTransportSettings;
        
        
        UA_StatusCode rv = connection->channel->regist(connection->channel, &transportSettings);
        if (rv == UA_STATUSCODE_GOOD) {
            UA_UInt64 subscriptionCallbackId;
            UA_Server_addRepeatedCallback(server, (UA_ServerCallback)mqttPollingCallback,
                                          connection, 200, &subscriptionCallbackId);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "register channel failed: %s!",
                           UA_StatusCode_name(rv));
        }
        
        rv = connection->channel->setCallback(connection->channel, &callback);
    }
    
    retval |= UA_Server_run(server, &running);
    UA_Server_delete(server);
    UA_ServerConfig_delete(config);
    return (int)retval;
}

