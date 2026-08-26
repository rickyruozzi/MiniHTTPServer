#include "HTTPserver.h"
#include "HTMLmanagement.h"
#include "Response.h"

#include <stdarg.h>

#define INDEX_TEMPLATE_PATH "templates/index.html"

char **read_credentials_from_file(const char *filename, size_t *num_credentials) {
    if (filename == NULL || num_credentials == NULL) {
        return NULL;
    }
    *num_credentials = 0;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long file_size = ftell(file); //file dimension
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file); //restart from the beginning of the file
    char *buffer = malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    size_t read_bytes = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);
    buffer[read_bytes] = '\0';
    size_t capacity = 10;
    char **credentials = malloc(capacity * sizeof(char *));
    if (credentials == NULL) {
        free(buffer);
        return NULL;
    }
    char *saveptr = NULL;
    char *token = strtok_r(buffer, "\n", &saveptr);
    while (token != NULL) {
        token[strcspn(token, "\r")] = '\0'; // Remove any carriage return characters, inserting the string terminator at the first occurrence of '\r'
        if (*num_credentials >= capacity) {  /* FIX: dereferenziato correttamente */
            capacity *= 2;
            char **new_credentials = realloc(credentials, capacity * sizeof(char *));
            if (new_credentials == NULL) {
                for (size_t i = 0; i < *num_credentials; i++) {
                    free(credentials[i]);
                }
                free(credentials);
                free(buffer);
                return NULL;
            }
            credentials = new_credentials;
        }
        credentials[*num_credentials] = strdup(token); //strdup allocates memory and copies the string
        if (credentials[*num_credentials] == NULL) {
            for (size_t i = 0; i < *num_credentials; i++) {
                free(credentials[i]);
            }
            free(credentials);
            free(buffer);
            return NULL;
        }
        printf("TOKEN: [%s]\n", token);
        printf("CREDENTIAL: [%s]\n", credentials[*num_credentials]);
        (*num_credentials)++; 
        token = strtok_r(NULL, "\n", &saveptr); //get the next token
    }
    free(buffer);
    return credentials;
}

int extract_json_string_value(const char *json, const char *key, char *output, size_t max_len) {
    if (!json || !key || !output || max_len == 0) {
        return -1;
    }

    // Costruisce il pattern da cercare con le virgolette: "key"
    char pattern[128];
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
        return -1; // Nome della chiave troppo lungo
    }

    // 1. Cerca la chiave nel JSON
    const char *ptr = strstr(json, pattern);
    if (!ptr) {
        return -1; // Chiave non trovata
    }

    ptr += strlen(pattern); // Salta il nome della chiave

    // 2. Salta eventuali spazi prima dei due punti ':'
    while (*ptr && isspace((unsigned char)*ptr)) {
        ptr++;
    }

    if (*ptr != ':') {
        return -1; // Sintassi JSON non valida
    }
    ptr++; // Salta i due punti ':'

    // 3. Salta eventuali spazi prima della virgoletta iniziale del valore
    while (*ptr && isspace((unsigned char)*ptr)) {
        ptr++;
    }

    if (*ptr != '"') {
        return -1; // Il valore non è una stringa (es. è un numero, un boolean o null)
    }
    ptr++; // Salta la virgoletta d'apertura '"'

    // 4. Copia i caratteri fino alla virgoletta di chiusura (gestendo gli escape '\"')
    size_t out_idx = 0;
    while (*ptr && out_idx < max_len - 1) {
        // Se troviamo un carattere di escape '\', copiamo il carattere successivo
        if (*ptr == '\\' && *(ptr + 1) != '\0') {
            ptr++; // Salta il backslash
            output[out_idx++] = *ptr;
            ptr++;
            continue;
        }

        // Se troviamo la virgoletta di chiusura (non sfuggita), abbiamo finito
        if (*ptr == '"') {
            output[out_idx] = '\0';
            return 0; // Successo!
        }

        output[out_idx++] = *ptr;
        ptr++;
    }

    // Se siamo arrivati qui senza trovare la virgoletta di chiusura
    // oppure il buffer di output è diventato pieno:
    output[out_idx] = '\0';
    return -1;
}


