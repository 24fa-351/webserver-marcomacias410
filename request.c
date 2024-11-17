#include "request.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

bool read_request_line(Request* req, int fd);
bool read_headers(Request* req, int fd);
bool read_body(Request* req, int fd);

static int total_requests = 0;
static int total_bytes_received = 0;
static int total_bytes_sent = 0;

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

void static_request(Request* req, int client_sock) {
    char response[4096];
    if (!strcmp((req)->method, "GET") && !strncmp((req)->path, "/static/", 8)) {
        char* path = (req)->path + 8;
        char full_path[2048];

        char file_name[1024];

        sscanf((req)->path, "/static/%s", file_name);

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
                 "%s 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Disposition: %s; filename=\"%s\"\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n",
                 req->version, mime_type, disposition, full_path, file_size);

        write(client_sock, response, strlen(response));
        write(client_sock, file_contents, file_size);
        total_bytes_sent += strlen(response) + file_size;

        close(file_fd);
        free(file_contents);

    } else {
        snprintf(response, sizeof(response),
                 "%s 405 Method Not Allowed\r\n"
                 "Content-Length: 0\r\n"
                 "\r\n",
                 req->version);
        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);
    }
}

void return_stats(Request* req, int client_sock) {
    char response[1024];
    if (!strcmp((req)->method, "GET") && !strncmp((req)->path, "/stats/", 7)) {
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
                 "%s 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %d\r\n"
                 "\r\n"
                 "<html><body>\n"
                 "<h1>Server Statistics</h1>\n"
                 "<p>Total Requests: %d</p>\n"
                 "<p>Total Bytes Received: %d</p>\n"
                 "<p>Total Bytes Sent: %d</p>\n"
                 "</body></html>\n",
                 req->version, content_length, total_requests,
                 total_bytes_received, total_bytes_sent);

        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);

    } else {
        snprintf(response, sizeof(response),
                 "%s 405 Method Not Allowed\r\n"
                 "Content-Length: 0\r\n"
                 "\r\n",
                 req->version);
        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);
    }
}

void calc_request(Request* req, int client_sock) {
    char response[1024];

    if (!strcmp((req)->method, "GET") && !strncmp((req)->path, "/calc/", 6)) {
        int a, b;

        if (sscanf((req)->path, "/calc/%d/%d", &a, &b) == 2) {
            int result = a + b;

            int content_length = snprintf(NULL, 0, "%d", result);
            snprintf(response, sizeof(response),
                     "%s 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %d\r\n"
                     "\r\n"
                     "Sum of a and b: %d\n",
                     req->version, content_length, result);

            write(client_sock, response, strlen(response));
            total_bytes_sent += strlen(response);

        } else {
            snprintf(response, sizeof(response),
                     "%s 405 Method Not Allowed\r\n"
                     "Content-Length: 0\r\n"
                     "\r\n",
                     req->version);
            write(client_sock, response, strlen(response));
            total_bytes_sent += strlen(response);
        }
    } else {
        snprintf(response, sizeof(response),
                 "%s 405 Method Not Allowed\r\n"
                 "Content-Length: 0\r\n"
                 "\r\n",
                 req->version);
        write(client_sock, response, strlen(response));
        total_bytes_sent += strlen(response);
    }
}

Request* request_read_from_fd(int fd) {
    printf("Reading request from fd %d\n", fd);

    Request* req = malloc(sizeof(Request));

    if (read_request_line(req, fd) == false) {
        printf("Failed to read request line\n");
        request_free(req);
        return NULL;
    }
    // read_headers also eats the second CRLF
    if (read_headers(req, fd) == false) {
        printf("Failed to read headers\n");
        request_free(req);
        return NULL;
    }
    if (read_body(req, fd) == false) {
        printf("Failed to read body\n");
        request_free(req);
        return NULL;
    }

    return req;
}

void request_print(Request* req) {
    printf("vvv Request vvv\n");
    if (req->method) {
        printf("Method: %s\n", req->method);
    }
    if (req->path) {
        printf("Path: %s\n", req->path);
    }
    if (req->version) {
        printf("Version: %s\n", req->version);
    }
    for (int ix = 0; ix < req->header_count; ix++) {
        printf("Header %d: %s %s\n", ix + 1, req->headers[ix].key,
               req->headers[ix].value);
    }

    printf("^^^ Request ^^^\n");
}

