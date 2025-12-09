#ifndef TFLM_WRAPPER_H_
#define TFLM_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa o modelo TensorFlow Lite Micro
int tflm_init_model(void);

//Executa inferência no modelo de classificação de motor
//in_features: array com 6 features [Accel_X, Accel_Y, Accel_Z, Gyro_X, Gyro_Y, Gyro_Z]
//out_scores: array de saída com 4 probabilidades [Level 0, Level 1, Level 2, Level 3]
int tflm_infer(const float in_features[6], float out_scores[4]);

#ifdef __cplusplus
}
#endif

#endif  //TFLM_WRAPPER_H_
