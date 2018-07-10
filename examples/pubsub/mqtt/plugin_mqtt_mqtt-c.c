/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2018 Fraunhofer IOSB (Author: Lukas Meling)
 */

#ifndef PLUGIN_MQTT_mqttc_H_
#define PLUGIN_MQTT_mqttc_H_

#ifdef __cplusplus
extern "C" {
#endif
    
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../include/ua_plugin_mqtt.h"
#include "mqtt.h"
#include <ua_network_tcp.h>
#include "ua_log_stdout.h"
#include <fcntl.h>
    
/* setup a client */
uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
//int sockfd;

UA_StatusCode disconnectMqtt(UA_PubSubChannelDataMQTT* channelData){
    struct mqtt_client* client = (struct mqtt_client*)channelData->mqttClient;
    mqtt_disconnect(client);
    mqtt_sync(client);
    
    channelData->connection->close(channelData->connection);

    return UA_STATUSCODE_GOOD;
}

struct messageData{
    char* msg;
    char* topic;
    size_t size;
};
struct messageData *lastMessage;

void publish_callback(void** channelDataPtr, struct mqtt_response_publish *published);

void publish_callback(void** channelDataPtr, struct mqtt_response_publish *published) 
{
    printf("Received publish('%s'): %s\n", "", (const char*) published->application_message);
    if(channelDataPtr != NULL){
        UA_PubSubChannelDataMQTT *channelData = (UA_PubSubChannelDataMQTT*)*channelDataPtr;
        if(channelData != NULL){
            if(channelData->callback != NULL){
                
                //Setup topic
                UA_ByteString *topic = UA_ByteString_new();
                if(!topic) return;
                UA_ByteString *msg = UA_ByteString_new();  
                if(!msg) return;
                
                UA_StatusCode ret = UA_ByteString_allocBuffer(topic, published->topic_name_size);
                if(ret){
                    free(topic); free(msg); return;
                }
                
                ret = UA_ByteString_allocBuffer(msg, published->application_message_size);
                if(ret){
                    UA_ByteString_delete(topic); free(msg); return;
                }
                    
                memcpy(topic->data, published->topic_name, published->topic_name_size);
                memcpy(msg->data, published->application_message, published->application_message_size);
                
                //callback with message and topic as bytestring.
                channelData->callback(msg, topic);
            }
        }
    }  
}

UA_StatusCode connectMqtt(UA_PubSubChannelDataMQTT* channelData){
    
    /* open the non-blocking TCP socket (connecting to the broker) */
    UA_ConnectionConfig conf;
    conf.protocolVersion = 0;
    conf.sendBufferSize = 1000;
    conf.recvBufferSize = 2000;
    conf.maxMessageSize = 1000;
    conf.maxChunkCount = 1;
    UA_Connection connection = UA_ClientConnectionTCP( conf,"opc.tcp://127.0.0.1:1883", 1000,NULL);
    channelData->connection = (UA_Connection*)UA_calloc(1, sizeof(UA_Connection));
    memcpy(channelData->connection, &connection, sizeof(UA_Connection));
    
    
    //alloc mqtt_client
    struct mqtt_client* client = (struct mqtt_client*)UA_calloc(1, sizeof(struct mqtt_client));
    
    //save reference
    channelData->mqttClient = client;
    
    //Copy the socketfd to the mqtt client!
    int sockfd = channelData->connection->sockfd;    
    mqtt_init(client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    
    
    //Init custom data for subscribe callback function: 
    //A reference to the channeldata will be available in the callback.
    //This is used to call the user callback channelData.callback
    client->publish_response_callback_state = channelData;
    
    //SET socket to nonblocking! TODO: code duplication: use UA_network tcp
    if (sockfd != -1){
        #ifdef _WIN32
            u_long iMode = 1;
            if(ioctlsocket(sockfd, FIONBIO, &iMode) != NO_ERROR)
                return UA_STATUSCODE_BADINTERNALERROR;
        #elif defined(_WRS_KERNEL) || defined(UA_FREERTOS)
            int on = TRUE;
            if(ioctl(sockfd, FIONBIO, &on) < 0)
              return UA_STATUSCODE_BADINTERNALERROR;
        #else
            int opts = fcntl(sockfd, F_GETFL);
            if(opts < 0 || fcntl(sockfd, F_SETFL, opts|O_NONBLOCK) < 0)
                return UA_STATUSCODE_BADINTERNALERROR;
        #endif
    };
    
    if (sockfd == -1) {
        perror("Failed to open socket: ");
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }
    
    
    //Connect mqtt with socket fd of networktcp 
    mqtt_connect(client, "publishing_client", NULL, NULL, 0, NULL, NULL, 0, 400);
    mqtt_sync(client);
    //mqtt_sync(client);
    
    /* check that we don't have any errors */
    if (client->error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client->error));
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }
    
    
    return UA_STATUSCODE_GOOD;
}



