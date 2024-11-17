#ifndef REQUEST_H
#define REQUEST_H

#include <stdbool.h>

typedef struct {
    char* key;
    char* value;
} Header;

typedef struct {
    char* method;
    char* path;
    char* version;
    int header_count;
    Header* headers;
} Request;

Request* request_read_from_fd(int fd);

void request_print(Request* req);

void request_free(Request* req);

bool read_headers(Request* req, int fd);

#endif
