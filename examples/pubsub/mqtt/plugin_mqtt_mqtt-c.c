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

    
/* setup a client */
struct mqtt_client client;
uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
int sockfd;

UA_StatusCode disconnectMqtt(){
    if (sockfd != -1) close(sockfd);
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

UA_StatusCode connectMqtt(UA_String *host, int port, UA_PubSubChannelDataMQTT* options){
    /* open the non-blocking TCP socket (connecting to the broker) */
    
    UA_STACKARRAY(char, hostChar, sizeof(char) * host->length +1);
    memcpy(hostChar, host->data, host->length);
    hostChar[host->length] = 0;
    
    char portString[6];
    snprintf(portString, 6, "%d", port);

    sockfd = mqtt_pal_sockopen(hostChar, portString, AF_INET);

    if (sockfd == -1) {
        perror("Failed to open socket: ");
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }

   
    mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    mqtt_connect(&client, "publishing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

    /* check that we don't have any errors */
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }
    
    
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode subscribeMqtt(UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf)){
    UA_STACKARRAY(char, topicStr, sizeof(char) * topic.length +1);
    memcpy(topicStr, topic.data, topic.length);
    topicStr[topic.length] = 0;
    mqtt_subscribe(&client, topicStr, 0);
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode unSubscribeMqtt(UA_String topic){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}


UA_StatusCode yieldMqtt(){
    //mqtt_sync((struct mqtt_client*) &client);
    return UA_STATUSCODE_GOOD;
}



UA_StatusCode publishMqtt(UA_String topic, const UA_ByteString *buf){
    UA_STACKARRAY(char, topicChar, sizeof(char) * topic.length +1);
    memcpy(topicChar, topic.data, topic.length);
    topicChar[topic.length] = 0;
    
    /* publish the time */
    mqtt_publish(&client, topicChar, buf->data, buf->length, MQTT_PUBLISH_QOS_0);
    mqtt_sync((struct mqtt_client*) &client);
    
    /* check for errors */
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        return UA_STATUSCODE_BADCONNECTIONREJECTED;
    }
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode recvMqtt(UA_ByteString *buf){
     

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
