#include "HTTPserver.h"
#include "Request.h"
#include "Response.h"
#include "HTMLmanagement.h"

static int is_ip_banned(uint32_t addr, HTTPserver_t *server) {
    BanList *current = server->head;
    if (current == NULL) {
        return 0; // No banned IPs
    }
    do {
        struct in_addr banned_addr;
        if (inet_pton(AF_INET, current->ip_address, &banned_addr) == 1 &&
            banned_addr.s_addr == addr) {
            return 1; // IP is banned
        }
        current = current->next;
    } while (current != server->head);
    return 0; // IP is not banned

}

#define TEMPLATE_DIR "templates"

#ifdef _WIN32
#define CLOSE_SOCKET(s) closesocket(s)
#else
#define CLOSE_SOCKET(s) close(s)
#endif

HTTPserver_t *http_server_init(uint16_t port){
    HTTPserver_t *server = malloc(sizeof(HTTPserver_t));
    if (!server) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    server->port = port;
    memset(&server->server_addr, 0, sizeof(server->server_addr));
    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_addr.s_addr = INADDR_ANY;
    server->server_addr.sin_port = htons(port);
    server->head = NULL;
    server->server_socket = create_server_socket(port);
    return server;
}

void http_request_init(request_t *req){
    memset(req, 0, sizeof(request_t));
    req->method[0] = '\0';
    req->path[0] = '\0';
    req->headers[0] = '\0';
    req->body[0] = '\0';
}

void http_request_free(request_t *req){
    (void)req;
}

int http_request_has_body(const request_t *req){
    return strlen(req->body) > 0;
}

const char *http_request_get_method(const request_t *req){
    return req->method;
}

void http_response_init(response_t *res){
    memset(res, 0, sizeof(response_t));
    res->status_code = HTTP_STATUS_OK;
    res->content_type[0] = '\0';
    res->headers[0] = '\0';
    res->body[0] = '\0';
    res->body_len = 0;
}

void http_response_free(response_t *res){
    free(res);
}

//function insert the structure fields on a buffer and return the buffer
void http_send_response(int client_fd, const response_t *resp) {
    char header[1024];
    int header_len;
    header_len = snprintf(
        header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        resp->status_code,
        resp->content_type,
        resp->body_len
    );
    send(client_fd, header, header_len, 0);
    send(client_fd, resp->body, resp->body_len, 0);
}

//setting the response as a json response
void http_send_json_response(int client_fd, const char *json_content){
    response_t *response; 
    response = malloc(sizeof(response_t));
    http_response_init(response); //initialize the response structure
    response->status_code = HTTP_STATUS_OK; 
    snprintf(response->body, sizeof(response->body), "%s", json_content != NULL ? json_content : "{}");
    response->body_len = strlen(response->body);
    strncpy(response->content_type, "application/json", sizeof(response->content_type) - 1);
    http_send_response(client_fd, response);
    http_response_free(response); //free the response structure
}

//setting the response as a html response
void http_send_html_response(int client_fd, const char *html_content){
    response_t *response; 
    response = malloc(sizeof(response_t));
    http_response_init(response); //initialize the response structure
    response->status_code = HTTP_STATUS_OK; 
    snprintf(response->body, sizeof(response->body), "%s", html_content != NULL ? html_content : "");
    response->body_len = strlen(response->body);
    strncpy(response->content_type, "text/html", sizeof(response->content_type) - 1);
    http_send_response(client_fd, response);
    http_response_free(response);
}

//setting the response as a error response
void http_send_error_response(int client_fd, int status_code, const char *error_message){
    response_t *response; 
    response = malloc(sizeof(response_t));
    http_response_init(response); //initialize the response structure
    response->status_code = status_code; 
    snprintf(response->body, sizeof(response->body), "%s", error_message != NULL ? error_message : "Error");
    response->body_len = strlen(response->body);
    strncpy(response->content_type, "text/plain", sizeof(response->content_type) - 1);
    http_send_response(client_fd, response);
    http_response_free(response);
}

