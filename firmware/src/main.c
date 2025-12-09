#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "mpu6050.h"
#include "ssd1306.h"
#include "tflm_wrapper.h"
#include "scaler_params.h"

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

// --- GLOBAL VARIABLES ---

static ssd1306_t oled_display;
static mpu6050_data_t sensor_data;
static int predicted_level = -1; // -1 indicates no prediction yet
static float confidence = 0.0f;

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
        ssd1306_draw_string(&oled_display, line, 35, 30, false);

        snprintf(line, sizeof(line), "Acc: %.1f%%", confidence * 100.0f);
        ssd1306_draw_string(&oled_display, line, 30, 45, false);
    } else {
        ssd1306_draw_string(&oled_display, "Aguardando...", 15, 35, false);
    }

    ssd1306_send_data(&oled_display);
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

    printf("--- Starting Inference Loop ---\n");

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

        printf("Prediction: %d (Confidence: %.1f%%)\n\n", predicted_level, confidence * 100.0f);

        // Update the display
        update_display();

        // Wait before the next reading
        sleep_ms(UPDATE_TIME_MS);
    }
}