int extract_json_double_value(const char *json, const char *key, double *output) {
    if (!json || !key || !output) {
        return -1;
    }

    // Costruisce il pattern da cercare: "key"
    char pattern[128];
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) {
        return -1;
    }

    // 1. Cerca la chiave
    const char *ptr = strstr(json, pattern);
    if (!ptr) {
        return -1;
    }

    ptr += strlen(pattern);

    // 2. Salta gli spazi prima dei due punti ':'
    while (*ptr && isspace((unsigned char)*ptr)) {
        ptr++;
    }

    if (*ptr != ':') {
        return -1;
    }
    ptr++; // Salta ':'

    // 3. Salta gli spazi prima del valore
    while (*ptr && isspace((unsigned char)*ptr)) {
        ptr++;
    }

    // Se il numero è racchiuso tra virgolette (es. "19.99"), salta la prima virgoletta
    if (*ptr == '"') {
        ptr++;
    }

    // 4. Converte la stringa in double
    char *endptr;
    double val = strtod(ptr, &endptr);

    // Se endptr coincide con ptr, non è stato letto alcun numero valido
    if (endptr == ptr) {
        return -1;
    }

    *output = val;
    return 0; // Successo
}

//used to print formatted string into a buffer, updating the used size and checking for errors
static int appendf(char *dst, size_t dst_size, size_t *used, const char *fmt, ...){
    va_list args; //arguments_list
    int written;  //used to store the number of characters written by vsnprintf
    if(dst == NULL || used == NULL || fmt == NULL || *used >= dst_size){
        perror("wrong params");
        return -1;
    }
    va_start(args, fmt);
    written = vsnprintf(dst + *used, dst_size - *used, fmt, args); //printing the formatted string in the buffer
    va_end(args); //cleaning the argument list; 
    if(written < 0 || (size_t)written >= (dst_size - *used)){
        perror("vsnprintf error"); //error handling if nothing has been written
        return -1;
    }
    *used += (size_t)written; //updating the number of characters written
    return 0;
}

//used for escapes translation in html, for example & becomes &amp; and < becomes &lt;
static int append_html_escaped(char *dst, size_t dst_size, size_t *used, const char *src){
    size_t i; 
    if(src == NULL){
        return appendf(dst, dst_size, used, ""); //if the source string is NULL, we append an empty string to the destination buffer
    }
    for(int i=0; src[i] != '\0'; i++){
        switch(src[i]){
            case '&':
                if(appendf(dst, dst_size, used, "&amp;") != 0) return -1;
                break;
            case '<':
                if(appendf(dst, dst_size, used, "&lt;") != 0) return -1;
                break;
            case '>':
                if(appendf(dst, dst_size, used, "&gt;") != 0) return -1;
                break;
            case '"':
                if(appendf(dst, dst_size, used, "&quot;") != 0) return -1;
                break;
            case '\'':
                if(appendf(dst, dst_size, used, "&#39;") != 0) return -1;
                break;
            default:
                if(appendf(dst, dst_size, used, "%c", src[i]) != 0) return -1;
                break;
        }
    }
}

static int append_product_card(char *dst, size_t dst_size, size_t *used, const char *name, const char *description, const char *price) {
    if(appendf(dst, dst_size, used, "\n     <article class=\"card\">\n          <h3>") != 0) {
        return -1; // Error appending opening article and h3 tags
    }
    if(append_html_escaped(dst, dst_size, used, name) != 0) {
        return -1; // Error appending escaped product name
    }
    if(appendf(dst, dst_size, used, "</h3>\n          <p>") != 0) {
        return -1; // Error appending closing h3 and opening p tags
    }
    if(append_html_escaped(dst, dst_size, used, description) != 0) {
        return -1; // Error appending escaped product description
    }
    if(appendf(dst, dst_size, used, "</p>\n          <p class=\"price\">Da EUR ") != 0) {
        return -1; // Error appending closing p and opening price tags
    }
    if(append_html_escaped(dst, dst_size, used, price) != 0) {
        return -1; // Error appending escaped product price
    }
    return appendf(dst, dst_size, used, "</p>\n        </article>\n");
}