//setting the response as a redirect response
void http_send_redirect_response(int client_fd, const char *location){
    response_t *response; 
    response = malloc(sizeof(response_t));
    http_response_init(response); //initialize the response structure
    response->status_code = HTTP_STATUS_REDIRECT; 
    snprintf(response->headers, sizeof(response->headers), "Location: %s\r\n", location);
    response->body_len = 0;
    strncpy(response->content_type, "text/plain", sizeof(response->content_type) - 1);
    http_send_response(client_fd, response);
    http_response_free(response);
}

const char *http_request_get_path(const request_t *req){
    return req->path;
}


//metodo per accettare i client che arrivano al server
void http_accept_client(HTTPserver_t *server, HTTPclient_t *client){
    socklen_t addr_len = sizeof(client->client_addr);
    client->client_socket = accept(server->server_socket, (struct sockaddr *)&client->client_addr, &addr_len);
    if (client->client_socket < 0) {
        perror("accept");
        return;
    }

    if (is_ip_banned(client->client_addr.sin_addr.s_addr, server)) {
        printf("Connection from banned IP: %s\n", inet_ntoa(client->client_addr.sin_addr));
        CLOSE_SOCKET(client->client_socket);
        return;
    }

    http_handle_connection(client->client_socket);
}

void http_handle_connection(int client_fd){
    char buffer[4096];
    ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        CLOSE_SOCKET(client_fd);
        return;
    }
    buffer[received] = '\0';
    request_t request;
    parse_http_request(buffer, &request);
    handle_request(client_fd, &request);
    CLOSE_SOCKET(client_fd);
}

void handle_request(int client_fd, request_t *request){
    if (strcmp(request->method, "GET") == 0) {
        handle_get_request(client_fd, request);
        return;
    }
    if (strcmp(request->method, "POST") == 0) {
        handle_post_request(client_fd, request);
        return;
    }
    http_send_error_response(client_fd, HTTP_STATUS_BAD_REQUEST, "Unsupported HTTP method");
}

int create_server_socket(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        CLOSE_SOCKET(server_fd);
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        CLOSE_SOCKET(server_fd);
        return -1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        CLOSE_SOCKET(server_fd);
        return -1;
    }
    return server_fd;
}

void parse_http_request(const char *raw, request_t *request){
    char method[16];
    char path[256];
    char version[32];
    const char *header_end;

    http_request_init(request);
    if (sscanf(raw, "%15s %255s %31s", method, path, version) >= 2) {
        snprintf(request->method, sizeof(request->method), "%s", method);
        snprintf(request->path, sizeof(request->path), "%s", path);
    }

    header_end = strstr(raw, "\r\n\r\n");
    if (header_end != NULL) {
        const char *body_start = header_end + 4;
        snprintf(request->body, sizeof(request->body), "%s", body_start);
    }
}

void handle_get_request(int client_fd, request_t *request){
    char file_path[256] = {0};
    char html_content[HTTP_RESPONSE_BODY_MAX] = {0};

    if (strcmp(request->path, "/") == 0) {
        if (html_render_homepage_with_products(html_content, sizeof(html_content)) == 0) {
            http_send_html_response(client_fd, html_content);
            return;
        }
    }

    resolve_get_path(request->path, file_path, sizeof(file_path));

    if (read_html_file(file_path, html_content, sizeof(html_content)) == 0) {
        http_send_html_response(client_fd, html_content);
        return;
    }

    if (strcmp(file_path, TEMPLATE_DIR "/404.html") != 0 &&
        read_html_file(TEMPLATE_DIR "/404.html", html_content, sizeof(html_content)) == 0) {
        http_send_html_response(client_fd, html_content);
    } else {
        http_send_error_response(client_fd, HTTP_STATUS_NOT_FOUND, "404 Not Found");
    }
}

int read_html_file(const char *file_path, char *buffer, size_t buffer_size) {
    FILE *file;
    size_t bytes_read;

    if (file_path == NULL || buffer == NULL || buffer_size < 2) {
        return -1;
    }

    file = fopen(file_path, "rb");
    if (file == NULL) {
        return -1;
    }

    bytes_read = fread(buffer, 1, buffer_size - 1, file);
    if (ferror(file)) {
        fclose(file);
        return -1;
    }

    buffer[bytes_read] = '\0';
    fclose(file);
    return 0;
}

