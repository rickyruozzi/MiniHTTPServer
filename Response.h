#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_RESPONSE_BODY_MAX 16384

typedef struct {
	int status_code;
	char headers[1024];
	char body[HTTP_RESPONSE_BODY_MAX];
	size_t body_len;
	char content_type[64];
} response_t;

void http_response_init(response_t *res);
void http_response_free(response_t *res);
void http_send_response(HTTPserver_t *server, int client_fd, const response_t *resp);
void http_send_html_response(HTTPserver_t *server, int client_fd, const char *html_content);
void http_send_json_response(HTTPserver_t *server, int client_fd, const char *json_content);
void http_send_error_response(HTTPserver_t *server, int client_fd, int status_code, const char *error_message);
void http_send_redirect_response(HTTPserver_t *server, int client_fd, const char *location);

#endif // RESPONSE_H