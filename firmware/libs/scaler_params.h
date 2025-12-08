// Scaler parameters for normalization
#ifndef SCALER_PARAMS_H
#define SCALER_PARAMS_H

// Mean values
const float scaler_mean[] = {
  -0.991490f,  // Acel_X
  0.659751f,  // Acel_Y
  9.951256f,  // Acel_Z
  0.642380f,  // Giro_X
  1.296435f,  // Giro_Y
  0.326754f  // Giro_Z
};

// Scale (standard deviation) values
const float scaler_scale[] = {
  2.606281f,  // Acel_X
  1.498122f,  // Acel_Y
  0.580618f,  // Acel_Z
  4.727813f,  // Giro_X
  3.477966f,  // Giro_Y
  4.317478f  // Giro_Z
};

#endif // SCALER_PARAMS_H
