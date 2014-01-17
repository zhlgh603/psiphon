//
//  ssh_helper.c
//  Psiphon
//
//  Created by eugene on 2013-08-30.
//  Copyright (c) 2012 Psiphon. All rights reserved.
//

#include "psi_helper.h"
#include <sys/errno.h>

struct destination
{
    char addr[256];
    int port;
};

int setnonblocking(int sock)
{
    {
        int opts;
        
        opts = fcntl(sock,F_GETFL);
        if (opts < 0) {
            error_print("fcntl(F_GETFL) error");
            exit(EXIT_FAILURE);
        }
        opts = (opts | O_NONBLOCK);
        if (fcntl(sock,F_SETFL,opts) < 0) {
            error_print("fcntl(F_SETFL) error");
            exit(EXIT_FAILURE);
        }
        return 0;
    }
}

bool get_socks4_destination(int sock, char* buf, int buflen, struct destination* dest)
{
    char* host;
    u_int len, have, i, found, need;
    struct {
        u_int8_t version;
        u_int8_t command;
        u_int16_t dest_port;
        struct in_addr dest_addr;
    } req;
    
    memset(dest, 0, sizeof(struct destination));
    
    have = buflen;
    len = sizeof(req);
    if (have < len)
        return false;
    
    need = 1;
    /* SOCKS4A uses an invalid IP address 0.0.0.x */
    if (buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] != 0) {
        /* ... and needs an extra string (the hostname) */
        need = 2;
    }
    /* Check for terminating NULL on the string(s) */
    for (found = 0, i = len; i < have; i++) {
        if (buf[i] == '\0') {
            found++;
            if (found == need)
                break;
        }
        if (i > 1024) {
            /* the peer is probably sending garbage */
            return false;
        }
    }
    
    if (found < need)
    {
        error_print("%s", "socks4 error: buffer is too short");
        return false;
    }
    
    memcpy(&req, buf, sizeof(req));
    
    buf += sizeof(req);
    buflen -= sizeof(req);
    
    len = strlen(buf);
    len++; // for trailing '\0'
    if(len > buflen)
    {
        error_print("%s", "socks4 error: username is too long");
        return false;
    }
    buf += len; //ignore username if any
    buflen -=len;
    
    if(1 == need) //SOCKS4
    {
        host = inet_ntoa(req.dest_addr);
        len = strlen(host);
        
        if(NULL != host && len < 256)
        {
            memcpy(dest->addr, host, len);
            dest->addr[len] = '\0';
        }
        else
        {
            error_print("inet_ntoa() error");
            return false;
        }
    }
    else //SOCKS4a
    {
        len = strlen(buf);
        len++; // for trailing '\0'
        
        if (len > buflen)
        {
            error_print( "socks4 error: socks4a hostname wrong length");
            return false;
        }
        if (len > 256)
        {
            error_print( "socks4a error: hostname \"%.100s\" is too long", buf);
            return false;
        }
        
        memcpy(dest->addr, buf, buflen);
        dest->addr[buflen] = '\0';
    }
    
    dest->port = ntohs(req.dest_port);
    
    if (req.command != 1) {
        error_print( "socks4 error: can't handle 0x%1x command", req.command);
        return false;
    }
    
//Response
    struct iovec rsp[4];
    struct in_addr dest_addr;
    
    dest_addr.s_addr = INADDR_ANY;
    
    u_int16_t dest_port = 0x00;
    u_int8_t first_byte = 0x00;
    u_int8_t status = 0x5a;
    
    
    rsp[0].iov_base = &first_byte;              /* 1. 1 null byte for reply */
    rsp[0].iov_len = sizeof(u_int8_t);
    
    rsp[1].iov_base = &status;                  /* 2. 1 byte 0x5a 'request granted' */
    rsp[1].iov_len = sizeof(u_int8_t);
    
    rsp[2].iov_base = &dest_port;               /* 3. 2 bytes port, ignored */
    rsp[2].iov_len = sizeof(u_int16_t);
    
    rsp[3].iov_base = &dest_addr;               /* 4. 2 bytes IPv4 address, ignored */
    rsp[3].iov_len = sizeof(struct in_addr);
    
    
    len = writev(sock, rsp, 4);
    
    if(len != 6)
    {
        error_print("writev() error");
        return false;
    }
    
    debug_print( "got SOCKS4%s request for %s:%d", 1 == need ? "": "a", dest->addr, dest->port);
    return true;
}

