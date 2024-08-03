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
#include <msgpack/object.h>

#define LOG_SYSTEM_ERROR() (fprintf(stderr, "proxy: %s.\n", strerror(errno)));

// return -1 if error,
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


// Sends a packed message and blocks till gets a packed message in response,
// then unpacks it.
// sockfd - server socket
// msg - message to send
// unp - unpacker, keeps data buffer for reading
// und - unpacked message to return
int proxy_send_and_rcv(
    int sockfd,
    const msgpack_sbuffer* msg,
    msgpack_unpacker* unp,
    msgpack_unpacked* und)
{
    if (write(sockfd, msg->data, msg->size) == -1) {
        LOG_SYSTEM_ERROR();
        return -1;
    }

    // read data till the end of a first message
    for (;;) {
        // add more space if free space is not enough
        if (unp->free < MSGPACK_UNPACKER_INIT_BUFFER_SIZE) {
            if (!msgpack_unpacker_reserve_buffer(
                    unp, MSGPACK_UNPACKER_INIT_BUFFER_SIZE)) {
                fprintf(stderr, "proxy: Out of memory\n");
                return -1;
            }
        }
        // pointer to a current offset in an unpacker buffer
        char* buff = msgpack_unpacker_buffer(unp);
        // fill free space of the buffer
        int consumed = read(sockfd, buff, unp->free);
        if (consumed <= 0) {
            LOG_SYSTEM_ERROR();
            return -1;
        } else {
            msgpack_unpacker_buffer_consumed(unp, consumed);
        }

        /* starts streaming deserialization. */
        // try to unpack a first message, we don't need more
        switch (msgpack_unpacker_next(unp, und)) {
            case MSGPACK_UNPACK_SUCCESS:
                return 0;
            case MSGPACK_UNPACK_EXTRA_BYTES:
                // there is more data, that is not messages, just discard it
                return -1;
            case MSGPACK_UNPACK_PARSE_ERROR:
                fprintf(stderr, "proxy: MSGPACK_UNPACK_PARSE_ERROR\n");
                return -1;
            case MSGPACK_UNPACK_NOMEM_ERROR:
                fprintf(stderr, "proxy: MSGPACK_UNPACK_NOMEM_ERROR\n");
                return -1;
            case MSGPACK_UNPACK_CONTINUE:
                // message is incomplete, get more data
                break;
        }
    }

    return 0;
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
        return -1;
    }

    /* Initialize the unpacker with initial buffer size. */
    /* It can expand later. */
    msgpack_unpacker unp;
    if (!msgpack_unpacker_init(&unp, MSGPACK_UNPACKER_INIT_BUFFER_SIZE)) {
        fprintf(stderr, "proxy: Out of memory\n");
        goto cleanup;
    }

    msgpack_unpacked und;
    msgpack_unpacked_init(&und);

    if (proxy_send_and_rcv(sockfd, buffer, &unp, &und) == -1) {
        goto cleanup;
    }

    msgpack_object msg = und.data;
    if (msg.type == MSGPACK_OBJECT_ARRAY) {
        for (size_t i = 0; i < msg.via.array.size; ++i) {
            msgpack_object el = msg.via.array.ptr[i];
            if (el.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                printf("%lu ", el.via.u64);
            }
        }
    }
    puts("");
    // msgpack_object_print(stdout, msg);

cleanup:
    msgpack_sbuffer_free(buffer);
    msgpack_packer_free(pac);

    msgpack_unpacker_destroy(&unp);
    msgpack_unpacked_destroy(&und);
    close(sockfd);

    return 0;
}
