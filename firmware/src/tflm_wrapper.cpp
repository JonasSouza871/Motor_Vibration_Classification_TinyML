#include <cstdio>
#include "pico/stdlib.h"

// -------------------------------------------------------------------
// TensorFlow Lite Micro (via pico-tflmicro)
// -------------------------------------------------------------------
// Biblioteca disponível em: git clone https://github.com/raspberrypi/pico-tflmicro.git
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Modelo convertido em array C (gerado pelo notebook Python)
#include "motor_model.h"

// Parâmetros de normalização (gerado pelo notebook Python)
#include "scaler_params.h"

// API em C que será chamada pelo main.c
#include "tflm_wrapper.h"

// -------------------------------------------------------------------
// Objetos estáticos do TFLM
// -------------------------------------------------------------------
namespace {

// Tamanho da arena de tensores (ajuste se der erro de memória)
constexpr int kTensorArenaSize = 10 * 1024;  // 10KB para o modelo de motor
alignas(16) static uint8_t tensor_arena[kTensorArenaSize];

// Logger de erros
static tflite::MicroErrorReporter micro_error_reporter;
static tflite::ErrorReporter* error_reporter = &micro_error_reporter;

// Modelo e intérprete
static const tflite::Model* model = nullptr;

// Registrador de operações (MLP com Dropout usa: Dense, ReLU, Softmax, Reshape)
static tflite::MicroMutableOpResolver<4> resolver;

// Intérprete e tensores de entrada/saída
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input_tensor = nullptr;
static TfLiteTensor* output_tensor = nullptr;

}  // namespace

// -------------------------------------------------------------------
// Inicializa o modelo TFLM
// -------------------------------------------------------------------
int tflm_init_model(void) {
    // Aponta para o modelo dentro do array motor_model (gerado pelo notebook)
    model = tflite::GetModel(motor_model);
    if (model == nullptr) {
        printf("Erro: modelo nulo.\n");
        return -1;
    }

    // Registrar apenas as operações usadas pelo MLP de motor
    // (FullyConnected + ReLU + Softmax + Reshape)
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddSoftmax();
    resolver.AddReshape();

    // Cria o intérprete estático usando a arena
    static tflite::MicroInterpreter static_interpreter(
        model,
        resolver,
        tensor_arena,
        kTensorArenaSize,
        nullptr,  // resource variables
        nullptr,  // profiler
        false     // use_recording_allocator
    );

    interpreter = &static_interpreter;

    // Aloca os tensores
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("AllocateTensors falhou.\n");
        return -2;
    }

    input_tensor  = interpreter->input(0);
    output_tensor = interpreter->output(0);

    if (!input_tensor || !output_tensor) {
        printf("Erro ao obter tensores de entrada/saida.\n");
        return -3;
    }

    printf("TFLM inicializado com sucesso.\n");
    printf("Dimensoes input: ");
    for (int i = 0; i < input_tensor->dims->size; i++) {
        printf("%d ", input_tensor->dims->data[i]);
    }
    printf("\n");

    printf("Dimensoes output: ");
    for (int i = 0; i < output_tensor->dims->size; i++) {
        printf("%d ", output_tensor->dims->data[i]);
    }
    printf("\n");

    return 0;
}

// -------------------------------------------------------------------
// Executa uma inferência no modelo de classificação de motor
// -------------------------------------------------------------------
int tflm_infer(const float in_features[6], float out_scores[4]) {
    if (!interpreter || !input_tensor || !output_tensor) {
        return -1;
    }

    // Normalizar os dados usando os parâmetros do StandardScaler
    // Fórmula: (valor - mean) / scale
    float normalized[6];
    for (int i = 0; i < 6; i++) {
        normalized[i] = (in_features[i] - scaler_mean[i]) / scaler_scale[i];
    }

    // Copia os 6 atributos normalizados para o tensor de input
    for (int i = 0; i < 6; i++) {
        input_tensor->data.f[i] = normalized[i];
    }

    // Executa o modelo
    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Invoke falhou.\n");
        return -2;
    }

    // Copia as 4 saídas (uma por classe: Level 0, 1, 2, 3)
    for (int i = 0; i < 4; i++) {
        out_scores[i] = output_tensor->data.f[i];
    }

    return 0;
}