bool get_socks5_destination(int sock, char* buf, int buflen, struct destination* dest)
{
    
    int rc;
    int i, nmethods, found;
    int addrlen, af;
    
    char tmp_dest[255 + 1];
    u_int16_t tmp_dest_port;
    
    char* tmp_str = NULL;
    
    struct {
        u_int8_t version;
        u_int8_t command;
        u_int8_t reserved;
        u_int8_t atyp;
    } req;
    
    
    memset(dest, 0, sizeof(struct destination));
    
    
    nmethods = buf[1];
    
    if(buflen < nmethods + 2)
    {
        error_print( "buffer too short");
        return false;
    }
    
    /* look for method: "NO AUTHENTICATION REQUIRED" == 0x00*/
    for (found = 0, i = 2; i < nmethods + 2; i++) {
        if (buf[i] == SOCKS5_NOAUTH) {
            found = 1;
            break;
        }
    }
    if (!found) {
        error_print( "'NO AUTHENTICATION REQUIRED' not found");
        return false;
    }
    
    //respond to the client with protocol and method
    buf[0] = SOCKS5_VERSION;
    buf[1] = SOCKS5_NOAUTH;
    
    rc = write(sock, buf, 2);
    
    if(rc != 2)
    {
        error_print("write() error");
        return false;
    }
    
    rc  = (int)read(sock, buf, BUF_SIZE);
    
    if (rc < sizeof(req)+1)
    {
        error_print( "read too few bytes negotiating, rc == %d", rc);
        return false;
    }
    
    memcpy(&req, buf, sizeof(req));
    
    if (req.version != SOCKS5_VERSION)
    {
        error_print("unsupported SOCKS version: %d", req.version);
        return false;

    }
    
    if(req.command != SOCKS5_CONNECT)
    {
        error_print("unsupported SOCKS5 command: %d", req.command);
        
    }
    
    if(req.reserved != SOCKS5_RESERVED) {
        error_print("unsupported SOCKS5 reserved field: %d", req.reserved);
        return false;
    }
    
    switch (req.atyp){
        case SOCKS5_IPV4:
            addrlen = 4;
            af = AF_INET;
            break;
        case SOCKS5_DOMAIN:
            addrlen = buf[sizeof(req)];
            af = -1;
            break;
        case SOCKS5_IPV6:
            addrlen = 16;
            af = AF_INET6;
            break;
        default:
            error_print("bad SOCKS5 atyp %d", req.atyp);
            return false;
    }
    
    
    tmp_str = &buf[sizeof(req)];
    if(req.atyp == SOCKS5_DOMAIN)
    {
        tmp_str++; //eat name length
        if(addrlen > 255)
        {
            error_print("SOCKS5 name too long '%.100s'", tmp_str);
            return false;
        }
    }
    
    /*TODO error checking om memcpy*/
    
    memcpy(&tmp_dest, tmp_str, addrlen);
    tmp_dest[addrlen] = '\0';
    
    tmp_str += addrlen;
    
    memcpy(&tmp_dest_port, tmp_str, 2);
    
    if(req.atyp == SOCKS5_DOMAIN)
    {
        memcpy(dest->addr, &tmp_dest, addrlen);
        dest->addr[addrlen] = '\0';
    }
    else
    {
        if (NULL == inet_ntop(af, &tmp_dest, dest->addr, sizeof(dest->addr)))
        {
            error_print("inet_ntop error for destination: %s", tmp_dest);
            return false;
        }
    }
    dest->port = ntohs(tmp_dest_port);
    
    //Response
    int len = 0;
    struct iovec rsp[6];

    struct in_addr dest_addr;
    dest_addr.s_addr = INADDR_ANY;

    u_int16_t dest_port = 0;
    u_int8_t version = SOCKS5_VERSION;
    u_int8_t success = SOCKS5_SUCCESS;
    u_int8_t reserved = 0;
    u_int8_t ipv = SOCKS5_IPV4;
    
    rsp[0].iov_base = &version;                 /* 1. 1 byte SOCKS 5 version */
    rsp[0].iov_len = sizeof(u_int8_t);
    
    rsp[1].iov_base = &success;                 /* 2. 1 byte 0x00 'request granted' */
    rsp[1].iov_len = sizeof(u_int8_t);
    
    rsp[2].iov_base = &reserved;                 /* 3. 1 byte 0x00 reserved */
    rsp[2].iov_len = sizeof(u_int8_t);
    
    rsp[3].iov_base = &ipv;                     /* 4. 1 byte 0x01 for IPv4 address*/
    rsp[3].iov_len = sizeof(u_int8_t);
    
    rsp[4].iov_base = &dest_addr;               /* 5. 4 bytes dest addr for IPV4, ignored*/
    rsp[4].iov_len = sizeof(struct in_addr);
    
    rsp[5].iov_base = &dest_port;               /* 6. 2 bytes dest port, ignored*/
    rsp[5].iov_len = sizeof(u_int16_t);

    
    len = writev(sock, (const struct iovec *)rsp, 6);
    
    if(len != 10)
    {
        error_print("writev()");
        return false;
    }
    
    debug_print("got SOCKS5 request for %s:%d", dest->addr, dest->port);
    
    return true;
}

