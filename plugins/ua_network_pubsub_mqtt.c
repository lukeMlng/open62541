/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2018 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2018 Fraunhofer IOSB (Author: Lukas Meling)
 */

/* Enable POSIX features */
#if !defined(_XOPEN_SOURCE) && !defined(_WRS_KERNEL)
# define _XOPEN_SOURCE 600
#endif
#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
#endif

/* On older systems we need to define _BSD_SOURCE.
 * _DEFAULT_SOURCE is an alias for that. */
#ifndef _BSD_SOURCE
# define _BSD_SOURCE
#endif

 /* Disable some security warnings on MSVC */
#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

 /* Assume that Windows versions are newer than Windows XP */
#if defined(__MINGW32__) && (!defined(WINVER) || WINVER < 0x501)
# undef WINVER
# undef _WIN32_WINDOWS
# undef _WIN32_WINNT
# define WINVER 0x0501
# define _WIN32_WINDOWS 0x0501
# define _WIN32_WINNT 0x0501
#endif

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# include <Iphlpapi.h>
# define CLOSESOCKET(S) closesocket((SOCKET)S)
# define ssize_t int
# define UA_fd_set(fd, fds) FD_SET((unsigned int)fd, fds)
# define UA_fd_isset(fd, fds) FD_ISSET((unsigned int)fd, fds)
#else /* _WIN32 */
#  define CLOSESOCKET(S) close(S)
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
# define UA_fd_set(fd, fds) FD_SET(fd, fds)
# define UA_fd_isset(fd, fds) FD_ISSET(fd, fds)
# endif /* Not Windows */

#include <stdio.h>
#include "ua_plugin_network.h"
#include "ua_network_pubsub_mqtt.h"
#include "ua_log_stdout.h"

//mqtt network layer specific internal data
typedef struct {
    UA_UInt32 id;
    struct sockaddr_storage *ai_addr;
} UA_PubSubChannelDataMQTT;

/**
 * Open communication socket based on the connectionConfig. Protocol specific parameters are
 * provided within the connectionConfig as KeyValuePair.
 * 
 *
 * @return ref to created channel, NULL on error
 */
static UA_PubSubChannel *
UA_PubSubChannelMQTT_open(const UA_PubSubConnectionConfig *connectionConfig) {
    #ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif /* Not Windows */

    UA_NetworkAddressUrlDataType address;
    if(UA_Variant_hasScalarType(&connectionConfig->address, &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE])){
        address = *(UA_NetworkAddressUrlDataType *)connectionConfig->address.data;
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection creation failed. Invalid Address.");
        return NULL;
    }
    //allocate and init memory for the UDP multicast specific internal data
    UA_PubSubChannelDataMQTT * channelDataMQTT =
            (UA_PubSubChannelDataMQTT *) UA_calloc(1, (sizeof(UA_PubSubChannelDataMQTT)));
    if(!channelDataMQTT){
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection creation failed. Out of memory.");
        return NULL;
    }
    //set default values
    memcpy(channelDataMQTT, &(UA_PubSubChannelDataMQTT){0}, sizeof(UA_PubSubChannelDataMQTT));
    //iterate over the given KeyValuePair paramters
    UA_String idParam = UA_STRING("id");
    for(size_t i = 0; i < connectionConfig->connectionPropertiesSize; i++){
        if(UA_String_equal(&connectionConfig->connectionProperties[i].key.name, &idParam)){
            if(UA_Variant_hasScalarType(&connectionConfig->connectionProperties[i].value, &UA_TYPES[UA_TYPES_BOOLEAN])){
                channelDataMQTT->id = *(UA_UInt32 *) connectionConfig->connectionProperties[i].value.data;
            }
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection creation. Unknown connection parameter.");
        }
    }

    UA_PubSubChannel *newChannel = (UA_PubSubChannel *) UA_calloc(1, sizeof(UA_PubSubChannel));
    if(!newChannel){
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection creation failed. Out of memory.");
        UA_free(channelDataMQTT);
        return NULL;
    }

    UA_String hostname, path;
    UA_UInt16 networkPort;
    //TODO replace fallback to use the existing parseEndpointUrl function. Extend parseEndpointUrl for UDP or create own parseEndpointUrl function for PubSub.
    if(strncmp((char*)&address.url.data, "opc.mqtt://", 10) != 0){
        strncpy((char*)address.url.data, "opc.tcp://", 10);
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Invalid URL.");
        UA_free(channelDataMQTT);
        UA_free(newChannel);
        return NULL;
    }
    if(UA_parseEndpointUrl(&address.url, &hostname, &networkPort, &path) != UA_STATUSCODE_GOOD){
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. Invalid URL.");
        UA_free(channelDataMQTT);
        UA_free(newChannel);
        return NULL;
    }
    if(hostname.length > 512) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                     "PubSub Connection creation failed. URL maximum length is 512.");
        UA_free(channelDataMQTT);
        UA_free(newChannel);
        return NULL;
    }

    UA_STACKARRAY(char, addressAsChar, sizeof(char) * hostname.length +1);
    memcpy(addressAsChar, hostname.data, hostname.length);
    addressAsChar[hostname.length] = 0;
    char port[6];
    sprintf(port, "%u", networkPort);

    
    
    //link channel and internal channel data
    newChannel->handle = channelDataMQTT;

    newChannel->state = UA_PUBSUB_CHANNEL_PUB;
    return newChannel;
}

