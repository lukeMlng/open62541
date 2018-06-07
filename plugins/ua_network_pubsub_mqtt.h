/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information.
 * 
 *    Copyright (c) 2017-2018 Fraunhofer IOSB (Author: Andreas Ebner)
 *    Copyright 2018 (c) Fraunhofer IOSB (Author: Lukas Meling)
 */

#ifndef UA_NETWORK_UDPMC_H_
#define UA_NETWORK_UDPMC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_plugin_pubsub.h"
 
//mqtt network layer specific internal data
typedef struct {
    UA_UInt32 recvBufferSize;
    UA_UInt32 sendBufferSize;
    UA_UInt32 keepAliveTime;
    UA_String *mqttClientId;
} UA_PubSubChannelDataMQTT;
    
/*typedef struct {
    UA_StatusCode (*connectMqtt)(UA_String *host, int port, UA_PubSubChannelDataMQTT* channelDataMQTT);
    UA_StatusCode (*disconnectMqtt)(void);
    UA_StatusCode (*publishMqtt)(UA_String topic, const UA_ByteString *buf);
    UA_StatusCode (*subscribeMqtt)(UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf));
    UA_StatusCode (*unSubscribeMqtt)(UA_String topic);
    UA_StatusCode (*yieldMqtt)(void);
    UA_StatusCode (*recvMqtt)(UA_ByteString *buf);
} MQTT_Funcs;*/
    
UA_PubSubTransportLayer
UA_PubSubTransportLayerMQTT(void);//MQTT_Funcs);


#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_NETWORK_UDPMC_H_ */