void disconnect_callback(LIBSSH2_SESSION *session,
                         int reason,
                         const char *message,
                         int message_len,
                         const char *language,
                         int language_len,
                         void **abstract)
{
    if(session)
    {
        libssh2_session_disconnect(session, "");
        libssh2_session_free(session);
        session = NULL;
    }
}

LIBSSH2_CHANNEL* make_dynamic_ssh_channel(int sock, LIBSSH2_SESSION* session)
{
    int rc;
    char buf[BUF_SIZE];
    
    struct destination dest;
    
    LIBSSH2_CHANNEL* channel = NULL;
    int ssh_last_err;
    
    rc  = (int)read(sock, buf, BUF_SIZE);
    if(rc < 2)
    {
        error_print("socks client read %d,", rc);
        return NULL;
    }
    
    if(SOCKS4_VERSION == buf[0])
    {
        rc = get_socks4_destination(sock, buf, rc, &dest);
    }
    else if (SOCKS5_VERSION == buf[0])
    {
        rc = get_socks5_destination(sock, buf, rc, &dest);
    }
    else
    {
        error_print("socks version 0x%1x not supported", buf[0]);
        return NULL;
    }
    
    if(rc == 0) //Error getting destination addr
    {
        return NULL;
    }
    
    /* Block until we've got a channel */
    ssh_last_err = LIBSSH2_ERROR_EAGAIN;
    while(LIBSSH2_ERROR_EAGAIN == ssh_last_err)
    {
        channel = libssh2_channel_direct_tcpip(session, dest.addr, dest.port);
        if(channel) break;
        ssh_last_err = libssh2_session_last_errno(session);
    }
    
    if (!channel) {
        char *errmsg;
        int errlen;
        int err = libssh2_session_last_error(session, &errmsg, &errlen, 0);
        
        error_print("Could not open the direct-tcpip channel!\n"
                "Last error: %s (%d)", errmsg, err);
        return NULL;
    }
    
    return channel;
}

void kill_connection(struct psi_connection** connection)
{
    debug_print("killing connection: sock %d, channel %p", (*connection)->sock, (*connection)->channel);
    close((*connection)->sock);
    (*connection)->sock = INVALID_SOCKET;
    
    /* Block until we've freed the  channel */
    int ssh_last_err = LIBSSH2_ERROR_EAGAIN;
    while(0 != ssh_last_err)
    {
        ssh_last_err = libssh2_channel_free((*connection)->channel);
    }
    (*connection)->channel = NULL;
}

