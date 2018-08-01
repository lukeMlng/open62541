#include <mqtt.h>

/** 
 * @file 
 * @brief Implements @ref mqtt_pal_sendall and @ref mqtt_pal_recvall and 
 *        any platform-specific helpers you'd like.
 * @cond Doxygen_Suppress
 */


#ifdef __unix__

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#include <ua_network_tcp.h>

# define SOCKET int
# define WIN32_INT

#ifdef _WIN32
# define errno__ WSAGetLastError()
# define INTERRUPTED WSAEINTR
# define WOULDBLOCK WSAEWOULDBLOCK
# define AGAIN WSAEWOULDBLOCK
#else
# define errno__ errno
# define INTERRUPTED EINTR
# define WOULDBLOCK EWOULDBLOCK
# define AGAIN EAGAIN
#endif

ssize_t mqtt_pal_sendall(int fd, const void* buf, size_t len, int flags, void* client) {
    /* Prevent OS signals when sending to a closed socket */
    //int flags = 0;
#ifdef MSG_NOSIGNAL
    flags |= MSG_NOSIGNAL;
#endif

    struct mqtt_client* c = (struct mqtt_client*)client;
    UA_Connection *connection = (UA_Connection*) c->custom;
    UA_ByteString sendBuffer;
    sendBuffer.data = (UA_Byte*)UA_malloc(len);
    //TODO: check
    sendBuffer.length = len;
    memcpy(sendBuffer.data, buf, len);

    connection->send(connection, &sendBuffer);
    
    /* Send the full buffer. This may require several calls to send 
    size_t nWritten = 0;
    do {
        ssize_t n = 0;
        do {
            size_t bytes_to_send = len - nWritten;
            n = send((SOCKET)fd,
                     (const char*)buf + nWritten,
                     WIN32_INT bytes_to_send, flags);
            if(n < 0 && errno__ != INTERRUPTED && errno__ != AGAIN) {
                //connection->close(connection);
                return MQTT_ERROR_SOCKET_ERROR;
            }
        } while(n < 0);
        nWritten += (size_t)n;
    } while(nWritten < len);
    
    return (ssize_t)nWritten;*/
    
    return (ssize_t)len;
}

/*
ssize_t mqtt_pal_sendall(int fd, const void* buf, size_t len, int flags) {
    size_t sent = 0;
    while(sent < len) {
        ssize_t tmp = send(fd, (const void*)((const uint8_t*)buf + sent), len - sent, flags);
        if (tmp < 1) {
            return MQTT_ERROR_SOCKET_ERROR;
        }
        sent += (size_t) tmp;
    }
    return (ssize_t)sent;
}
*/
ssize_t mqtt_pal_recvall(int fd, void* buf, size_t bufsz, int flags, void* client) {
    
    struct mqtt_client* c = (struct mqtt_client*)client;
    UA_Connection *connection = (UA_Connection*) c->custom;
    
    connection->localConf.recvBufferSize = (UA_UInt32) bufsz;
    UA_ByteString inBuffer;
    UA_StatusCode ret = connection->recv(connection, &inBuffer, 10);
    if(ret == UA_STATUSCODE_GOOD ){
        /* Buffer received */
        memcpy(buf, inBuffer.data, inBuffer.length);
        
        connection->releaseRecvBuffer(connection, &inBuffer);
        return (ssize_t)inBuffer.length;
    }else if(ret == UA_STATUSCODE_GOODNONCRITICALTIMEOUT){
        /* nothin recv? */
        return 0;
    }else{
        return -1;
    }
    /*const void const *start = buf;
    ssize_t rv;
    do {
        rv = recv(fd, buf, bufsz, flags);
        if (rv > 0) {
            // successfully read bytes from the socket 
            buf = (uint8_t*)buf + rv;
            bufsz -= (size_t)rv;
        } else if (rv < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // an error occurred that wasn't "nothing to read". 
            return MQTT_ERROR_SOCKET_ERROR;
        }
    } while (rv > 0);

    return (ssize_t)((uintptr_t)buf - (uintptr_t)start);*/
}

/*
int mqtt_pal_sockopen(const char* addr, const char* port, int af) {
    struct addrinfo hints = {0};

    hints.ai_family = af; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Must be TCP
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;

    // get address information
    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if(rv != 0) {
        fprintf(stderr, "Failed to open socket (getaddrinfo): %s\n", gai_strerror(rv));
        return -1;
    }

    //open the first possible socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        // connect to server
        rv = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
        if(rv == -1) continue;
        break;
    }  

    //free servinfo
    freeaddrinfo(servinfo);

    // make non-blocking
    if (sockfd != -1) fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    // return the new socket fd
    return sockfd;  
}*/

#endif

/** @endcond */