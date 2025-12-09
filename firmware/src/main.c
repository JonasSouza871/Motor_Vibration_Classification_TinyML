#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "mpu6050.h"
#include "ssd1306.h"
#include "tflm_wrapper.h"
#include "scaler_params.h"
#include "iot/pico_http_server.h"

// --- HARDWARE SETTINGS ---

// I2C pins for MPU6050 sensor
#define I2C_SENSOR_PORT i2c0
#define I2C_SENSOR_SDA 0
#define I2C_SENSOR_SCL 1

// I2C pins for OLED display
#define I2C_DISPLAY_PORT i2c1
#define I2C_DISPLAY_SDA 14
#define I2C_DISPLAY_SCL 15
#define OLED_ADDR 0x3C

// Update configuration
#define UPDATE_TIME_MS 1000 // Update screen every 1s

// WiFi credentials - CONFIGURE AQUI!
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// --- GLOBAL VARIABLES ---

static ssd1306_t oled_display;
static mpu6050_data_t sensor_data;
static int predicted_level = -1; // -1 indicates no prediction yet
static float confidence = 0.0f;
static float scores[4] = {0};
static float accel_data[3] = {0};
static float gyro_data[3] = {0};
static bool iot_enabled = false; // IoT system status

// --- SYSTEM FUNCTIONS ---

// Find the index of the maximum value in a float array
int argmax(const float* array, int size) {
    if (size <= 0) return -1;
    int max_index = 0;
    for (int i = 1; i < size; ++i) {
        if (array[i] > array[max_index]) {
            max_index = i;
        }
    }
    return max_index;
}

// Initialize I2C buses and devices
void setup_hardware(void) {
    // 1. Configure MPU6050 I2C (400kHz)
    i2c_init(I2C_SENSOR_PORT, 400 * 1000);
    gpio_set_function(I2C_SENSOR_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SENSOR_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SENSOR_SDA);
    gpio_pull_up(I2C_SENSOR_SCL);
    mpu6050_init(I2C_SENSOR_PORT);

    // 2. Configure OLED Display I2C (400kHz)
    i2c_init(I2C_DISPLAY_PORT, 400 * 1000);
    gpio_set_function(I2C_DISPLAY_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_DISPLAY_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_DISPLAY_SDA);
    gpio_pull_up(I2C_DISPLAY_SCL);
    ssd1306_init(&oled_display, 128, 64, false, OLED_ADDR, I2C_DISPLAY_PORT);
    ssd1306_config(&oled_display);
}

// Function to display data on the OLED screen
void update_display(void) {
    char line[32];

    ssd1306_fill(&oled_display, false);

    // Title
    ssd1306_draw_string(&oled_display, "MOTOR LEVEL", 28, 5, false);
    ssd1306_hline(&oled_display, 0, 127, 18, true);

    // Display predicted level
    if (predicted_level != -1) {
        snprintf(line, sizeof(line), "Nivel: %d", predicted_level);
        ssd1306_draw_string(&oled_display, line, 10, 28, false);

        snprintf(line, sizeof(line), "Conf: %.1f%%", confidence * 100.0f);
        ssd1306_draw_string(&oled_display, line, 10, 40, false);
    } else {
        ssd1306_draw_string(&oled_display, "Aguardando...", 10, 35, false);
    }

    // Display IoT status
    if (iot_enabled) {
        ssd1306_draw_string(&oled_display, "IoT: ON", 10, 52, false);
    }

    ssd1306_send_data(&oled_display);
}

// HTTP handler for homepage
static char html_buffer[8192];

