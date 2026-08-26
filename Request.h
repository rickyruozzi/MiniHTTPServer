#ifndef REQUEST_H
#define REQUEST_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char method[16];
	char path[256];
	char headers[1024];
	char body[2048];
} request_t;

void http_request_init(request_t *req);
void http_request_free(request_t *req);
int http_request_has_body(const request_t *req);
const char *http_request_get_method(const request_t *req);
const char *http_request_get_path(const request_t *req);

#endif // REQUEST_H