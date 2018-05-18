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

#include <signal.h>
#include "open62541.h"

//#include "MQTTLinux.h"
#include "MQTTClient.h"
#include "src_generated/ua_statuscodes.h"


UA_StatusCode connectMqtt(UA_String, int);
UA_StatusCode disconnectMqtt(void);
UA_StatusCode unSubscribeMqtt(UA_String topic);
UA_StatusCode publishMqtt(UA_String topic, const UA_ByteString *buf);
UA_StatusCode subscribeMqtt(UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf));
UA_StatusCode yieldMqtt(void);

unsigned char bufmqtt[2000];
unsigned char readbufmqtt[2000];
Network network;
MQTTClient client;
   

UA_NodeId connectionIdent, publishedDataSetIdent, writerGroupIdent;


static void
addPubSubConnection(UA_Server *server){
    /* Details about the connection configuration and handling are located
     * in the pubsub connection tutorial */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("MQTT Connection 1");
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt");
    connectionConfig.enabled = UA_TRUE;
    //UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL , UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL , UA_STRING("opc.udp://192.168.178.40:1883/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl, &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.numeric = UA_UInt32_random();
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
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("Server localtime");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
            UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_LOCALTIME);
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dataSetFieldConfig, &dataSetFieldIdent);
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
    writerGroupConfig.publishingInterval = 1000;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_JSON;
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



UA_StatusCode disconnectMqtt(){
    int rc = 0;
    rc = MQTTIsConnected(&client);
    if(rc == 0){
        return UA_STATUSCODE_GOOD;
    }
    
    int ret = MQTTDisconnect(&client);
    NetworkDisconnect(&network);
    
    if(ret != 0){
        return UA_STATUSCODE_BADDISCONNECT;  
    }
    
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode connectMqtt(UA_String host, int port){
     
    int rc = 0;

    UA_STACKARRAY(char, hostChar, sizeof(char) * host.length +1);
    memcpy(hostChar, host.data, host.length);
    hostChar[host.length] = 0;
    
    NetworkInit(&network);
    rc = NetworkConnect(&network, hostChar, port);
    
    if(rc != 0){
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }
    
    MQTTClientInit(&client, &network, 1000, bufmqtt, 2000, readbufmqtt, 2000);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.willFlag = 0;
    data.MQTTVersion = 3;
    data.clientID.cstring = "23af54t";
    //data.username.cstring = opts.username;
    //data.password.cstring = opts.password;

    data.keepAliveInterval = 10;
    data.cleansession = 1;
    printf("Connecting\n");

    rc = MQTTConnect(&client, &data);
    
    if(rc != 0){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode subscribeMqtt(UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf)){
    int rc = 0;
    rc = MQTTIsConnected(&client);
    if(rc == 0){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    return 0;
}


UA_StatusCode unSubscribeMqtt(UA_String topic){
    int rc = 0;
    rc = MQTTIsConnected(&client);
    if(rc == 0){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    
    return 0;
}


UA_StatusCode yieldMqtt(){
    int rc = 0;
    rc = MQTTIsConnected(&client);
    if(rc == 0){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    
    MQTTYield(&client, 1000);
    return 0;
}



UA_StatusCode publishMqtt(UA_String topic, const UA_ByteString *buf){
    int rc = 0;
    rc = MQTTIsConnected(&client);
    if(rc == 0){
        printf("Connecting\n");
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    
    UA_STACKARRAY(char, topicChar, sizeof(char) * topic.length +1);
    memcpy(topicChar, topic.data, topic.length);
    topicChar[topic.length] = 0;
    
    MQTTMessage m;
    m.qos = QOS0;
    m.payload = buf->data;
    m.payloadlen = buf->length;
    rc = MQTTPublish(&client, topicChar, &m);
    
    if(rc != 0){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    return UA_STATUSCODE_GOOD;
}

int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    UA_ServerConfig *config = UA_ServerConfig_new_default();
    /* Details about the connection configuration and handling are located in the pubsub connection tutorial */
    config->pubsubTransportLayers = (UA_PubSubTransportLayer *) UA_malloc(sizeof(UA_PubSubTransportLayer));
    if(!config->pubsubTransportLayers) {
        UA_ServerConfig_delete(config);
        return -1;
    }
    
    MQTT_Funcs funcs;
    funcs.connectMqtt = &connectMqtt;
    funcs.publishMqtt = &publishMqtt;
    funcs.yieldMqtt = &yieldMqtt;
    funcs.disconnectMqtt = &disconnectMqtt;
    funcs.unSubscribeMqtt = &unSubscribeMqtt;
    funcs.subscribeMqtt = &subscribeMqtt;
    
    //config->pubsubTransportLayers[0] = UA_PubSubTransportLayerUDPMP();
    config->pubsubTransportLayers[0] = UA_PubSubTransportLayerMQTT(funcs);
    config->pubsubTransportLayersSize++;
    UA_Server *server = UA_Server_new(config);

    addPubSubConnection(server);
    addPublishedDataSet(server);
    addDataSetField(server);
    addWriterGroup(server);
    addDataSetWriter(server);

    retval |= UA_Server_run(server, &running);
    UA_Server_delete(server);
    UA_ServerConfig_delete(config);
    return (int)retval;
}

