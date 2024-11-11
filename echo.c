#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 46645
#define LISTEN_BACKLOG 5

#include "http_message.h"






int respond_to_http_client_message(int sock_fd, http_client_message_t* http_msg) {
    char * response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    write(sock_fd, response, strlen(response));
    return 0;
}


void handleConnection(int *sock_fd_ptr) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer)); 





    int sock_fd = *sock_fd_ptr;
    free(sock_fd_ptr);

    http_client_message_t *http_msg;
    http_read_result_t result;

    int bytes_read = 0;
    int total_requests = 0;

    int bytes_received = 0;
    while (1) {
    
        read_http_client_message(sock_fd, &http_msg, &result);


        if (result == BAD_REQUEST) {
            printf("Bad Request\n");
            close(sock_fd);
            return;
        } else if (result == CLOSED_CONNECTION) {
            printf("Closed Connection\n");
            close(sock_fd);
            return;
        }


        respond_to_http_client_message(sock_fd, http_msg);
        http_client_message_free(http_msg);

     
    }

    
   close(sock_fd);


}

int main(int argc, char *argv[]) {
    int port_number = 0;
    if (argc == 3) {
        port_number = atoi(argv[2]);

    } else {
        port_number = DEFAULT_PORT;
    }
    printf("Number %d\n", port_number);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in socket_address;
    memset(&socket_address, '\0', sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);

    socket_address.sin_port = htons(port_number);

    int returnval;

    returnval = bind(socket_fd, (struct sockaddr *)&socket_address,
                     sizeof(socket_address));
    
    if(returnval < 0){
        perror("Error binding socket\n");
        exit(1);
    }

    returnval = listen(socket_fd, LISTEN_BACKLOG);

    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(client_address);

    while (1) {
        pthread_t thread;

        int *client_fd_buf = malloc(sizeof(int));

        *client_fd_buf = accept(socket_fd, (struct sockaddr *)&client_address,
                                &client_address_len);
    
        pthread_create(&thread, NULL, (void *(*)(void *))handleConnection,
                       (void *)client_fd_buf);

    }

    return 0;
}