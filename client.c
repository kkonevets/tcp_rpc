#include <arpa/inet.h>  // inet_addr()
#include <assert.h>
#include <msgpack/unpack.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // bzero()
#include <sys/socket.h>
#include <unistd.h>  // read(), write(), close()

#include <msgpack.h>

#define PORT 8080
#define SA struct sockaddr


int proxy_connect_to_server()
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        return -1;
    } else {
        printf("Socket successfully created..\n");
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        return -2;
    } else {
        printf("connected to the server..\n");
    }

    return sockfd;
}

void proxy_send_and_rcv(int sockfd, const msgpack_sbuffer* in)
{
    if (write(sockfd, in->data, in->size) == -1) {
        printf("write error\n");
        goto cleanup;
    }

    /* Initialize the unpacker with initial buffer size. */
    /* It can expand later. */
    msgpack_unpacker unp;
    if (!msgpack_unpacker_init(&unp, MSGPACK_UNPACKER_INIT_BUFFER_SIZE)) {
        printf("Out of memory\n");
        goto cleanup;
    }

    msgpack_unpacked und;
    msgpack_unpacked_init(&und);

    for (;;) {
        // add more space if free space is not enough
        if (unp.free < MSGPACK_UNPACKER_INIT_BUFFER_SIZE) {
            if (!msgpack_unpacker_reserve_buffer(
                    &unp, MSGPACK_UNPACKER_INIT_BUFFER_SIZE)) {
                printf("Out of memory\n");
                goto cleanup;
            }
        }
        // pointer to a current offset in an unpacker buffer
        char* buff = msgpack_unpacker_buffer(&unp);
        // read up to free space in a buffer
        int n = read(sockfd, buff, unp.free);
        if (n == -1) {
            printf("got read error\n");
            goto cleanup;
        } else if (n == 0) {
            // server closed a TCP connection
            printf("server closed a TCP connection\n");
            goto cleanup;
        } else {
            msgpack_unpacker_buffer_consumed(&unp, n);
        }

        msgpack_unpack_return ret;

        /* starts streaming deserialization. */
        while ((ret = msgpack_unpacker_next(&unp, &und))) {
            switch (ret) {
                case MSGPACK_UNPACK_SUCCESS: {
                    msgpack_object obj = und.data;
                    msgpack_object_print(stdout, obj);
                    puts("");
                }
                    goto cleanup;
                case MSGPACK_UNPACK_CONTINUE:
                    puts("MSGPACK_UNPACK_CONTINUE");
                    // message is not complete, get more data
                    break;
                case MSGPACK_UNPACK_EXTRA_BYTES:
                    // there is more data, that is not messages, just discard it
                    puts("MSGPACK_UNPACK_EXTRA_BYTES");
                    goto cleanup;
                case MSGPACK_UNPACK_PARSE_ERROR:
                    puts("MSGPACK_UNPACK_PARSE_ERROR");
                    goto cleanup;
                case MSGPACK_UNPACK_NOMEM_ERROR:
                    puts("MSGPACK_UNPACK_NOMEM_ERROR");
                    goto cleanup;
            }
        }
    }

cleanup:
    msgpack_unpacker_destroy(&unp);
    msgpack_unpacked_destroy(&und);
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

    int sockfd = proxy_connect_to_server();
    if (sockfd < 0) {
        goto cleanup;
    }
    proxy_send_and_rcv(sockfd, buffer);

cleanup:
    msgpack_sbuffer_free(buffer);
    msgpack_packer_free(pac);

    return 0;
}