void handle_post_request(int client_fd, request_t *request) {
    if(strcmp(request->path, "/products")==0){
        handle_create_product(client_fd, request);
        return;
    }
    else{
        http_send_error_response(client_fd, HTTP_STATUS_BAD_REQUEST, "Invalid endpoint");
        return;
    }
    response_t response;
    char body_response[512];
    const char *payload = request->body[0] != '\0' ? request->body : "{}";

    http_response_init(&response);
    response.status_code = HTTP_STATUS_OK;
    snprintf(body_response, sizeof(body_response),
             "{\"status\":\"ok\",\"method\":\"POST\",\"received\":%s}",
             payload);
    snprintf(response.body, sizeof(response.body), "%s", body_response);
    response.body_len = strlen(response.body);
    snprintf(response.content_type, sizeof(response.content_type), "application/json");

    http_send_response(client_fd, &response);
}

void resolve_get_path(const char *path, char *file_path, size_t file_path_size) {
    if (path == NULL || file_path == NULL || file_path_size == 0) {
        return;
    }

    if (strcmp(path, "/") == 0) {
        snprintf(file_path, file_path_size, TEMPLATE_DIR "/index.html");
        return;
    }

    if (strcmp(path, "/404") == 0 || strcmp(path, "/404.html") == 0) {
        snprintf(file_path, file_path_size, TEMPLATE_DIR "/404.html");
        return;
    }

    //check if path is valid, if not it add the extension .html automatically, if the path is valid it return the path as it is
    if (path[0] == '/' && strstr(path, "..") == NULL) {
        const char *route_name = path + 1;
        if (strchr(route_name, '.') == NULL) {
            snprintf(file_path, file_path_size, TEMPLATE_DIR "/%s.html", route_name);
            return;
        }

        if (strcmp(strrchr(route_name, '.'), ".html") == 0) {
            snprintf(file_path, file_path_size, TEMPLATE_DIR "/%s", route_name);
            return;
        }
    }

    snprintf(file_path, file_path_size, TEMPLATE_DIR "/404.html");
}

void ban_ip(HTTPserver_t *server, const char *ip_address) {
    BanList *new_ban = malloc(sizeof(BanList));
    BanList *tail;
    if (!new_ban) {
        perror("malloc");
        return;
    }
    if(server == NULL || ip_address == NULL) {
        free(new_ban);
        return;
    }

    // Check if the IP is already banned
    if(is_ip_banned(inet_addr(ip_address), server)) {
        free(new_ban);
        return; // IP is already banned
    }

    strncpy(new_ban->ip_address, ip_address, sizeof(new_ban->ip_address) - 1);
    new_ban->ip_address[sizeof(new_ban->ip_address) - 1] = '\0';
    if (server->head == NULL) {
        new_ban->next = new_ban;
        server->head = new_ban;
        return;
    }
    tail = server->head;
    while (tail->next != server->head) {
        tail = tail->next;
    }
    new_ban->next = server->head;
    tail->next = new_ban;
    server->head = new_ban;
}

void unban_ip(HTTPserver_t *server, const char *ip_address) {
    BanList *current = server->head;
    BanList *prev = NULL;
    BanList *tail;
    if (current == NULL) {
        return;
    }
    do {
        if (strcmp(current->ip_address, ip_address) == 0) {
            if (current->next == current) {
                server->head = NULL;
            } else if (prev == NULL) {
                tail = current;
                while (tail->next != current) {
                    tail = tail->next;
                }
                tail->next = current->next;
                server->head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    } while (current != server->head);
}

int main(void) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    HTTPserver_t *server = http_server_init(8080);
    if (server->server_socket < 0) {
        free(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    while (1) {
        HTTPclient_t client = {0};
        http_accept_client(server, &client);
    }

    CLOSE_SOCKET(server->server_socket);
    free(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