UA_StatusCode subscribeMqtt(UA_PubSubChannelDataMQTT* chanData, UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf)){
    struct mqtt_client* client = (struct mqtt_client*)chanData->mqttClient;
    
    UA_STACKARRAY(char, topicStr, sizeof(char) * topic.length +1);
    memcpy(topicStr, topic.data, topic.length);
    topicStr[topic.length] = 0;
    mqtt_subscribe(client, topicStr, 0);
    //mqtt_sync(client); //Send
    //mqtt_sync(client); //Recv SubAck
    
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode unSubscribeMqtt(UA_PubSubChannelDataMQTT* chanData, UA_String topic){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

UA_StatusCode yieldMqtt(UA_PubSubChannelDataMQTT* chanData){
    struct mqtt_client* client = (struct mqtt_client*)chanData->mqttClient;
    enum MQTTErrors error = mqtt_sync(client);
    if(error == MQTT_OK){
        return UA_STATUSCODE_GOOD;
    }
    
    const char* errorStr = mqtt_error_str(error);
    UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "%s", errorStr);
    
    switch(error){
        case MQTT_ERROR_CLIENT_NOT_CONNECTED:
            return UA_STATUSCODE_BADNOTCONNECTED;
        case MQTT_ERROR_SOCKET_ERROR:
            return UA_STATUSCODE_BADCOMMUNICATIONERROR;
        case MQTT_ERROR_CONNECTION_REFUSED:
            return UA_STATUSCODE_BADCONNECTIONREJECTED;
            
        default:
            return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
        /*MQTT_ERROR(MQTT_ERROR_NULLPTR)                 \
    MQTT_ERROR(MQTT_ERROR_CONTROL_FORBIDDEN_TYPE)        \
    MQTT_ERROR(MQTT_ERROR_CONTROL_INVALID_FLAGS)         \
    MQTT_ERROR(MQTT_ERROR_CONTROL_WRONG_TYPE)            \
    MQTT_ERROR(MQTT_ERROR_CONNECT_NULL_CLIENT_ID)        \
    MQTT_ERROR(MQTT_ERROR_CONNECT_NULL_WILL_MESSAGE)     \
    MQTT_ERROR(MQTT_ERROR_CONNECT_FORBIDDEN_WILL_QOS)    \
    MQTT_ERROR(MQTT_ERROR_CONNACK_FORBIDDEN_FLAGS)       \
    MQTT_ERROR(MQTT_ERROR_CONNACK_FORBIDDEN_CODE)        \
    MQTT_ERROR(MQTT_ERROR_PUBLISH_FORBIDDEN_QOS)         \
    MQTT_ERROR(MQTT_ERROR_SUBSCRIBE_TOO_MANY_TOPICS)     \
    MQTT_ERROR(MQTT_ERROR_MALFORMED_RESPONSE)            \
    MQTT_ERROR(MQTT_ERROR_UNSUBSCRIBE_TOO_MANY_TOPICS)   \
    MQTT_ERROR(MQTT_ERROR_RESPONSE_INVALID_CONTROL_TYPE) \
    MQTT_ERROR(MQTT_ERROR_CLIENT_NOT_CONNECTED)          \
    MQTT_ERROR(MQTT_ERROR_SEND_BUFFER_IS_FULL)           \
    MQTT_ERROR(MQTT_ERROR_SOCKET_ERROR)                  \
    MQTT_ERROR(MQTT_ERROR_MALFORMED_REQUEST)             \
    MQTT_ERROR(MQTT_ERROR_RECV_BUFFER_TOO_SMALL)         \
    MQTT_ERROR(MQTT_ERROR_ACK_OF_UNKNOWN)                \
    MQTT_ERROR(MQTT_ERROR_NOT_IMPLEMENTED)               \
    MQTT_ERROR(MQTT_ERROR_CONNECTION_REFUSED)            \
    MQTT_ERROR(MQTT_ERROR_SUBSCRIBE_FAILED)              \
    MQTT_ERROR(MQTT_ERROR_CONNECTION_CLOSED) */
    
    return UA_STATUSCODE_BADCOMMUNICATIONERROR;
}



UA_StatusCode publishMqtt(UA_PubSubChannelDataMQTT* chanData, UA_String topic, const UA_ByteString *buf){
    UA_STACKARRAY(char, topicChar, sizeof(char) * topic.length +1);
    memcpy(topicChar, topic.data, topic.length);
    topicChar[topic.length] = 0;
    
    struct mqtt_client* client = (struct mqtt_client*)chanData->mqttClient;
    if(client == NULL)
        return UA_STATUSCODE_BADNOTCONNECTED;
    
    /* publish the time */
    mqtt_publish(client, topicChar, buf->data, buf->length, MQTT_PUBLISH_QOS_0);
    mqtt_sync(client);
    
    /* check for errors */
    if (client->error != MQTT_OK) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "%s", mqtt_error_str(client->error));
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode recvMqtt(UA_PubSubChannelDataMQTT* chanData, UA_ByteString *buf){
     
    return yieldMqtt(chanData);
    //mqtt_sync((struct mqtt_client*) &client);

    //if(lastMessage){
    //    buf->data = (UA_Byte*)malloc(lastMessage->application_message_size);
    //    memcpy(buf->data, lastMessage->application_message, lastMessage->application_message_size);
    //    buf->length = lastMessage->application_message_size;
    //}
}
    
#ifdef __cplusplus
} // extern "C"
#endif

#endif /* PLUGIN_MQTT_mqttc_H_ */
