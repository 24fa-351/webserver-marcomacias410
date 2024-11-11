#include "http_message.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int total_requests = 0;
static int total_bytes_received = 0;
static int total_bytes_sent = 0;



bool is_complete_http_message(char* buffer) {
    size_t len = strlen(buffer);
    if (len < 4) {
        return false;
    }
    if (strncmp(buffer, "GET", 3) != 0) {
        return false;
    }

    if (strncmp(buffer + len - 4, "\r\n\r\n", 4) == 0) {
        return true;
    }
    return false;
}


void calc_request(int client_sock, http_client_message_t** msg) {
   

    char response[1024];  
                        
    if (!strcmp((*msg)->method, "GET") && !strncmp((*msg)->path, "/calc/", 6)) {
        int a, b;

        if (sscanf((*msg)->path, "/calc/%d/%d", &a, &b) == 2) {
         

            int result = a + b;

         
            int content_length = snprintf(NULL, 0, "%d", result);
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %d\r\n"
                     "\r\n"
                     "%d\n",
                     content_length, result);

            write(client_sock, response, strlen(response));
            total_bytes_sent += strlen(response);
        } else {
            const char* response =
                "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
            write(client_sock, response, strlen(response));
            total_bytes_sent += strlen(response);
        }
    } else {
        const char* response =
            "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);
    }
}

void return_stats(int client_sock, http_client_message_t** msg) {
    if (!strcmp((*msg)->method, "GET") &&
        !strncmp((*msg)->path, "/stats/", 7)) {
        char response[1024];
        int content_length =
            snprintf(NULL, 0,
                     "<html><body>\n"
                     "<h1>Server Statistics</h1>\n"
                     "<p>Total Requests: %d</p>\n"
                     "<p>Total Bytes Received: %d</p>\n"
                     "<p>Total Bytes Sent: %d</p>\n"
                     "</body></html>\n",
                     total_requests, total_bytes_received, total_bytes_sent);
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %d\r\n"
                 "\r\n"
                 "<html><body>\n"
                 "<h1>Server Statistics</h1>\n"
                 "<p>Total Requests: %d</p>\n"
                 "<p>Total Bytes Received: %d</p>\n"
                 "<p>Total Bytes Sent: %d</p>\n"
                 "</body></html>\n",
                 content_length, total_requests, total_bytes_received,
                 total_bytes_sent);

        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);
    
    } else {
        char* response =
            "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);
        
    }

}

const char* get_mime_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    if (strcmp(ext, ".html") == 0) {
        return "text/html";
    } else if (strcmp(ext, ".css") == 0) {
        return "text/css";
    } else if (strcmp(ext, ".js") == 0) {
        return "application/javascript";
    } else if (strcmp(ext, ".png") == 0) {
        return "image/png";
    } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(ext, ".gif") == 0) {
        return "image/gif";
    } else if (strcmp(ext, ".pdf") == 0) {
        return "application/pdf";
    } else {
        return "application/octet-stream";
    }
}



void static_request(int client_sock, http_client_message_t** msg) {
    char response[4096];
    if (!strcmp((*msg)->method, "GET") &&
        !strncmp((*msg)->path, "/static/", 8)) {
        char* path = (*msg)->path + 8;
        char full_path[2048];

        char file_name[1024];

        sscanf((*msg)->path, "/static/%s", file_name);

        snprintf(full_path, sizeof(full_path), "./%s", file_name);


        int file_fd = open(full_path, O_RDONLY);
        if (file_fd < 0) {
            const char* response =
                "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            write(client_sock, response, strlen(response));
            total_bytes_sent += strlen(response);
            return;
        }

        off_t file_size = lseek(file_fd, 0, SEEK_END);
        lseek(file_fd, 0, SEEK_SET);

        char* file_contents = malloc(file_size);
        read(file_fd, file_contents, file_size);

        const char* mime_type = get_mime_type(full_path);
        const char* disposition = "inline";
        if (strcmp(mime_type, "application/octet-stream") == 0) {
            disposition = "attachment";
        }

        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Disposition: %s; filename=\"%s\"\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n",
                 mime_type, disposition, full_path, file_size);

        write(client_sock, response, strlen(response));
        write(client_sock, file_contents, file_size);
        total_bytes_sent += strlen(response) + file_size;

        close(file_fd);
        free(file_contents);

    } else {
        const char* response =
            "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);
    }
}

void read_http_client_message(int client_sock, http_client_message_t** msg,
                              http_read_result_t* result) {
    *msg = malloc(sizeof(http_client_message_t));

    char buffer[1024];
  
    memset(buffer, 0, sizeof(buffer));

    while (!is_complete_http_message(buffer)) {
        int bytes_read = read(client_sock, buffer + strlen(buffer),
                              sizeof(buffer) - strlen(buffer));
        if (bytes_read == 0) {
            *result = CLOSED_CONNECTION;
            return;
        }
        if (bytes_read < 0) {
            *result = BAD_REQUEST;
            return;
        }

        total_bytes_received += bytes_read;
    }
    (*msg)->method = strndup(buffer, 3);

    (*msg)->path = strdup(buffer + 4);


total_requests++;

    if (strncmp((*msg)->path, "/calc/", 6) == 0) {
        calc_request(client_sock, msg);
        *result = MESSAGE;

    } else if (strncmp((*msg)->path, "/stats/", 7) == 0) {
        return_stats(client_sock, msg);
        *result = MESSAGE;
    } else if (strncmp((*msg)->path, "/static/", 8) == 0) {
        static_request(client_sock, msg);
        *result = MESSAGE;
    } else {
        *result = BAD_REQUEST;
        return;
    }
}

void http_client_message_free(http_client_message_t* msg) {
    free(msg->method);
    free(msg->path);
    free(msg->http_version);
    free(msg);
}