const char* homepage_handler(const char* req) {
    http_server_set_content_type(HTTP_CONTENT_TYPE_HTML);

    snprintf(html_buffer, sizeof(html_buffer),
        "<!DOCTYPE html>"
        "<html lang='pt-BR'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>Motor Level Monitor</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:linear-gradient(135deg,#667eea 0%%,#764ba2 100%%);color:white}"
        ".container{max-width:800px;margin:0 auto;background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);border-radius:20px;padding:30px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
        "h1{text-align:center;margin-bottom:30px;font-size:2.5em;text-shadow:2px 2px 4px rgba(0,0,0,0.3)}"
        ".card{background:rgba(255,255,255,0.2);border-radius:15px;padding:20px;margin:15px 0;box-shadow:0 4px 6px rgba(0,0,0,0.1)}"
        ".level{font-size:4em;text-align:center;font-weight:bold;margin:20px 0;text-shadow:3px 3px 6px rgba(0,0,0,0.4)}"
        ".confidence{font-size:1.5em;text-align:center;margin:10px 0}"
        ".data-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:15px;margin-top:20px}"
        ".data-item{background:rgba(0,0,0,0.2);padding:15px;border-radius:10px}"
        ".data-label{font-size:0.9em;opacity:0.8;margin-bottom:5px}"
        ".data-value{font-size:1.3em;font-weight:bold}"
        ".bar-container{width:100%%;height:30px;background:rgba(0,0,0,0.3);border-radius:15px;overflow:hidden;margin:10px 0}"
        ".bar{height:100%%;background:linear-gradient(90deg,#00ff88,#00cc6a);transition:width 0.3s ease;display:flex;align-items:center;justify-content:center;font-weight:bold}"
        ".level-scores{margin-top:20px}"
        ".score-item{margin:10px 0}"
        "@keyframes pulse{0%%,100%%{opacity:1}50%%{opacity:0.7}}"
        ".live{animation:pulse 2s infinite}"
        "</style>"
        "<script>function refresh(){location.reload()}setInterval(refresh,2000);</script>"
        "</head>"
        "<body>"
        "<div class='container'>"
        "<h1>\xF0\x9F\x94\xA7 Motor Level Monitor</h1>"

        "<div class='card'>"
        "<div class='level live'>N\xC3\xADvel %d</div>"
        "<div class='confidence'>Acur\xC3\xA1cia: %.1f%%</div>"
        "</div>"

        "<div class='card level-scores'>"
        "<h2>Scores de Predi\xC3\xA7\xC3\xA3o</h2>"
        "<div class='score-item'>"
        "<div class='data-label'>N\xC3\xADvel 0</div>"
        "<div class='bar-container'><div class='bar' style='width:%.0f%%'>%.1f%%</div></div>"
        "</div>"
        "<div class='score-item'>"
        "<div class='data-label'>N\xC3\xADvel 1</div>"
        "<div class='bar-container'><div class='bar' style='width:%.0f%%'>%.1f%%</div></div>"
        "</div>"
        "<div class='score-item'>"
        "<div class='data-label'>N\xC3\xADvel 2</div>"
        "<div class='bar-container'><div class='bar' style='width:%.0f%%'>%.1f%%</div></div>"
        "</div>"
        "<div class='score-item'>"
        "<div class='data-label'>N\xC3\xADvel 3</div>"
        "<div class='bar-container'><div class='bar' style='width:%.0f%%'>%.1f%%</div></div>"
        "</div>"
        "</div>"

        "<div class='card'>"
        "<h2>Dados do Sensor</h2>"
        "<div class='data-grid'>"
        "<div class='data-item'><div class='data-label'>Accel X</div><div class='data-value'>%.2f</div></div>"
        "<div class='data-item'><div class='data-label'>Accel Y</div><div class='data-value'>%.2f</div></div>"
        "<div class='data-item'><div class='data-label'>Accel Z</div><div class='data-value'>%.2f</div></div>"
        "<div class='data-item'><div class='data-label'>Gyro X</div><div class='data-value'>%.2f</div></div>"
        "<div class='data-item'><div class='data-label'>Gyro Y</div><div class='data-value'>%.2f</div></div>"
        "<div class='data-item'><div class='data-label'>Gyro Z</div><div class='data-value'>%.2f</div></div>"
        "</div>"
        "</div>"

        "</div>"
        "</body>"
        "</html>",
        predicted_level,
        confidence * 100.0f,
        scores[0] * 100.0f, scores[0] * 100.0f,
        scores[1] * 100.0f, scores[1] * 100.0f,
        scores[2] * 100.0f, scores[2] * 100.0f,
        scores[3] * 100.0f, scores[3] * 100.0f,
        accel_data[0], accel_data[1], accel_data[2],
        gyro_data[0], gyro_data[1], gyro_data[2]
    );

    return html_buffer;
}

// --- MAIN ---

int main(void) {
    stdio_init_all();

    // Brief pause to allow serial monitor to connect
    sleep_ms(2000);
    printf("--- System Initializing ---\n");

    // Configure I2C, MPU, and Display
    setup_hardware();

    // Initialize the TinyML model
    if (tflm_init_model() != 0) {
        printf("Failed to initialize model.\n");
        ssd1306_draw_string(&oled_display, "Model Init Failed", 0, 0, false);
        ssd1306_send_data(&oled_display);
        while(1);
    }

    // Initialize IoT system (WiFi + Web Server)
    printf("\n--- IoT Initialization ---\n");
    if (http_server_init(WIFI_SSID, WIFI_PASSWORD) == 0) {
        printf("IoT System Ready!\n");

        // Register homepage handler
        http_request_handler_t handler = {
            .path = "/",
            .handler = homepage_handler
        };
        http_server_register_handler(handler);

        iot_enabled = true;
    } else {
        printf("WARNING: IoT system disabled. Continuing without WiFi.\n");
        iot_enabled = false;
    }

    printf("\n--- Starting Inference Loop ---\n");

    // Main loop
    while (1) {
        // Read raw sensor data
        mpu6050_read_data(&sensor_data);

        // Prepare features for the model (raw data)
        float in_features[6] = {
            sensor_data.accel_x,
            sensor_data.accel_y,
            sensor_data.accel_z,
            sensor_data.gyro_x,
            sensor_data.gyro_y,
            sensor_data.gyro_z
        };

        printf("Raw -> Acc(%.2f, %.2f, %.2f) Gyr(%.2f, %.2f, %.2f)\n",
               in_features[0], in_features[1], in_features[2],
               in_features[3], in_features[4], in_features[5]);

        // Run inference (normalization is handled inside tflm_infer)
        float out_scores[4];
        tflm_infer(in_features, out_scores);

        printf("Scores -> L0: %.3f, L1: %.3f, L2: %.3f, L3: %.3f\n",
               out_scores[0], out_scores[1], out_scores[2], out_scores[3]);

        // Get the predicted level
        predicted_level = argmax(out_scores, 4);
        confidence = out_scores[predicted_level];

        // Update global variables for web server
        scores[0] = out_scores[0];
        scores[1] = out_scores[1];
        scores[2] = out_scores[2];
        scores[3] = out_scores[3];
        accel_data[0] = in_features[0];
        accel_data[1] = in_features[1];
        accel_data[2] = in_features[2];
        gyro_data[0] = in_features[3];
        gyro_data[1] = in_features[4];
        gyro_data[2] = in_features[5];

        printf("Prediction: %d (Confidence: %.1f%%)\n\n", predicted_level, confidence * 100.0f);

        // Update the display
        update_display();

        // Poll WiFi stack if IoT is enabled
        if (iot_enabled) {
            cyw43_arch_poll();
        }

        // Wait before the next reading
        sleep_ms(UPDATE_TIME_MS);
    }
}
