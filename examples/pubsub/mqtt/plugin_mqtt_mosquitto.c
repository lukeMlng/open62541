/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2018 Fraunhofer IOSB (Author: Lukas Meling)
 */

#ifndef PLUGIN_MQTT_MOSQUITTO_H_
#define PLUGIN_MQTT_MOSQUITTO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "open62541.h"
#include "../../include/ua_plugin_mqtt.h"
#include <stdio.h>
#include <mosquitto.h>
#include <stdlib.h>
#include <unistd.h>

struct mosquitto *mosq = NULL;
    
UA_StatusCode disconnectMqtt(){
    int ret = mosquitto_disconnect(mosq);
    //ret = mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    if(ret != MOSQ_ERR_SUCCESS){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    return UA_STATUSCODE_GOOD;
}

struct messageData{
    char* msg;
    char* topic;
    size_t size;
};

struct messageData *subscribeMessage;

static void on_message(struct mosquitto *mosqConnection, void *obj, const struct mosquitto_message *message)
{
    puts("-------------------------------CB-----------------------------");
    struct messageData *buf = (struct messageData*)obj;
    buf->size = (size_t)message->payloadlen;
    buf->msg = (char*)message->payload;
    char *msg = (char*)message->payload;
    printf("%s", msg);
    
    buf->msg = (char*)malloc((size_t)message->payloadlen);
    memcpy(buf->msg, msg, (size_t)message->payloadlen);
    printf("%s", buf->msg);
}

UA_StatusCode connectMqtt(UA_String host, int port){
   
    subscribeMessage = (struct messageData*)malloc(sizeof(struct messageData));
    subscribeMessage->msg = NULL;
    subscribeMessage->size = 0;
    
    UA_STACKARRAY(char, hostChar, sizeof(char) * host.length +1);
    memcpy(hostChar, host.data, host.length);
    hostChar[host.length] = 0;
    
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, subscribeMessage);
    if(!mosq){
        fprintf(stderr, "Error: Out of memory.\n");
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }

    if(mosquitto_connect(mosq, hostChar, port, 1000)){
        fprintf(stderr, "Unable to connect.\n");
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    
    //mosquitto_loop(mosq);
    //int loop = mosquitto_loop_start(mosq);
    //if(loop != MOSQ_ERR_SUCCESS){
    //    fprintf(stderr, "Unable to start loop: %i\n", loop);
    //    return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    //}
    
    mosquitto_message_callback_set(mosq, &on_message);
    
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode subscribeMqtt(UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf)){
    int mid;
    if(mosquitto_subscribe(mosq, &mid, "test", 0)){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode unSubscribeMqtt(UA_String topic){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}


UA_StatusCode yieldMqtt(){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

UA_StatusCode recvMqtt(UA_ByteString *buf){
     

    //size_t i;
    //for (i = 0; i < 10; i++) {
        printf("%s", "Check: ");
        mosquitto_loop(mosq, 1, 1);
        if(subscribeMessage->size != 0){
            printf("%s","OK");
            printf("%s", subscribeMessage->msg);
            
            buf->data = (UA_Byte*)subscribeMessage->msg;
            buf->length = subscribeMessage->size;
            
            subscribeMessage->size = 0;
            //break;
        }
        printf("%s","Nada\n");
    //}
    
    return 0;
}


UA_StatusCode publishMqtt(UA_String topic, const UA_ByteString *buf){
    UA_STACKARRAY(char, topicChar, sizeof(char) * topic.length +1);
    memcpy(topicChar, topic.data, topic.length);
    topicChar[topic.length] = 0;
    
    mosquitto_publish(mosq, NULL, topicChar, (int)(buf->length), buf->data, 0, 0);
    return UA_STATUSCODE_GOOD;
}

    
#ifdef __cplusplus
} // extern "C"
#endif

#endif /* PLUGIN_MQTT_MOSQUITTO_H_ */
