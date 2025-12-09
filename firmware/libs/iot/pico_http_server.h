#ifndef PICO_HTTP_SERVER_H
#define PICO_HTTP_SERVER_H

#include "pico/cyw43_arch.h"
#include "lwip/err.h"
#include "lwip/tcp.h"

//tipos de retorno suportados
typedef enum {
    HTTP_CONTENT_TYPE_HTML,
    HTTP_CONTENT_TYPE_JSON,
    HTTP_CONTENT_TYPE_PLAIN
} http_content_type_t;

//struct pra mapear url -> funcao
typedef struct {
    const char *path;
    const char *(*handler)(const char *);
} http_request_handler_t;

// --- Core ---

//inicia wifi e sobe o server na porta 80
int http_server_init(const char *ssid, const char *password);

//define o html da rota /
void http_server_set_homepage(const char *html_content);

//registra novas rotas (ex: /api/dados)
void http_server_register_handler(http_request_handler_t handler);

// --- Utils ---

//muda o content-type da resposta atual (padrao eh html)
void http_server_set_content_type(http_content_type_t type);

//helper pra catar float da query string (ex: ?val=10.5)
void http_server_parse_float_param(const char *req, const char *param, float *value);

//le arquivo do fs e retorna string alocada (lembrar de dar free)
char *http_server_read_html_file(const char *filename);

#endif