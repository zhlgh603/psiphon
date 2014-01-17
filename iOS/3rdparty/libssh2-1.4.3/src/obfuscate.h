#ifndef _OBFUSCATE_H
#define _OBFUSCATE_H

#define u_int unsigned int
#define u_char unsigned char

void _libssh2_obfuscation_free(LIBSSH2_OBFUSCATION *obfuscation);
int _libssh2_obfuscation_send_seed(LIBSSH2_SESSION *session);
int _libssh2_obfuscation_initialize(LIBSSH2_OBFUSCATION *obfuscation);
ssize_t _libssh2_obfuscation_send(int sock, const void *buffer, size_t length,
        int flags, void **abstract);
ssize_t _libssh2_obfuscation_recv(int sock, void *buffer, size_t length, 
        int flags, void **abstract);

int _libssh2_obfuscate_skip_http_prefix(LIBSSH2_SESSION *session);

#endif


