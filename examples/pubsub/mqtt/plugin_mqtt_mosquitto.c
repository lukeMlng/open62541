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
    int ret = mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    if(ret != MOSQ_ERR_SUCCESS){
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode connectMqtt(UA_String host, int port){
   
    UA_STACKARRAY(char, hostChar, sizeof(char) * host.length +1);
    memcpy(hostChar, host.data, host.length);
    hostChar[host.length] = 0;
    
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, NULL);
    if(!mosq){
        fprintf(stderr, "Error: Out of memory.\n");
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }

    if(mosquitto_connect(mosq, hostChar, port, 1000)){
        fprintf(stderr, "Unable to connect.\n");
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    
    int loop = mosquitto_loop_start(mosq);
    if(loop != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Unable to start loop: %i\n", loop);
        return UA_STATUSCODE_BADCOMMUNICATIONERROR;
    }
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode subscribeMqtt(UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf)){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}


UA_StatusCode unSubscribeMqtt(UA_String topic){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}


UA_StatusCode yieldMqtt(){

    return UA_STATUSCODE_BADNOTIMPLEMENTED;
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