static int load_products_html(char *products_html, size_t products_html_size) {
	size_t used = 0;

#ifdef HAVE_MYSQL_CAPI
	MYSQL *conn = NULL; //connection handler
	MYSQL_RES *result = NULL; //query result handler
	MYSQL_ROW row; //row handler

    size_t num_credentials = 5;

    char **credentials = read_credentials_from_file("credentials.txt", &num_credentials);  

    const char *host= credentials[0];
	const char *user = credentials[1];
	const char *password = credentials[2];
	const char *database = credentials[3];
	const char *port_str = credentials[4];
	unsigned int port = 3306;

	if (host == NULL || host[0] == '\0') host = "";
	if (user == NULL || user[0] == '\0') user = "";
	if (password == NULL) password = "";
	if (database == NULL || database[0] == '\0') database = "";
	if (port_str != NULL && port_str[0] != '\0') {
		unsigned long port_ul = strtoul(port_str, NULL, 10); //string to unsigned integer conversion
		if (port_ul > 0 && port_ul <= 65535UL) {
			port = (unsigned int)port_ul; //checking port range
		}
	}

	conn = mysql_init(NULL); //connection initialization
	if (conn == NULL) {
		return appendf(products_html, products_html_size, &used,
					   "\n        <article class=\"card\"><h3>Errore DB</h3><p>mysql_init fallita.</p><p class=\"price\">-</p></article>\n");
            // we use appendf to add an error message to the products_html buffer if mysql_init fails
	}

	if (mysql_real_connect(conn, host, user, password, database, port, NULL, 0) == NULL) { //trying to connect to the db        
        appendf(products_html, products_html_size, &used,
				"\n        <article class=\"card\"><h3>Errore DB</h3><p>Connessione fallita.</p><p class=\"price\">-</p></article>\n");
        printf("variables: host=%s \n user=%s \n password=%s \n database=%s \n port=%u\n", host, user, password, database, port);    
        mysql_close(conn);
        printf("MySQL error: %s\n", mysql_error(conn));
        printf("MySQL error code: %u\n", mysql_errno(conn));
        printf("mysql_get_client_info = %s\n", mysql_get_client_info());
		return 0;
	}

	if (mysql_query(conn, "SELECT name, description, price FROM products ORDER BY id DESC LIMIT 12") != 0) { //query execution
		appendf(products_html, products_html_size, &used,
				"\n        <article class=\"card\"><h3>Errore Query</h3><p>Query su products fallita.</p><p class=\"price\">-</p></article>\n");
		mysql_close(conn);
		return 0;
	}

	result = mysql_store_result(conn); //query result storing
	if (result == NULL) {
		appendf(products_html, products_html_size, &used,
				"\n        <article class=\"card\"><h3>Errore Query</h3><p>Nessun risultato disponibile.</p><p class=\"price\">-</p></article>\n");
		mysql_close(conn);
		return 0;
	}

	while ((row = mysql_fetch_row(result)) != NULL) {
		const char *name = (row[0] != NULL) ? row[0] : "Prodotto";
		const char *description = (row[1] != NULL) ? row[1] : "Descrizione non disponibile";
		const char *price = (row[2] != NULL) ? row[2] : "0.00";

		if (append_product_card(products_html, products_html_size, &used, name, description, price) != 0) {
			break;
		} //append the product card
	}

	if (used == 0) { //if no products were added, append a message indicating that there are no products
		appendf(products_html, products_html_size, &used,
				"\n        <article class=\"card\"><h3>Nessun prodotto</h3><p>La tabella products e vuota.</p><p class=\"price\">-</p></article>\n");
	}

	mysql_free_result(result); //free the result set
	mysql_close(conn); //close the connection
	return 0;
#else
	return appendf(products_html, products_html_size, &used,
				   "\n        <article class=\"card\"><h3>MySQL non disponibile</h3><p>Installa e linka MySQL Connector/C per usare il catalogo dinamico.</p><p class=\"price\">-</p></article>\n");
#endif
}

int html_render_homepage_with_products(char *output_html, size_t output_html_size) {
	char template_html[HTTP_RESPONSE_BODY_MAX] = {0};
	char products_html[4096] = {0};
	const char *catalog_section;
	const char *catalog_content_start;
	const char *catalog_section_end;
	size_t prefix_len;
	size_t products_len;
	size_t suffix_len;

	if (output_html == NULL || output_html_size == 0) {
		return -1;
	}

	if (read_html_file(INDEX_TEMPLATE_PATH, template_html, sizeof(template_html)) != 0) {
		return -1;
	} //read html file and put it in a buffer

	if (load_products_html(products_html, sizeof(products_html)) != 0) {
		return -1;
	} //load products from the db

	catalog_section = strstr(template_html, "<section id=\"catalogo\" class=\"grid\">"); //find the start of the catalog section
	if (catalog_section == NULL) {
		return -1;
	}

	catalog_content_start = strchr(catalog_section, '>'); //move the pointer at the end of the section tag
	if (catalog_content_start == NULL) {
		return -1;
	}
	catalog_content_start++; //move pointer to the next character after the '>' of the section tag

	catalog_section_end = strstr(catalog_content_start, "</section>"); //find the first occurrence of the closing section tag after the catalog section
	if (catalog_section_end == NULL) {
		return -1;
	}

	prefix_len = (size_t)(catalog_content_start - template_html); // calculate the length of the prefix (the part of the template before the catalog content)
	products_len = strlen(products_html); // calculate the length of the products HTML content
	suffix_len = strlen(catalog_section_end); // calculate the length of the suffix (the part of the template after the catalog content)

	if (prefix_len + products_len + suffix_len + 1 > output_html_size) {
		return -1;
	} // check if the output buffer is large enough to hold the final HTML content

	memcpy(output_html, template_html, prefix_len); // copy the prefix (the part of the template before the catalog content) to the output buffer
	memcpy(output_html + prefix_len, products_html, products_len); // copy the products HTML content to the output buffer, right after the prefix
	memcpy(output_html + prefix_len + products_len, catalog_section_end, suffix_len + 1); // copy the suffix (the part of the template after the catalog content) to the output buffer, right after the products HTML content, including the null terminator
    //+1 for the terminator

	return 0;
}

