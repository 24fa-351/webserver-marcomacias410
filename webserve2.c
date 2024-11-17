#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "request.h"

// threaded; handles multiple messages; closes connection when done

#define PORT 8080

#define LISTEN_BACKLOG 5

void server_dispatch_request(Request* req, int fd) {
    printf("Dispatching request\n");
}

// input pointer is freed by this function
void handleConnection(int* sock_fd_ptr) {
    int sock_fd = *sock_fd_ptr;
    free(sock_fd_ptr);

    printf("Handling connection on %d\n", sock_fd);
    while (1) {
        Request* req = request_read_from_fd(sock_fd);
        if (req == NULL) {
            break;
        }
        request_print(req);

        server_dispatch_request(req, sock_fd);
        request_free(req);
    }
    printf("done with connection %d\n", sock_fd);
    close(sock_fd);
}

int main(int argc, char* argv[]) {
    int port = 0;
    if (argc == 3) {
        port = atoi(argv[2]);

    } else {
        port = PORT;
    }

    if (argc == 2 && !strcmp(argv[1], "--request")) {
        printf("Reading ONE request from stdin\n");
        Request* req = request_read_from_fd(0);
        if (req == NULL) {
            printf("Failed to read request\n");
            exit(1);
        }
        request_print(req);
        request_free(req);
        exit(0);
    }
    if (argc == 2 && !strcmp(argv[1], "--handle")) {
        printf("Handling ONE connection from stdin\n");
        int* sock_fd = malloc(sizeof(int));
        *sock_fd = 0;
        handleConnection(sock_fd);
        printf("Done handling connection\n");
        exit(0);
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in socket_address;
    memset(&socket_address, '\0', sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socket_address.sin_port = htons(port);

    printf("Binding to port %d\n", port);

    int returnval;

    returnval = bind(socket_fd, (struct sockaddr*)&socket_address,
                     sizeof(socket_address));
    if (returnval < 0) {
        perror("bind");
        return 1;
    }
    returnval = listen(socket_fd, LISTEN_BACKLOG);

    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);

    while (1) {
        pthread_t thread;
        int* client_fd_buf = malloc(sizeof(int));

        *client_fd_buf = accept(socket_fd, (struct sockaddr*)&client_address,
                                &client_address_len);

        printf("accepted connection on %d\n", *client_fd_buf);

        pthread_create(&thread, NULL, (void* (*)(void*))handleConnection,
                       (void*)client_fd_buf);
    }

    return 0;
}