int forward_conn(struct psi_connection* connection)
{
    char buf[BUF_SIZE];
    int rcvd, sent;
    
    int sock = connection->sock;
    LIBSSH2_CHANNEL* channel = connection->channel;
    int direction = connection->direction;
    /*
     direction OUT: local sock -> ssh channel
     direction IN: ssh channel -> local sock
     */
    switch(direction)
    {
        case IN:
            //find out if there's data avilable on the channel
            rcvd = libssh2_channel_read(channel, buf, BUF_SIZE);
            if(rcvd == LIBSSH2_ERROR_EAGAIN)
            {
                return -1;
            }
            debug_print("%d bytes received from channel %p", rcvd, channel);
            if(rcvd < 1)
            {
                return rcvd;
            }
            sent = write(sock, buf, rcvd);
            debug_print("%d bytes forwarded to browser sock %d", sent, sock);
            return sent;
            
            /*
             TODO: deal with partial sends. ie sent != recvd
             */
            break;
            
            
        case OUT:
            rcvd = (int)read(sock, buf, BUF_SIZE);
            debug_print("%d bytes received from browser sock %d", rcvd, sock);
            if(rcvd < 1)
            {
                return rcvd;
            }
            sent = libssh2_channel_write(channel, buf, rcvd);
            debug_print("%d bytes written to channel %p", sent, channel);
            
            return sent;
            /*
             TODO: deal with partial sends. ie sent != recvd
             */
            break;
        default:
            error_print( "connection direction is not set.\n"
                    "This should not happen!");
            return -1;
    }
}

bool accept_incoming(int sock, struct psi_connection** connections, int num_conn, LIBSSH2_SESSION* session)
{
    int i;
    struct sockaddr_in addrin;
    socklen_t sin_size;
    int incoming;
    int conn_issset = 0;
    LIBSSH2_CHANNEL* channel = NULL;
    
    sin_size = (socklen_t)sizeof(struct sockaddr);
    incoming = accept(sock, (struct sockaddr *)&addrin,
                      &sin_size);
    
    
    if (incoming < 0)
    {
        error_print("accept()");
        return false;
    }
    
    channel = make_dynamic_ssh_channel(incoming, session);
    if(NULL == channel)
    {
        error_print("couldn't make ssh channel");
        close(incoming);
        return true;
    }
    else
    {
        debug_print("created SSH channel %p for incoming sock %d", channel, incoming);
    }
    
    setnonblocking(incoming);

    for (i=0; i < num_conn; i++)
    {
        conn_issset = 0;
        if(INVALID_SOCKET == (*connections+i)->sock)
        {
            (*connections+i)->sock = incoming;
            (*connections+i)->channel = channel;
            conn_issset = 1;
            debug_print("Inserting new connection, sock == %d, channel == %p", incoming, channel);
            break;
        }
    }
    if(0 == conn_issset)
    {
        error_print( "ERROR: Maximum limit of incoming connections reached.\n"
                "This should not happen!");
        close(incoming);
        libssh2_channel_free(channel);
        return false;
    }
    
    return true;
}


