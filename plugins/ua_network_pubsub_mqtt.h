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
#include "ua_network_tcp.h"
    
//mqtt network layer specific internal data
typedef struct {
    UA_NetworkAddressUrlDataType address;
    
    UA_UInt32 mqttRecvBufferSize;
    UA_UInt32 mqttSendBufferSize;
    
    uint8_t *mqttSendBuffer; /* sendbuf should be large enough to hold multiple whole mqtt messages */
    uint8_t *mqttRecvBuffer; /* recvbuf should be large enough any whole mqtt message expected to be received */
    
    UA_UInt32 keepAliveTime;
    UA_String *mqttClientId;
    
    UA_Connection *connection; //Holds the connection with the socket fd.
    void * mqttClient; //Holds the mqtt client
    
    void (*callback)(UA_ByteString *encodedBuffer, UA_ByteString *topic);
} UA_PubSubChannelDataMQTT;
    
UA_PubSubTransportLayer
UA_PubSubTransportLayerMQTT(void);


#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_NETWORK_UDPMC_H_ */
