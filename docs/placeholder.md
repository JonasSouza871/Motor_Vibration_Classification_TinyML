# Firmware para BitDogLab (RP2040)

## Descrição

Esta pasta contém o código C++ para executar inferência TinyML na placa BitDogLab com o modelo de classificação de velocidade do motor.

## Estrutura

```
firmware/
├── include/                  # Headers
│   ├── motor_model.h        # Array C do modelo TFLite (gerado pelo notebook)
│   └── scaler_params.h      # Parâmetros de normalização (gerado pelo notebook)
│
├── src/                     # Código fonte
│   └── main.cpp            # Código principal (a ser criado)
│
├── CMakeLists.txt          # Configuração do CMake (a ser criado)
└── README.md               # Este arquivo
```

## Arquivos Gerados pelo Notebook

Após executar o notebook Jupyter, os seguintes arquivos serão automaticamente gerados nesta pasta:

### `include/motor_model.h`
- Contém o modelo TFLite convertido em array C
- Tamanho aproximado: 5-10 KB
- Variável: `motor_model[]`

### `include/scaler_params.h`
- Contém os parâmetros de normalização (média e desvio padrão)
- Necessário para normalizar os dados antes da inferência
- Variáveis: `scaler_mean[]` e `scaler_scale[]`

## Próximos Passos

1. **Executar o notebook**: Gere os arquivos `.h` necessários
2. **Criar `CMakeLists.txt`**: Configurar o projeto para compilação
3. **Criar `src/main.cpp`**: Implementar o código de inferência
4. **Compilar**: Usar CMake para gerar o firmware
5. **Flash**: Gravar o `.uf2` na BitDogLab

## Funcionalidades do Firmware (Planejado)

- Leitura contínua do sensor MPU6050
- Normalização dos dados usando `scaler_params.h`
- Inferência usando TensorFlow Lite Micro
- Exibição do resultado no display/serial
- LED indicando a classe predita

## Dependências

- Pico SDK
- TensorFlow Lite Micro
- Biblioteca MPU6050 para RP2040

## Status

⚠️ **Em desenvolvimento** - Código C++ será implementado em breve.
