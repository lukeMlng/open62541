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

#include <fcntl.h>
    
/* setup a client */
uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
//int sockfd;

UA_Connection connection;

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

void publish_callback(void** unused, struct mqtt_response_publish *published);

void publish_callback(void** unused, struct mqtt_response_publish *published) 
{
    //lastMessage->msg = published->application_message
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    printf("Received publish('%s'): %s\n", "", (const char*) published->application_message);
}

UA_StatusCode connectMqtt(UA_PubSubChannelDataMQTT* channelData){
    /* open the non-blocking TCP socket (connecting to the broker) */
    
    //UA_STACKARRAY(char, hostChar, sizeof(char) * host->length +1);
    //memcpy(hostChar, host->data, host->length);
    //hostChar[host->length] = 0;
    
    //char portString[6];
    //snprintf(portString, 6, "%d", port);

    //UA_ConnectionConfig conf;
    //conf.protocolVersion = 0;
    //conf.sendBufferSize = 1000;
    //conf.recvBufferSize = 2000;
    //conf.maxMessageSize = 1000;
    //conf.maxChunkCount = 1;
    //connection =  UA_ClientConnectionTCP( conf,"opc.tcp://127.0.0.1:1883", 10000,NULL);
    
    //sockfd = mqtt_pal_sockopen(hostChar, portString, AF_INET);
    //sockfd = connection.sockfd;

 

    
    //Setup mqtt_client
    struct mqtt_client* client = (struct mqtt_client*)UA_calloc(1, sizeof(struct mqtt_client));
    
    //save reference
    channelData->mqttClient = client;
    
    //Copy the socketfd to the mqtt client!
    int sockfd = channelData->connection->sockfd;    
    mqtt_init(client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    
    //SET socket to nonblocking!
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
    
    
    //Connect with socket fd of networktcp
    mqtt_connect(client, "publishing_client", NULL, NULL, 0, NULL, NULL, 0, 400);
    mqtt_sync(client);
    
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
    mqtt_sync(client);
    
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode unSubscribeMqtt(UA_PubSubChannelDataMQTT* chanData, UA_String topic){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

UA_StatusCode yieldMqtt(UA_PubSubChannelDataMQTT* chanData){
    //mqtt_sync((struct mqtt_client*) &client);
    return UA_STATUSCODE_GOOD;
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
        fprintf(stderr, "error: %s\n", mqtt_error_str(client->error));
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode recvMqtt(UA_PubSubChannelDataMQTT* chanData, UA_ByteString *buf){
     

    //mqtt_sync((struct mqtt_client*) &client);

    //if(lastMessage){
    //    buf->data = (UA_Byte*)malloc(lastMessage->application_message_size);
    //    memcpy(buf->data, lastMessage->application_message, lastMessage->application_message_size);
    //    buf->length = lastMessage->application_message_size;
    //}

    return 0;
}
    
#ifdef __cplusplus
} // extern "C"
#endif

#endif /* PLUGIN_MQTT_mqttc_H_ */
