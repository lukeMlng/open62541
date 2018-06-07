/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2018 Fraunhofer IOSB (Author: Lukas Meling)
 */

#ifndef UA_PLUGIN_MQTT_H_
#define UA_PLUGIN_MQTT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ua_network_pubsub_mqtt.h"

UA_StatusCode connectMqtt(UA_String*, int, UA_PubSubChannelDataMQTT*);
UA_StatusCode disconnectMqtt(void);
UA_StatusCode unSubscribeMqtt(UA_String topic);
UA_StatusCode publishMqtt(UA_String topic, const UA_ByteString *buf);
UA_StatusCode subscribeMqtt(UA_String topic, UA_StatusCode (*cb)(UA_ByteString *buf));
UA_StatusCode yieldMqtt(void);
UA_StatusCode recvMqtt(UA_ByteString *buf);

    
#ifdef __cplusplus
} // extern "C"
#endif

#endif /* UA_PLUGIN_MQTT_H_ */
