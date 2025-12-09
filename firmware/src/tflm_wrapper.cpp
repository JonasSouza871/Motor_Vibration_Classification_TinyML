#include <cstdio>
#include "pico/stdlib.h"

//bibliotecas do tflite micro
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

//arquivos gerados pelo notebook
#include "motor_model.h"
#include "scaler_params.h" 
#include "tflm_wrapper.h" //header da api

//area de memoria pro tflite, se der erro de alloc tem que aumentar aqui
constexpr int kTensorArenaSize = 10 * 1024; 
alignas(16) static uint8_t tensor_arena[kTensorArenaSize];

static const tflite::Model* model = nullptr;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input_tensor = nullptr;
static TfLiteTensor* output_tensor = nullptr;

//resolver pra carregar as operacoes usadas no modelo
//se mudar a arquitetura no python tem que atualizar aqui o numero de ops
static tflite::MicroMutableOpResolver<4> resolver;

int tflm_init_model(void) {
    //carrega o modelo do array de bytes
    model = tflite::GetModel(motor_model);
    if (model == nullptr) {
        MicroPrintf("Erro: model ta nulo");
        return -1;
    }

    //registrando ops necessarias (dense, relu, softmax, reshape)
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddSoftmax();
    resolver.AddReshape(); 

    //instancia o interpretador estatico
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize
    );
    interpreter = &static_interpreter;

    //aloca memoria pros tensores
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        MicroPrintf("Erro no AllocateTensors, checar kTensorArenaSize");
        return -2;
    }

    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    //validacao basica
    if (!input_tensor || !output_tensor) {
        MicroPrintf("Nao conseguiu pegar tensores de in/out");
        return -3;
    }

    MicroPrintf("TFLM iniciado. In dims: %d, Out dims: %d", input_tensor->dims->size, output_tensor->dims->size);
    return 0;
}

int tflm_infer(const float in_features[6], float out_scores[4]) {
    if (!interpreter) return -1; //seguranca

    float normalized[6];
    
    //aplica a normalizacao (standard scaler) igual foi feito no python
    //formula: (valor - media) / desvio
    for (int i = 0; i < 6; i++) {
        normalized[i] = (in_features[i] - scaler_mean[i]) / scaler_scale[i];
    }

    //debug pra ver se a normalizacao ta batendo
    //MicroPrintf("Input norm: %.2f %.2f %.2f...", normalized[0], normalized[1], normalized[2]);

    //copia pro tensor de entrada
    for (int i = 0; i < 6; i++) {
        input_tensor->data.f[i] = normalized[i];
    }

    //roda a inferencia
    if (interpreter->Invoke() != kTfLiteOk) {
        MicroPrintf("Erro ao rodar Invoke");
        return -2;
    }

    //pega o resultado (probabilidades das 4 classes)
    for (int i = 0; i < 4; i++) {
        out_scores[i] = output_tensor->data.f[i];
    }

    return 0;
}