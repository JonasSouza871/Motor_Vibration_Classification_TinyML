#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "mpu6050.h"
#include "ssd1306.h"
#include "tflm_wrapper.h"
#include "scaler_params.h"
#include "iot/pico_http_server.h"

//configs de hardware
#define I2C_SENSOR_PORT i2c0
#define I2C_SENSOR_SDA 0
#define I2C_SENSOR_SCL 1

#define I2C_DISPLAY_PORT i2c1
#define I2C_DISPLAY_SDA 14
#define I2C_DISPLAY_SCL 15
#define OLED_ADDR 0x3C

#define UPDATE_TIME_MS 1000 //tempo entre leituras

//trocar aqui pela rede de vcs
#define WIFI_SSID "NOME_DA_REDE"
#define WIFI_PASSWORD "SENHA_DA_REDE"

//globais
static ssd1306_t oled;
static mpu6050_data_t sensor_data;

//variaveis de estado pro server ler
static int predicted_level = -1; 
static float confidence = 0.0f;
static float scores[4] = {0};
static float raw_features[6] = {0}; //acc xyz, gyr xyz
static bool wifi_ok = false; //flag se conectou ou nao

//funcao auxiliar pra pegar o maior indice do array
int get_max_index(const float* arr, int size) {
    int max_i = 0;
    for (int i = 1; i < size; i++) {
        if (arr[i] > arr[max_i]) max_i = i;
    }
    return max_i;
}

void setup_hardware(void) {
    //config sensor mpu6050
    i2c_init(I2C_SENSOR_PORT, 400 * 1000);
    gpio_set_function(I2C_SENSOR_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SENSOR_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SENSOR_SDA); //tem que ter pullup
    gpio_pull_up(I2C_SENSOR_SCL);
    mpu6050_init(I2C_SENSOR_PORT);

    //config display oled
    i2c_init(I2C_DISPLAY_PORT, 400 * 1000);
    gpio_set_function(I2C_DISPLAY_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_DISPLAY_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_DISPLAY_SDA);
    gpio_pull_up(I2C_DISPLAY_SCL);
    
    ssd1306_init(&oled, 128, 64, false, OLED_ADDR, I2C_DISPLAY_PORT);
    ssd1306_config(&oled);
}

void update_oled(void) {
    char buf[32];
    ssd1306_fill(&oled, false); //limpa tela
    
    ssd1306_draw_string(&oled, "MOTOR MONITOR", 15, 0, false);
    ssd1306_hline(&oled, 0, 127, 12, true); //linha separadora

    if (predicted_level != -1) {
        snprintf(buf, sizeof(buf), "Nivel: %d", predicted_level);
        ssd1306_draw_string(&oled, buf, 10, 20, false);

        snprintf(buf, sizeof(buf), "Conf: %.1f%%", confidence * 100.0f);
        ssd1306_draw_string(&oled, buf, 10, 32, false);
    } else {
        ssd1306_draw_string(&oled, "Lendo...", 10, 25, false);
    }

    if (wifi_ok) ssd1306_draw_string(&oled, "IP: OK", 10, 50, false);
    else ssd1306_draw_string(&oled, "Wifi: OFF", 10, 50, false);

    ssd1306_send_data(&oled); //manda pro display
}

//bufferzao pro html
static char html_out[8192];