LIBSSH2_SESSION* psi_start_ssh_session(int sock,
                                       const char* username,
                                       const char* password,
                                       const char* obfuscate_keyword,
                                       int use_http_prefix)
{
    int rc = libssh2_init(0);
    
    if (rc != 0) {
        error_print("libssh2 initialization failed (%d)\n", rc);
        return NULL;
    }
    
    /*Start ssh session*/

    enum {
        AUTH_NONE = 0,
        AUTH_PASSWORD,
        AUTH_PUBLICKEY
    };
    int auth = AUTH_NONE;
    
    //const char *fingerprint;
    char *userauthlist;
    LIBSSH2_SESSION *session;
    
    
    /* Create a session instance */
    session = libssh2_session_init();
    libssh2_session_obfuscation_init(session, use_http_prefix, obfuscate_keyword);
    
/*
    libssh2_trace(session, LIBSSH2_TRACE_TRANS |
                  LIBSSH2_TRACE_KEX | LIBSSH2_TRACE_SOCKET |
                  LIBSSH2_TRACE_ERROR | LIBSSH2_TRACE_PUBLICKEY |
                  LIBSSH2_TRACE_CONN);
 */
    
    if(!session) {
        error_print( "Could not initialize SSH session!");
        return NULL;
    }
    
    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    rc = libssh2_session_handshake(session, sock);
    
    if(rc) {
        error_print( "Error when starting up SSH session: %d", rc);
        if(session)
        {
            libssh2_session_disconnect(session, "");
            libssh2_session_free(session);
        }

        return NULL;
    }
    
    /* At this point we haven't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    //fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    
    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, username, strlen(username));
    
    debug_print( "Authentication methods: %s", userauthlist);
    if (strstr(userauthlist, "password"))
        auth |= AUTH_PASSWORD;
    if (strstr(userauthlist, "publickey"))
        auth |= AUTH_PUBLICKEY;
    
    if (auth & AUTH_PASSWORD) {
        if (libssh2_userauth_password(session, username, password)) {
            
            error_print( "Authentication by password failed.");
            if(session)
            {
                libssh2_session_disconnect(session, "");
                libssh2_session_free(session);
            }

            return NULL;
        }
    }
    else {
        error_print( "No supported authentication methods found!");
        if(session)
        {
            libssh2_session_disconnect(session, "");
            libssh2_session_free(session);
        }

        return NULL;
    }
    
    //Set session to nonblocking 
    libssh2_session_set_blocking (session, 0);
    
    //Set disconnect callback
    libssh2_session_callback_set(session, LIBSSH2_CALLBACK_DISCONNECT, &disconnect_callback);
    
    return session;
}

bool psi_poll_connections(int listensock,  struct psi_connection** connections,
                     int num_conn, LIBSSH2_SESSION* session)
{
    int rc, i;
    fd_set read_fd;
    struct timeval timeout;
    int max_fd, sock, active_connections;
    struct psi_connection* connection;
    
    LIBSSH2_CHANNEL* channel;
    
    FD_ZERO(&read_fd);
    max_fd = 0;
    active_connections = 0;
    
    if(session == NULL)
        return false;
    
    for (i=0; i < num_conn; i++)
    {
        sock = (*connections+i)->sock;
        channel = (*connections+i)->channel;
        if(INVALID_SOCKET != sock && NULL != channel)
        {
            FD_SET(sock, &read_fd);
            max_fd = sock > max_fd ? sock : max_fd;
            active_connections++;
        }
    }
    
    //Poll for incoming connections
    if(active_connections < num_conn)
    {
        FD_SET(listensock, &read_fd);
        max_fd = listensock > max_fd ? listensock : max_fd;
    }
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 200;
    rc = select(max_fd+1, &read_fd, NULL, NULL, &timeout);
    
    if(rc > 0)
    {
        if(FD_ISSET(listensock, &read_fd))
        {
            debug_print("listensock %d is set in read_fd", listensock);
        }
        for (i=0; i < num_conn; i++)
        {
            connection = (*connections+i);
            sock = connection->sock;
            if(INVALID_SOCKET == sock)
                continue;
            if(FD_ISSET(sock, &read_fd))
            {
                debug_print("connection sock %d is set in read_fd", sock);
            }
        }
    }
    if(rc < 0)
    {
        error_print("select() ERROR!");
        return false;
    }
    
    
    if(FD_ISSET(listensock, &read_fd))
    {
        debug_print("active_connections: %d, accepting incoming", active_connections);
        if(false == accept_incoming(listensock, connections, num_conn, session))
        {
            error_print("accept() ERROR!");
            return false;
        }
    }
    
    for (i=0; i < num_conn; i++)
    {
        rc = -1;
        connection = (*connections+i);
        sock = connection->sock;
        if(INVALID_SOCKET == sock)
        {
            continue;
        }
        if(FD_ISSET(sock, &read_fd))
        {
            connection->direction = OUT;
            rc = forward_conn(connection);
        }
        else if(INVALID_SOCKET != sock)
        {
            connection->direction = IN;
            rc = forward_conn(connection);
        }
        if (rc == 0)
        {
            //either side requests connection close
            kill_connection(&connection);
        }
        /*
         TODO: flush partial data if any
         */
    }
    return true;
}