#define FREE_IF_NOT_NULL(ptr) \
    if (ptr) {                \
        free(ptr);            \
    }
void request_free(Request* req) {
    printf("freeing request\n");
    if (req == NULL) {
        return;
    }
    FREE_IF_NOT_NULL(req->method);
    FREE_IF_NOT_NULL(req->path);
    FREE_IF_NOT_NULL(req->version);

    for (int ix = 0; ix < req->header_count; ix++) {
        FREE_IF_NOT_NULL(req->headers[ix].key);
        FREE_IF_NOT_NULL(req->headers[ix].value);
    }
    FREE_IF_NOT_NULL(req->headers);

    free(req);
}

// return an allocated string; CRLF not present
char* read_line(int fd) {
    printf("Reading line from fd %d\n", fd);
    char* line = malloc(10000);
    int len_read = 0;
    while (1) {
        char ch;
        int number_bytes_read = read(fd, &ch, 1);
        total_bytes_received += number_bytes_read;
        if (number_bytes_read <= 0) {
            return NULL;
        }
        if (ch == '\n') {
            break;
        }
        line[len_read] = ch;
        len_read++;
        line[len_read] = '\0';
    }
    if (len_read > 0 && line[len_read - 1] == '\r') {
        line[len_read - 1] = '\0';
    }
    line = realloc(line, len_read + 1);
    return line;
}
bool read_request_line(Request* req, int fd) {
    total_requests++;
    printf("Reading request line\n");
    char* line = read_line(fd);
    if (line == NULL) {
        return false;
    }
    req->method = malloc(strlen(line) + 1);
    req->path = malloc(strlen(line) + 1);
    req->version = malloc(strlen(line) + 1);
    int length_parsed;
    int number_parsed;

    number_parsed = sscanf(line, "%s %s %s%n", req->method, req->path,
                           req->version, &length_parsed);

    if (number_parsed != 3 || length_parsed != strlen(line)) {
        printf("Failed to parse request line\n");
        free(line);
        return false;
    }

    if (strcmp(req->method, "GET") != 0 && strcmp(req->method, "POST") != 0) {
        printf("Invalid method: %s\n", req->method);
        free(line);
        return false;
    }

    if (strncmp((req)->path, "/calc/", 6) == 0) {
        calc_request(req, fd);
        //*result = MESSAGE;

    } else if (strncmp((req)->path, "/stats/", 7) == 0) {
        return_stats(req, fd);
        // *result = MESSAGE;
    } else if (strncmp((req)->path, "/static/", 8) == 0) {
        static_request(req, fd);
        // *result = MESSAGE;
    }

    return true;
}
bool read_headers(Request* req, int fd) {
    printf("Reading headers\n");
    req->headers = malloc(sizeof(Header) * 100);
    req->header_count = 0;
    while (1) {
        char* line = read_line(fd);
        if (line == NULL) {
            // closed connection or error
            return false;
        }
        // is this our end of headers?
        if (strlen(line) == 0) {
            free(line);
            break;
        }

        // printf("line: %s\n", line);
        req->headers[req->header_count].key = malloc(10000);
        req->headers[req->header_count].value = malloc(10000);
        int number_parsed;
        int length_parsed;
        number_parsed =
            sscanf(line, "%s %s%n", req->headers[req->header_count].key,
                   req->headers[req->header_count].value, &length_parsed);
        printf("number_parsed: %d\n", number_parsed);
        if (number_parsed != 2 || length_parsed != strlen(line)) {
            printf("Failed to parse header\n");
            free(line);
            return false;
        }
        req->headers[req->header_count].key =
            realloc(req->headers[req->header_count].key,
                    strlen(req->headers[req->header_count].key) + 1);
        req->headers[req->header_count].value =
            realloc(req->headers[req->header_count].value,
                    strlen(req->headers[req->header_count].value) + 1);
        req->header_count++;
        free(line);
    }
    return true;
}

bool read_body(Request* req, int fd) {
    printf("Reading body\n");

    return true;
}
