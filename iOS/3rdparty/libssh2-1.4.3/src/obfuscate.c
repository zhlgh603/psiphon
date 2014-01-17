#include "libssh2_priv.h"
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rc4.h>
#include <sys/poll.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "obfuscate.h"
#include "atomicio.h"

#define crBegin(v)  { int *crLine = &v; switch(v) { case 0:;
#define crFinish(z) } *crLine = 0; return (z); }
#define crReturn(z) \
    do {\
        *crLine =__LINE__; return (z); case __LINE__:;\
    } while (0)

/*
 * these are defined in libssh2_priv.h now
#define OBFUSCATE_KEY_LENGTH 	16
#define OBFUSCATE_SEED_LENGTH	16
#define OBFUSCATE_HASH_ITERATIONS 6000
#define OBFUSCATE_MAX_PADDING	8192
#define OBFUSCATE_MAGIC_VALUE	0x0BF5CA7E
*/

struct seed_msg {
    u_char seed_buffer[OBFUSCATE_SEED_LENGTH];
    u_int32_t magic;
    u_int32_t padding_length;
    u_char padding[];
};

static int generate_key(LIBSSH2_OBFUSCATION * , const u_char *, u_int , u_char *);
static void obfuscate_input(u_char *, u_int, LIBSSH2_OBFUSCATION *);
static void obfuscate_output(u_char *, u_int, LIBSSH2_OBFUSCATION *);
static int do_ssh_obfuscation_prefix(LIBSSH2_OBFUSCATION *, unsigned char c);



void 
_libssh2_obfuscation_free(LIBSSH2_OBFUSCATION *obfuscation)
{
    LIBSSH2_SESSION *session = obfuscation->session;
    assert(session);
    if(obfuscation->keyword)
    {
        LIBSSH2_FREE(session, obfuscation->keyword);
    }
    LIBSSH2_FREE(session, obfuscation);
}

int
_libssh2_obfuscation_send_seed(LIBSSH2_SESSION *session)
{
        char *buffer, *current_ptr;
	int i;
	u_int32_t rnd = 0;
	u_int buffer_length;
	u_int message_length;
	u_int padding_length;

        const char *prefix = "POST / HTTP/1.1\r\n\r\n";
        u_int prefix_length = strlen(prefix);

        LIBSSH2_OBFUSCATION *obfuscation = session->obfuscation;
	
	padding_length = arc4random() % OBFUSCATE_MAX_PADDING;
	message_length = buffer_length = padding_length + sizeof(struct seed_msg);

        if(obfuscation->use_http_prefix)
        {
            buffer_length += prefix_length;
        }

	current_ptr = buffer = malloc(buffer_length);

        if(obfuscation->use_http_prefix)
        {
            memcpy(buffer, prefix, prefix_length);
            current_ptr += prefix_length;
        }

        struct seed_msg *seed_msg = (struct seed_msg*) current_ptr;
        memcpy(seed_msg->seed_buffer, obfuscation->seed, OBFUSCATE_SEED_LENGTH);
	seed_msg->magic = htonl(OBFUSCATE_MAGIC_VALUE);
	seed_msg->padding_length = htonl(padding_length);
	for(i = 0; i < (int)padding_length; i++) {
		if(i % 4 == 0)
			rnd = arc4random();
		seed_msg->padding[i] = rnd & 0xff;
		rnd >>= 8;
	}
	obfuscate_output(((u_char *)seed_msg) + OBFUSCATE_SEED_LENGTH,
		message_length - OBFUSCATE_SEED_LENGTH,
                obfuscation);
	atomicio(vwrite, session->socket_fd, buffer, buffer_length);
	free(buffer);
}

ssize_t
_libssh2_obfuscation_send(int  sock, const void *buffer, size_t length,
              int flags, void **abstract)
{
    LIBSSH2_OBFUSCATION *obfuscation = (LIBSSH2_OBFUSCATION *) (*abstract);
    u_char *mutable_buffer = malloc(length);
    memcpy(mutable_buffer, buffer, length);
    obfuscate_output(mutable_buffer, length, obfuscation);
    ssize_t rc = atomicio(vwrite, sock, mutable_buffer, length);
    free(mutable_buffer);
    if (rc < 0 )
        return -errno;
    return rc;
}

ssize_t
_libssh2_obfuscation_recv(int sock, void *buffer, size_t length, int flags, void **abstract)
{
    LIBSSH2_OBFUSCATION *obfuscation = (LIBSSH2_OBFUSCATION *) (*abstract);
    ssize_t rc = recv(sock, buffer, length, flags);
    if (rc < 0 ){
        if ( errno == ENOENT )
            return -EAGAIN;
        else
            return -errno;
    }
    if(rc > 0)
    {
        obfuscate_input(buffer, rc, obfuscation);
    }
    return rc;
}

