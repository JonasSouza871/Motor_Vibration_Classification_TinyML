#include "pico_http_server.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pico/stdio.h"

#define MAX_HANDLERS 10

//globais do server
static http_request_handler_t handlers[MAX_HANDLERS];
static int handler_count = 0;
static const char *homepage_content = NULL;
static http_content_type_t curr_content_type = HTTP_CONTENT_TYPE_HTML;

//estado da conexao pra segurar o buffer
struct http_state {
    char response[16384]; //bufferzao pra caber o html
    size_t len;
    size_t sent;
};

//callback quando termina de enviar os dados
static err_t http_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    
    if (hs->sent >= hs->len) {
        tcp_close(tpcb); //fechou
        free(hs); //libera memoria do state
    }
    return ERR_OK;
}

//logica principal pra processar a url
static void handle_request(struct http_state *hs, const char *req_line) {
    char *path_end = strchr(req_line, ' ');
    if (!path_end) {
        hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 400 Bad Request\r\n\r\n");
        return;
    }

    //copia o path pra uma string limpa
    size_t path_len = path_end - req_line;
    char path[path_len + 1];
    strncpy(path, req_line, path_len);
    path[path_len] = '\0';

    //se for a home e tiver conteudo setado
    if ((strcmp(path, "/") == 0) && homepage_content) {
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
            (int)strlen(homepage_content), homepage_content);
        return;
    }

    //varre os handlers registrados
    for (int i = 0; i < handler_count; i++) {
        if (strstr(path, handlers[i].path) && handlers[i].handler) {
            const char *content = handlers[i].handler(req_line);
            
            //define o mime type
            const char *mime = "text/html";
            if (curr_content_type == HTTP_CONTENT_TYPE_JSON) mime = "application/json";
            else if (curr_content_type == HTTP_CONTENT_TYPE_PLAIN) mime = "text/plain";

            hs->len = snprintf(hs->response, sizeof(hs->response),
                "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                mime, (int)strlen(content), content);
            return;
        }
    }

    //nao achou nada
    hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 404 Not Found\r\n\r\n");
}

//chegou dados tcp
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    //aloca o state pra essa conexao
    struct http_state *hs = (struct http_state *)malloc(sizeof(struct http_state));
    hs->sent = 0;

    char *req = (char *)p->payload;
    if (strstr(req, "GET ") == req) {
        handle_request(hs, req + 4); //pula o "GET "
    } else {
        hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent_callback);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);
    
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv_callback);
    return ERR_OK;
}

int http_server_init(const char *ssid, const char *password) {
    if (cyw43_arch_init()) {
        printf("erro init cyw43\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Tentando conectar em %s...\n", ssid);
    
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 15000)) {
        printf("falha wifi connect\n");
        return -1;
    }
    printf("Wifi on! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    //sobe o tcp na porta 80
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);

    return 0;
}

// --- API publica ---

void http_server_set_homepage(const char *html_content) {
    homepage_content = html_content;
}

void http_server_register_handler(http_request_handler_t handler) {
    if (handler_count < MAX_HANDLERS) {
        handlers[handler_count++] = handler;
    }
}

void http_server_set_content_type(http_content_type_t type) {
    curr_content_type = type;
}

//parser simples pra pegar float da url (tipo ?temp=25.5)
void http_server_parse_float_param(const char *req, const char *param, float *value) {
    char *found = strstr(req, param);
    if (found) *value = atof(found + strlen(param));
}

//funcao auxiliar pra ler html do fs
//nota: precisa ter suporte a fs habilitado no cmake
char *http_server_read_html_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("erro abrir arquivo %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *raw = (char *)malloc(size + 1);
    if (!raw) {
        fclose(file);
        return NULL;
    }

    fread(raw, 1, size, file);
    raw[size] = '\0';
    fclose(file);

    //minificando na mao (tirando quebra de linha)
    //pra economizar espaco no buffer de envio
    char *mini = (char *)malloc(size + 1);
    int idx = 0;
    for (int i = 0; i < size; i++) {
        if (raw[i] != '\n' && raw[i] != '\r') {
            mini[idx++] = raw[i];
        }
    }
    mini[idx] = '\0';
    free(raw);

    return mini;
}