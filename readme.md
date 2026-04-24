# Jogo de Duelo de Batimentos Cardíacos (STM32)

Este projeto implementa um jogo competitivo para dois jogadores baseado na frequência cardíaca (BPM). O objetivo é manter o BPM mais alto que o do adversário para mover um servo motor em sua direção até o fim do tempo.

## 🚀 Funcionalidades

- Leitura Dupla de Sensores: Utiliza dois sensores MAX30100 via multiplexador I2C para medir o BPM de dois jogadores simultaneamente.

- Feedback em Tempo Real: Um Servo Motor atua como um "cabo de guerra" físico, movendo-se para a esquerda ou direita dependendo de quem está com o batimento mais alto.

- Sistema de Estados: Gerencia o fluxo do jogo desde a inicialização, contagem de tempo (30 segundos por partida) até a declaração do vencedor.

- Comunicação Serial: Envia dados de BPM e status do jogo via UART para depuração e monitoramento.

## 🛠️ Hardware Utilizado

- Microcontrolador: STM32F3xx (Ex: STM32F303).

- Sensores de Oximetria: 2x MAX30100.

- Multiplexador I2C: Para alternar a leitura entre os dois sensores (endereços iguais).

- Atuador: 1x Servo Motor (Controlado por PWM no TIM3).

- Input: Botão de usuário para iniciar a partida.

## ⚙️ Estrutura de Estados

O software opera baseado na seguinte lógica:

- AWAIT_INICIALIZATION: Aguarda o clique no botão para começar.

- RESET_INPUTS_AND_OUTPUTS: Centraliza o servo e reseta as variáveis de jogo.

- RUN_GAME: Lê os sensores a cada 10ms, calcula o BPM e move o servo proporcionalmente.

- GAME_FINISHED: O tempo acaba, o servo aponta para o vencedor e aguarda reset.

## 💻 Como Rodar

- Clone este repositório em seu workspace do STM32CubeIDE.

- Certifique-se de que as bibliotecas HAL para I2C, UART, TIM e GPIO estejam configuradas.

- Compile e faça o flash para a placa STM32.

- Abra um terminal serial (Baud rate configurado na UART2) para ver os logs de BPM.

Nota: Este projeto utiliza interrupções de Timer (TIM1) para garantir a precisão da amostragem dos sensores de batimento cardíaco.