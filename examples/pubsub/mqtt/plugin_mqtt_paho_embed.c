/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2018 Fraunhofer IOSB (Author: Lukas Meling)
 */

#ifndef PLUGIN_MQTT_PAHO_EMBED_H_
#define PLUGIN_MQTT_PAHO_EMBED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "open62541.h"
#include "../../include/ua_plugin_mqtt.h"
//#include "MQTTLinux.h"
#include "MQTTClient.h"

unsigned char bufmqtt[2000];
unsigned char readbufmqtt[2000];
Network network;
MQTTClient client;
   

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
    data.cleansession = 0;
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

    
#ifdef __cplusplus
} // extern "C"
#endif

#endif /* PLUGIN_MQTT_PAHO_EMBED_H_ */