/**
 * Subscribe to a given address.
 *
 * @return UA_STATUSCODE_GOOD on success
 */
static UA_StatusCode
UA_PubSubChannelMQTT_regist(UA_PubSubChannel *channel, UA_ExtensionObject *transportSettings) {
    if(!(channel->state == UA_PUBSUB_CHANNEL_PUB || channel->state == UA_PUBSUB_CHANNEL_RDY)){
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection regist failed.");
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    UA_PubSubChannelDataMQTT * connectionConfig = (UA_PubSubChannelDataMQTT *) channel->handle;
    printf("%u", connectionConfig->id);
    return UA_STATUSCODE_GOOD;
}

/**
 * Remove current subscription.
 *
 * @return UA_STATUSCODE_GOOD on success
 */
static UA_StatusCode
UA_PubSubChannelMQTT_unregist(UA_PubSubChannel *channel, UA_ExtensionObject *transportSettings) {
    if(!(channel->state == UA_PUBSUB_CHANNEL_PUB_SUB || channel->state == UA_PUBSUB_CHANNEL_SUB)){
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection unregist failed.");
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    UA_PubSubChannelDataMQTT * connectionConfig = (UA_PubSubChannelDataMQTT *) channel->handle;
    printf("%u", connectionConfig->id);
    return UA_STATUSCODE_GOOD;
}

/**
 * Send messages to the connection defined address
 *
 * @return UA_STATUSCODE_GOOD if success
 */
static UA_StatusCode
UA_PubSubChannelMQTT_send(UA_PubSubChannel *channel, UA_ExtensionObject *transportSettigns, const UA_ByteString *buf) {
    UA_PubSubChannelDataMQTT *channelConfigMQTT = (UA_PubSubChannelDataMQTT *) channel->handle;
    if(!(channel->state == UA_PUBSUB_CHANNEL_PUB || channel->state == UA_PUBSUB_CHANNEL_PUB_SUB)){
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection sending failed. Invalid state.");
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    printf("%u", channelConfigMQTT->id);
    return UA_STATUSCODE_GOOD;
}

/**
 * Receive messages. The regist function should be called before.
 *
 * @param timeout in usec | on windows platforms are only multiples of 1000usec possible
 * @return
 */
static UA_StatusCode
UA_PubSubChannelMQTT_receive(UA_PubSubChannel *channel, UA_ByteString *message, UA_ExtensionObject *transportSettigns, UA_UInt32 timeout){
    if(!(channel->state == UA_PUBSUB_CHANNEL_PUB || channel->state == UA_PUBSUB_CHANNEL_PUB_SUB)) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub Connection receive failed. Invalid state.");
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    UA_PubSubChannelDataMQTT *channelConfigMQTT = (UA_PubSubChannelDataMQTT *) channel->handle;
    printf("%u", channelConfigMQTT->id);
    return UA_STATUSCODE_GOOD;
}

/**
 * Close channel and free the channel data.
 *
 * @return UA_STATUSCODE_GOOD if success
 */
static UA_StatusCode
UA_PubSubChannelMQTT_close(UA_PubSubChannel *channel) {
    //cleanup the internal NetworkLayer data
    UA_PubSubChannelDataMQTT *networkLayerData = (UA_PubSubChannelDataMQTT *) channel->handle;
    UA_free(networkLayerData);
    UA_free(channel);
    return UA_STATUSCODE_GOOD;
}

/**
 * Generate a new channel. based on the given configuration.
 *
 * @param connectionConfig connection configuration
 * @return  ref to created channel, NULL on error
 */
static UA_PubSubChannel *
TransportLayerMQTT_addChannel(UA_PubSubConnectionConfig *connectionConfig) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "PubSub channel requested");
    UA_PubSubChannel * pubSubChannel = UA_PubSubChannelMQTT_open(connectionConfig);
    if(pubSubChannel){
        pubSubChannel->regist = UA_PubSubChannelMQTT_regist;
        pubSubChannel->unregist = UA_PubSubChannelMQTT_unregist;
        pubSubChannel->send = UA_PubSubChannelMQTT_send;
        pubSubChannel->receive = UA_PubSubChannelMQTT_receive;
        pubSubChannel->close = UA_PubSubChannelMQTT_close;
        pubSubChannel->connectionConfig = connectionConfig;
    }
    return pubSubChannel;
}

//MQTT channel factory
UA_PubSubTransportLayer
UA_PubSubTransportLayerMQTT() {
    UA_PubSubTransportLayer pubSubTransportLayer;
    pubSubTransportLayer.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-mqtt");
    pubSubTransportLayer.createPubSubChannel = &TransportLayerMQTT_addChannel;
    return pubSubTransportLayer;
}

#undef _POSIX_C_SOURCE
