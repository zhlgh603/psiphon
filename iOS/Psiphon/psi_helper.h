//
//  ssh_helper.c
//  Psiphon
//
//  Created by eugene on 2013-08-30.
//  Copyright (c) 2012 Psiphon. All rights reserved.
//

#include "libssh2/libssh2.h"

#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdbool.h>


#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#define DEBUG 0

#define debug_print(fmt, ...) \
do { if (DEBUG) fprintf(stderr, "DEBUG: %s(): " fmt "\n",  \
__func__, ##__VA_ARGS__); } while (0)

#define error_print(fmt, ...) \
fprintf(stderr, "ERROR: %s:%d:%s(): " fmt "\n", \
__FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__); 



#define INVALID_SOCKET -1
#define BUF_SIZE 32768

#define SOCKS4_VERSION 0x04
#define SOCKS5_CONNECT 0x01
#define SOCKS5_VERSION 0x05
#define SOCKS5_SUCCESS 0x00
#define SOCKS5_RESERVED 0x00
#define SOCKS5_NOAUTH 0x00
#define SOCKS5_IPV4 0x01
#define SOCKS5_IPV6 0x04
#define SOCKS5_DOMAIN 0x03

struct psi_connection
{
    int sock;
    LIBSSH2_CHANNEL* channel;
    char* data;
    int data_size;
    enum{IN, OUT} direction;
};

int psi_connect_server(const char* server_hostname, unsigned int server_port, int timeout); //timeout in seconds
int psi_start_local_listen(unsigned int local_port, int max_conn);
LIBSSH2_SESSION* psi_start_ssh_session(int sock, const char* username,
                                       const char* password,
                                       const char* obfuscate_keyword,
                                       int use_http_prefix);
bool psi_poll_connections(int listensock, struct psi_connection** connections,
                     int num_conn, LIBSSH2_SESSION* session);
void psi_shutdown(unsigned int listensock, LIBSSH2_SESSION* session);