int
_libssh2_obfuscation_initialize(LIBSSH2_OBFUSCATION *obfuscation)
{
    int i, rnd;
    u_char seed[OBFUSCATE_SEED_LENGTH];
    for(i = 0; i < OBFUSCATE_SEED_LENGTH; i++) {
        if(i % 4 == 0)
            rnd = arc4random();
        seed[i] = rnd & 0xff;
        rnd >>= 8;
    }
    memcpy(obfuscation->seed, seed, OBFUSCATE_SEED_LENGTH);

    u_char output_key[OBFUSCATE_KEY_LENGTH]; //client_to_sever
    u_char input_key[OBFUSCATE_KEY_LENGTH]; //server_to_client

    if(0 != generate_key(obfuscation, "client_to_server", strlen("client_to_server"), output_key))
    {
        return -1;
    }
    if(0 != generate_key(obfuscation, "server_to_client", strlen("server_to_client"), input_key))
    {
        return -1;
    }

    RC4_set_key(&obfuscation->rc4_input, OBFUSCATE_KEY_LENGTH, input_key);
    RC4_set_key(&obfuscation->rc4_output, OBFUSCATE_KEY_LENGTH, output_key);

}

int 
_libssh2_obfuscate_skip_http_prefix(LIBSSH2_SESSION *session)
{
    int rc, ret;
    char c;

    int socket = session->socket_fd;

    for(int i = 0; i < 50; i++)
    {
        rc = atomicio(read, socket, &c, 1);
        if (rc < 0 )
            return -errno;
        ret = do_ssh_obfuscation_prefix(session->obfuscation, c);
        if(ret == 0)
        {
            return 0;
        } 
    }
    return -1;
}

static int
generate_key(LIBSSH2_OBFUSCATION* obfuscation, const u_char *iv, u_int iv_len, u_char *key_data)
{
	EVP_MD_CTX ctx;
	u_char md_output[EVP_MAX_MD_SIZE];
	int md_len;
	int i;
	u_char *buffer;
	u_char *p;
	u_int buffer_length;
        const char *seed = obfuscation->seed;
        const char *keyword = obfuscation->keyword;

	buffer_length = OBFUSCATE_SEED_LENGTH + iv_len;
	if(keyword)
		buffer_length += strlen(keyword);

	p = buffer = malloc(buffer_length);

	memcpy(p, seed, OBFUSCATE_SEED_LENGTH);
	p += OBFUSCATE_SEED_LENGTH;

	if(keyword) {
		memcpy(p, keyword, strlen(keyword));
		p += strlen(keyword);
	}
	memcpy(p, iv, iv_len);

	EVP_DigestInit(&ctx, EVP_sha1());
	EVP_DigestUpdate(&ctx, buffer, buffer_length);
	EVP_DigestFinal(&ctx, md_output, &md_len);

	free(buffer);

	for(i = 0; i < OBFUSCATE_HASH_ITERATIONS; i++) {
		EVP_DigestInit(&ctx, EVP_sha1());
		EVP_DigestUpdate(&ctx, md_output, md_len);
		EVP_DigestFinal(&ctx, md_output, &md_len);
	}

	if(md_len < OBFUSCATE_KEY_LENGTH) 
		return -1;

	memcpy(key_data, md_output, OBFUSCATE_KEY_LENGTH);
        return 0;
}

static void
obfuscate_input(u_char *buffer, u_int buffer_len, LIBSSH2_OBFUSCATION *obfuscation)
{
    RC4(&obfuscation->rc4_input, buffer_len, buffer, buffer);
}

static void
obfuscate_output(u_char *buffer, u_int buffer_len, LIBSSH2_OBFUSCATION *obfuscation)
{
    RC4(&obfuscation->rc4_output, buffer_len, buffer, buffer);
}

static int 
do_ssh_obfuscation_prefix(LIBSSH2_OBFUSCATION* obfuscation, unsigned char c)
{
    crBegin(obfuscation->do_ssh_obfuscation_prefix_state);

    // PSIPHON HTTP-PREFIX
    // Skip all bytes up to and including the prefix terminator, <CR><LF><CR><LF>

    for (;;) {
    while (c != '\r')
        crReturn(1);
    crReturn(1);
    if (c != '\n') continue;
    crReturn(1);
    if (c != '\r') continue;
    crReturn(1);
    if (c != '\n') continue;
    break;
    }

    crFinish(0);
}