//handler da pagina principal
const char* root_handler(const char* req) {
    http_server_set_content_type(HTTP_CONTENT_TYPE_HTML);

    //montando o html na mao com snprintf
    //css inline pra nao precisar de arquivo extra
    snprintf(html_out, sizeof(html_out),
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta http-equiv='refresh' content='2'>" //refresh a cada 2s
        "<style>"
        "body{font-family:sans-serif;background:#1a1a1a;color:#fff;text-align:center;padding:10px}"
        ".card{background:#333;border-radius:10px;padding:15px;margin:10px auto;max-width:600px;box-shadow:0 4px 6px rgba(0,0,0,0.3)}"
        "h1{color:#4CAF50}.big{font-size:3em;margin:10px}.bar{background:#555;height:20px;border-radius:10px;overflow:hidden;margin:5px 0}"
        ".fill{height:100%%;background:#4CAF50}.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
        ".val{font-size:1.2em;color:#aaa}"
        "</style></head><body>"
        "<h1>Monitor de Motor IA</h1>"
        
        "<div class='card'>"
        "<div>Status Atual</div>"
        "<div class='big'>N&iacute;vel %d</div>"
        "<div>Confian&ccedil;a: %.1f%%</div>"
        "</div>"

        "<div class='card'><h3>Probabilidades</h3>"
        "L0: %.1f%%<div class='bar'><div class='fill' style='width:%.1f%%'></div></div>"
        "L1: %.1f%%<div class='bar'><div class='fill' style='width:%.1f%%'></div></div>"
        "L2: %.1f%%<div class='bar'><div class='fill' style='width:%.1f%%'></div></div>"
        "L3: %.1f%%<div class='bar'><div class='fill' style='width:%.1f%%'></div></div>"
        "</div>"

        "<div class='card'><h3>Sensores Raw</h3>"
        "<div class='grid'>"
        "<div>Ax: %.2f</div><div>Gx: %.2f</div>"
        "<div>Ay: %.2f</div><div>Gy: %.2f</div>"
        "<div>Az: %.2f</div><div>Gz: %.2f</div>"
        "</div></div>"
        "</body></html>",
        //injeta os valores aqui
        predicted_level, confidence * 100.0f,
        scores[0]*100, scores[0]*100,
        scores[1]*100, scores[1]*100,
        scores[2]*100, scores[2]*100,
        scores[3]*100, scores[3]*100,
        raw_features[0], raw_features[3],
        raw_features[1], raw_features[4],
        raw_features[2], raw_features[5]
    );

    return html_out;
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000); //espera usb conectar
    printf("\n=== Iniciando Monitor de Motor ===\n");

    setup_hardware();

    //inicia tflite
    if (tflm_init_model() != 0) {
        printf("Erro fatal: TFLite init falhou\n");
        while(1) { tight_loop_contents(); } //trava tudo
    }

    //tenta conectar wifi
    printf("Tentando conectar no Wifi %s...\n", WIFI_SSID);
    if (http_server_init(WIFI_SSID, WIFI_PASSWORD) == 0) {
        printf("Wifi Conectado! Servidor ON.\n");
        
        //registra rota home
        http_request_handler_t home = { .path = "/", .handler = root_handler };
        http_server_register_handler(home);
        
        wifi_ok = true;
    } else {
        printf("Falha no Wifi. Rodando offline.\n");
        wifi_ok = false; //segue sem net
    }

    printf("Loop principal iniciado...\n");

    while (true) {
        mpu6050_read_data(&sensor_data); //le o sensor

        //vetor pro modelo
        raw_features[0] = sensor_data.accel_x;
        raw_features[1] = sensor_data.accel_y;
        raw_features[2] = sensor_data.accel_z;
        raw_features[3] = sensor_data.gyro_x;
        raw_features[4] = sensor_data.gyro_y;
        raw_features[5] = sensor_data.gyro_z;

        //printf("Raw: %.2f %.2f...\n", raw_features[0], raw_features[1]); //debug

        float output_scores[4];
        tflm_infer(raw_features, output_scores); //roda inferencia

        //atualiza globais
        memcpy(scores, output_scores, sizeof(scores));
        predicted_level = get_max_index(scores, 4);
        confidence = scores[predicted_level];

        printf("Pred: Nivel %d (Conf: %.1f%%)\n", predicted_level, confidence * 100);

        update_oled(); //atualiza display

        if (wifi_ok) {
            cyw43_arch_poll(); //mantem wifi vivo
        }

        sleep_ms(UPDATE_TIME_MS); //delay
    }

    return 0;
}