void handle_create_product(int client_fd, request_t *request) {
#ifdef HAVE_MYSQL_CAPI
    MYSQL *conn = NULL;
    MYSQL_STMT *stmt = NULL;
    MYSQL_BIND bind[3];
    char name[128] = {0};
    char description[512] = {0};
    double price = 0.0;
    unsigned long name_len = 0;
    unsigned long desc_len = 0;

    // char** credentials = read_credentials_from_file("credentials.txt", 5);

    const char *host = getenv("MYSQL_HOST");
	const char *user = getenv("MYSQL_USER");
	const char *password = getenv("MYSQL_PASSWORD");
	const char *database = getenv("MYSQL_DATABASE");
	const char *port_str = getenv("MYSQL_PORT");
    int port=3306;
    if (port_str != NULL && port_str[0] != '\0') {
		unsigned long port_ul = strtoul(port_str, NULL, 10); //string to unsigned integer conversion
		if (port_ul > 0 && port_ul <= 65535UL) {
			port = (unsigned int)port_ul; //checking port range
		}
	}

    if(host == NULL || host[0] == '\0') host = "127.0.0.1";
    if(user == NULL || user[0] == '\0') user = "root";
    if(password == NULL) password = "Riccardo595//";
    if(database == NULL || database[0] == '\0') database = "HTTPserver";
    if(port_str == NULL || port_str[0] == '\0') port_str = "3306";

    if (request == NULL || request->body[0] == '\0') {
        http_send_error_response(client_fd, HTTP_STATUS_BAD_REQUEST, "Body empty");
        return;
    }

    if (extract_json_string_value(request->body, "name", name, sizeof(name)) != 0 ||
        extract_json_string_value(request->body, "description", description, sizeof(description)) != 0 ||
        extract_json_double_value(request->body, "price", &price) != 0) {
        http_send_error_response(client_fd, HTTP_STATUS_BAD_REQUEST, "Invalid JSON payload");
        return;
    }

    conn = mysql_init(NULL);
    if (conn == NULL) {
        http_send_error_response(client_fd, HTTP_STATUS_INTERNAL_SERVER_ERROR, "MySQL initialization failed");
        return;
    }

    if (mysql_real_connect(conn, host, user, password, database, port, NULL, 0) == NULL) {
        http_send_error_response(client_fd, HTTP_STATUS_INTERNAL_SERVER_ERROR, "MySQL connection failed");
        mysql_close(conn);
        return;
    }

    stmt = mysql_stmt_init(conn);
    if (stmt == NULL) {
        http_send_error_response(client_fd, HTTP_STATUS_INTERNAL_SERVER_ERROR, "MySQL statement init failed");
        mysql_close(conn);
        return;
    }

    const char *insert_query = "INSERT INTO products (name, description, price) VALUES (?, ?, ?)";
    if (mysql_stmt_prepare(stmt, insert_query, strlen(insert_query)) != 0) {
        http_send_error_response(client_fd, HTTP_STATUS_INTERNAL_SERVER_ERROR, "MySQL statement prepare failed");
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return;
    }

    name_len = strlen(name);
    desc_len = strlen(description);

    memset(bind, 0, sizeof(bind));

    //bind is composed by 3 elements, one for each parameter in the SQL query. Each element specifies the type of the parameter, a pointer to the buffer that holds the value, and the length of the buffer.

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = name;
    bind[0].buffer_length = sizeof(name);
    bind[0].length = &name_len;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = description;
    bind[1].buffer_length = sizeof(description);
    bind[1].length = &desc_len;

    bind[2].buffer_type = MYSQL_TYPE_DOUBLE;
    bind[2].buffer = &price;
    bind[2].buffer_length = sizeof(price);

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        http_send_error_response(client_fd, HTTP_STATUS_INTERNAL_SERVER_ERROR, "bind failed");
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        http_send_error_response(client_fd, HTTP_STATUS_INTERNAL_SERVER_ERROR, "insert failed");
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return;
    }

    mysql_stmt_close(stmt);
    mysql_close(conn);

    http_send_json_response(client_fd, "{\"status\":\"ok\",\"message\":\"Product created successfully\"}");
#else
    http_send_error_response(client_fd, HTTP_STATUS_INTERNAL_SERVER_ERROR, "MySQL Connector/C not available");
    return;
#endif
}

