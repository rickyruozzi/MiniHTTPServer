#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#define _DEFAULT_SOURCE   // oppure _BSD_SOURCE (deprecata) o _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <stdbool.h>
#include "Request.h"

#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#if defined(__has_include)
#if __has_include(<mysql/mysql.h>)
#include <mysql/mysql.h>
#define HAVE_MYSQL_CAPI 1
#elif __has_include(<mysql.h>)
#include <mysql.h>
#define HAVE_MYSQL_CAPI 1
#endif
#endif


#include <openssl/ssl.h>
#include <openssl/err.h>


typedef struct BanList{
    char ip_address[16];
    struct BanList* next;
} BanList;

typedef struct {
    int server_socket; 
    struct sockaddr_in server_addr; 
    uint16_t port;
    BanList* head;
    SSL_CTX *ssl_context;
} HTTPserver_t;

typedef struct {
    HTTPserver_t *server;
    int max_connections;
    bool enable_logging;
    BanList* banned_ips;
} server_config_t;

typedef struct {
    int client_socket; 
    struct sockaddr_in client_addr; 
} HTTPclient_t;

typedef struct {
    HTTPserver_t *server;
    HTTPclient_t *clients;
} Connections_t;


enum HTTPMethod {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_UNKNOWN
};

enum MimeType {
    MIME_TEXT_HTML,
    MIME_TEXT_PLAIN,
    MIME_APPLICATION_JSON,
    MIME_IMAGE_PNG,
    MIME_IMAGE_JPEG,
    MIME_UNKNOWN
};

typedef struct {
    char path[256];
    char file_path[256];
    enum HTTPMethod method;
    enum MimeType mime_type;
} Endpoint_t;

enum HTTPstatus_code {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
    HTTP_STATUS_REDIRECT = 302
} ;

HTTPserver_t *http_server_init(uint16_t port);
void http_accept_client(HTTPserver_t *server, HTTPclient_t *client);
void http_handle_connection(HTTPserver_t *server, int client_fd);
void resolve_get_path(const char *path, char *file_path, size_t file_path_size);


void ban_ip(HTTPserver_t *server, const char *ip_address);
void unban_ip(HTTPserver_t *server, const char *ip_address);

void parse_http_request(const char *raw, request_t *request);
void handle_request(HTTPserver_t *server, int client_fd, request_t *request);
void handle_get_request(HTTPserver_t *server, int client_fd, request_t *request);
void handle_post_request(HTTPserver_t *server, int client_fd, request_t *request);
int create_server_socket(int port);
int read_html_file(const char *file_path, char *buffer, size_t buffer_size);
void handle_create_product(HTTPserver_t *server, int client_fd, request_t *request);
#endif