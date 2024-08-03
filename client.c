#include <arpa/inet.h>  // inet_addr()
#include <netdb.h>
#include <sys/socket.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // bzero()
#include <unistd.h>   // read(), write(), close()
#include <errno.h>

#include <msgpack.h>
#include <msgpack/unpack.h>

#define LOG_SYSTEM_ERROR() (fprintf(stderr, "proxy: %s.\n", strerror(errno)));

// return -1 if error or
// socket file descriptor otherwise
int proxy_connect_to_server(const char* host, int port)
{
    int sockfd, connfd;
    struct sockaddr_in servaddr;

    // socket create and verify
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        LOG_SYSTEM_ERROR();
        return -1;
    } else {
        fprintf(stderr, "proxy: Socket successfully created.\n");
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
    servaddr.sin_port = htons(port);

    // connect the client socket to server socket
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        LOG_SYSTEM_ERROR();
        return -1;
    } else {
        fprintf(stderr, "proxy: Connected to the server %s.\n", host);
    }

    return sockfd;
}

// sends a packed message and blocks till gets a packed message in response
int proxy_send_and_rcv(int sockfd, const msgpack_sbuffer* in)
{
    if (write(sockfd, in->data, in->size) == -1) {
        LOG_SYSTEM_ERROR();
        return -1;
    }

    int ret = 0;

    msgpack_unpacked und;
    msgpack_unpacked_init(&und);

    /* Initialize the unpacker with initial buffer size. */
    /* It can expand later. */
    msgpack_unpacker unp;
    if (!msgpack_unpacker_init(&unp, MSGPACK_UNPACKER_INIT_BUFFER_SIZE)) {
        ret = -1;
        fprintf(stderr, "proxy: Out of memory\n");
        goto cleanup;
    }

    for (;;) {
        // add more space if free space is not enough
        if (unp.free < MSGPACK_UNPACKER_INIT_BUFFER_SIZE) {
            if (!msgpack_unpacker_reserve_buffer(
                    &unp, MSGPACK_UNPACKER_INIT_BUFFER_SIZE)) {
                ret = -1;
                fprintf(stderr, "proxy: Out of memory\n");
                goto cleanup;
            }
        }
        // pointer to a current offset in an unpacker buffer
        char* buff = msgpack_unpacker_buffer(&unp);
        // read no more than free space to a buffer
        int n = read(sockfd, buff, unp.free);
        if (n <= 0) {
            ret = -1;
            LOG_SYSTEM_ERROR();
            goto cleanup;
        } else {
            msgpack_unpacker_buffer_consumed(&unp, n);
        }

        /* starts streaming deserialization. */
        // try to unpack a first message, we don't need more
        switch (msgpack_unpacker_next(&unp, &und)) {
            case MSGPACK_UNPACK_SUCCESS: {
                msgpack_object obj = und.data;
                msgpack_object_print(stdout, obj);
            }
                goto cleanup;
            case MSGPACK_UNPACK_EXTRA_BYTES:
                // there is more data, that is not messages, just discard it
                goto cleanup;
            case MSGPACK_UNPACK_PARSE_ERROR:
                ret = -1;
                fprintf(stderr, "proxy: MSGPACK_UNPACK_PARSE_ERROR\n");
                goto cleanup;
            case MSGPACK_UNPACK_NOMEM_ERROR:
                ret = -1;
                fprintf(stderr, "proxy: MSGPACK_UNPACK_NOMEM_ERROR\n");
                goto cleanup;
            case MSGPACK_UNPACK_CONTINUE:
                // message is not complete, get more data
                break;
        }
    }

cleanup:
    msgpack_unpacker_destroy(&unp);
    msgpack_unpacked_destroy(&und);

    return ret;
}


int main()
{
    msgpack_sbuffer* buffer = msgpack_sbuffer_new();
    msgpack_packer* pac = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    /* serializes [42, "Hello", "World!"]. */
    msgpack_pack_array(pac, 3);
    msgpack_pack_int(pac, 42);
    msgpack_pack_bin(pac, 5);
    msgpack_pack_bin_body(pac, "Hello", 5);
    msgpack_pack_bin(pac, 6);
    msgpack_pack_bin_body(pac, "World!", 6);

    int sockfd = proxy_connect_to_server("127.0.0.1", 8080);
    if (sockfd < 0) {
        goto cleanup;
    }
    proxy_send_and_rcv(sockfd, buffer);

cleanup:
    msgpack_sbuffer_free(buffer);
    msgpack_packer_free(pac);

    return 0;
}
