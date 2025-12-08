#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "mpu6050.h" 
#include "ssd1306.h"

// --- CONFIGURAÇÕES DE HARDWARE ---

// Pinos do I²C para o sensor MPU6050
#define I2C_SENSOR_PORTA i2c0
#define I2C_SENSOR_SDA 0
#define I2C_SENSOR_SCL 1

// Pinos do I²C para o display OLED
#define I2C_DISPLAY_PORTA i2c1
#define I2C_DISPLAY_SDA 14
#define I2C_DISPLAY_SCL 15
#define ENDERECO_OLED 0x3C

// Configuração de atualização
#define TEMPO_ATUALIZACAO_MS 200 // Atualiza a tela a cada 200ms

// --- VARIÁVEIS GLOBAIS ---

static ssd1306_t display_oled;
static mpu6050_data_t dados_sensor;

// --- FUNÇÕES DO SISTEMA ---

// Inicializa os barramentos I2C e os dispositivos
void configurar_hardware(void) {
    // 1. Configura I2C do MPU6050 (400kHz)
    i2c_init(I2C_SENSOR_PORTA, 400 * 1000);
    gpio_set_function(I2C_SENSOR_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SENSOR_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SENSOR_SDA);
    gpio_pull_up(I2C_SENSOR_SCL);

    // Inicializa o MPU6050
    mpu6050_init(I2C_SENSOR_PORTA);

    // 2. Configura I2C do Display OLED (400kHz)
    i2c_init(I2C_DISPLAY_PORTA, 400 * 1000);
    gpio_set_function(I2C_DISPLAY_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_DISPLAY_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_DISPLAY_SDA);
    gpio_pull_up(I2C_DISPLAY_SCL);

    // Inicializa o Display
    ssd1306_init(&display_oled, 128, 64, false, ENDERECO_OLED, I2C_DISPLAY_PORTA);
    ssd1306_config(&display_oled);
}

// Função para exibir os dados na tela OLED
void atualizar_display(void) {
    // Limpa o buffer da tela
    ssd1306_fill(&display_oled, false);

    // Título
    ssd1306_draw_string(&display_oled, "MPU6050 DATA", 20, 0, false);
    ssd1306_hline(&display_oled, 0, 127, 10, true);

    char linha[32];
    int y = 14; // Posição vertical inicial

    // Exibe Aceleração (X, Y, Z)
    snprintf(linha, sizeof(linha), "Acc X: %.2f", dados_sensor.accel_x);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 8;

    snprintf(linha, sizeof(linha), "Acc Y: %.2f", dados_sensor.accel_y);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 8;

    snprintf(linha, sizeof(linha), "Acc Z: %.2f", dados_sensor.accel_z);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 8; // Espaço extra

    // Exibe Giroscópio (X, Y, Z)
    snprintf(linha, sizeof(linha), "Gyr X: %.2f", dados_sensor.gyro_x);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 8;

    snprintf(linha, sizeof(linha), "Gyr Y: %.2f", dados_sensor.gyro_y);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);
    y += 8;

    snprintf(linha, sizeof(linha), "Gyr Z: %.2f", dados_sensor.gyro_z);
    ssd1306_draw_string(&display_oled, linha, 0, y, false);

    // Envia os dados para o display físico
    ssd1306_send_data(&display_oled);
}

// --- MAIN ---

int main(void) {
    stdio_init_all();

    // Configura I2C, MPU e Display
    configurar_hardware();

    // Loop principal
    while (1) {
        // Lê os dados brutos do sensor
        mpu6050_read_data(&dados_sensor);

        // Atualiza a interface gráfica
        atualizar_display();

        // Aguarda antes da próxima leitura para não piscar demais o display
        sleep_ms(TEMPO_ATUALIZACAO_MS);
    }
}