int psi_connect(int sockfd,  const	 struct sockaddr *serv_addr, socklen_t addrlen, int timeout)
{
    fd_set fdset;
    struct timeval tv;
    
    connect(sockfd, (struct sockaddr *)&serv_addr, addrlen);
    
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    select(sockfd + 1, NULL, &fdset, NULL, &tv);
    if (FD_ISSET(sockfd, &fdset))
    {
        int so_error;
        socklen_t len = sizeof so_error;
        
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        
        if (so_error == 0) {
            return 0;
        }
    }
    return -1;
}

int psi_connect_server(const char* server_hostname, unsigned int server_port, int timeout) //timeout in seconds
{
    int serversock;
    struct sockaddr_in sin;
    
    struct hostent *hp = gethostbyname(server_hostname);
    if(NULL == hp)
    {
        error_print("Can't resolve server hostname %s", server_hostname);
        return INVALID_SOCKET;
    }
    
    const char* server_ip = inet_ntoa( *( struct in_addr*)( hp -> h_addr_list[0]));


    serversock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    if(serversock == -1)
    {
        perror("socket() failed");
        return INVALID_SOCKET;

    }
    sin.sin_family = AF_INET;
    if (INADDR_NONE == (sin.sin_addr.s_addr = inet_addr(server_ip))) {
        close(serversock);
        perror("inet_addr");
        return INVALID_SOCKET;
    }
    sin.sin_port = htons(server_port);
    
    setnonblocking(serversock);
    
    
    fd_set fdset;
    struct timeval tv;
    int so_error;
    socklen_t len = sizeof so_error;
    
    int rc = connect(serversock, (struct sockaddr*)(&sin),
            sizeof(struct sockaddr_in));
    
    if(rc != 0)
    {

        if(errno != EINPROGRESS)
        {
            close(serversock);
            return INVALID_SOCKET;
        }
    }
    
    FD_ZERO(&fdset);
    FD_SET(serversock, &fdset);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    
    select(serversock + 1, NULL, &fdset, NULL, &tv);
    
    if (FD_ISSET(serversock, &fdset))
    {
        getsockopt(serversock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        
        if (so_error == 0) {
            return serversock;
        }
    }
    close(serversock);
    return INVALID_SOCKET;
}

int psi_start_local_listen(unsigned int listen_port, int max_conn)
{
    int listensock;
    struct sockaddr_in sin;

    listensock = socket(AF_INET, SOCK_STREAM, 0);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK); //i.e. localhost
    sin.sin_port = htons(listen_port);
    
    int sockopt = 1;
    if(-1 == setsockopt(listensock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)))
    {
        error_print("setsockopt() error");
        return INVALID_SOCKET;
    }
    
    if (bind(listensock, (struct sockaddr *) &sin, sizeof (sin)) == -1)
    {
        error_print("bind() error");
    }
    
    if (-1 == listen(listensock, max_conn)) {
        error_print("listen() error");
        return INVALID_SOCKET;
    }

    return listensock;
}

void psi_shutdown(unsigned int listensock, LIBSSH2_SESSION* session)
{
    debug_print("SSH shutdown...");
    if(INVALID_SOCKET != listensock)
    {
        close(listensock);
    }
    
    if(session)
    {
        libssh2_session_disconnect(session, "");
        libssh2_session_free(session);
        session = NULL;
    }
    libssh2_exit();
}

