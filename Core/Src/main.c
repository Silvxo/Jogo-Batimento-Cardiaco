/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "stm32f3xx.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_tim.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include <stdint.h>
#include "stdlib.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct{
  uint16_t buffer_IR[100];
  uint16_t buffer_LED[100];
  uint8_t index;
  uint8_t detected_pulse;
  uint32_t rolling_avg;

  uint16_t bpm;
  float spo2;

  uint32_t samples_between_readings;

} Player;

union myConfigMAX30100 {
  uint8_t reg;

  struct {
    uint8_t reserved : 8;
  };
};

enum states { 
  AWAIT_INICIALIZATION = 0,
  RESET_INPUTS_AND_OUTPUTS,
  RUN_GAME,
  GAME_FINISHED
};

typedef struct {
  uint8_t RESET;
  uint8_t MINIMUM;
  uint8_t MAXIMUM;
  uint8_t current_position;
} ServoPositions;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
//Indica que a leitura do sensor está pronta
volatile uint8_t ready_to_read = 0;
volatile uint8_t timer_tick = 0;
volatile uint8_t servo_tick = 0;
uint8_t timer = 0;
uint8_t match_duration_seconds = 30;
uint8_t ticks_counter = 0;
uint8_t servo_ticks_counter = 0;

uint8_t button_last_state = GPIO_PIN_RESET;

uint8_t TIME_BETWEEN_READINGS = 10; //em ms
uint16_t threshold = 25000;
uint16_t aboveThreshold = 0;
Player player1;
Player player2;

ServoPositions servoPositions;
uint8_t degrees_per_move = 4;

enum states currentState = AWAIT_INICIALIZATION;

static uint8_t pulse_detected = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Select_I2C_Channel(uint8_t channel);
HAL_StatusTypeDef MAX30100_Write(uint8_t reg, uint8_t data);
void MAX30100_Read();
void add_reading(Player *player);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t tx_buffer[100];

// Função para selecionar canal I2C do multiplexador I2C
void Select_I2C_Channel(uint8_t channel){
  uint8_t data = 1 << channel;
  HAL_I2C_Master_Transmit(&hi2c1, (0x70 << 1), &data, 1, 100);
}

// Função para escrever nos registradores do sensor MAX30100
HAL_StatusTypeDef MAX30100_Write(uint8_t reg, uint8_t data){
  return HAL_I2C_Mem_Write(&hi2c1, (0x57 << 1), reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

void MAX30100_Init(){
  //if(MAX30100_Write(0x06, 0b01001011) != HAL_OK) return;
  MAX30100_Write(0x06, 0x40); 
  HAL_Delay(10);
  if(MAX30100_Write(0x06, 0x03) != HAL_OK) return;
  if(MAX30100_Write(0x09, 0x77) != HAL_OK) return;
  if(MAX30100_Write(0x07, 0x07) != HAL_OK) return;
}


void read_BPM(Player *player){
  uint8_t buffer[4];
  //Lê valor do IR diretamente do registrador 0x05
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, (0x57 << 1), 0x05, I2C_MEMADD_SIZE_8BIT, buffer, 4, 100);
  if(status != HAL_OK) {
    player->bpm = 0;
    return; // Se falhar na leitura, sai da função
  }
  
  uint16_t ir_value = (buffer[0] << 8) | buffer[1];
  player->buffer_IR[player->index] = ir_value;

  if(ir_value < 15000){
    player->bpm = 0;
    return;
  }

  // Filtro passa baixa
  player->rolling_avg = (player->rolling_avg * 0.95) + (ir_value * 0.05);

  if (ir_value < (player->rolling_avg - 150) && !player->detected_pulse) { 
      // Pulso detectado
      uint16_t bpm = 60000 / (player->samples_between_readings * TIME_BETWEEN_READINGS);
      if(bpm > 35 && bpm < 120){
        player->bpm = bpm;
      }
      player->samples_between_readings = 0;
      player->detected_pulse = 1;
  } 

  if (ir_value > player->rolling_avg) {
      player->detected_pulse = 0; // Reseta pro proximo batimento
  }

  player->samples_between_readings++;
  player->index = (player1.index + 1) % 100;
}

void read_BPM_from_players(){
  if(ready_to_read == 1){
    Select_I2C_Channel(0);
    read_BPM(&player1);
    Select_I2C_Channel(1);
    read_BPM(&player2);
    ready_to_read = 0;
  }
}

void initialize_sensors(){
  Select_I2C_Channel(0);
  MAX30100_Init();
  Select_I2C_Channel(1);
  MAX30100_Init();
}

void move_servo(uint8_t angle){
  if(angle > 179) angle = 179;

  uint32_t ccr_val = 50 + ((uint32_t)angle * 50 / 180);

  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, ccr_val);
}

void update_servo_position(uint16_t bpm1, uint16_t bpm2){
  if(servo_tick == 1){
    servo_ticks_counter++;
    servo_tick = 0;
  }
  if(servo_ticks_counter == 100){
    servo_ticks_counter = 0;

    if(bpm1 > bpm2){
      servoPositions.current_position += degrees_per_move;
    } else if(bpm1 < bpm2){
      servoPositions.current_position -= degrees_per_move;
    }
    if(servoPositions.current_position < servoPositions.MINIMUM)
      servoPositions.current_position = servoPositions.MINIMUM;
    if(servoPositions.current_position > servoPositions.MAXIMUM)
      servoPositions.current_position = servoPositions.MAXIMUM;

    int len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "Player 1: %d\n", player1.bpm);
    HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); 
    len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "Player 2: %d\n", player2.bpm);
    HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); 

    len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "SERVO UPDATE\n");
    HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); 

    move_servo(servoPositions.current_position);
  }
}

void servo_display_winner(){
  //Se todos forem 0, o lado esquerdo ganha
  if(servoPositions.current_position >= 90){
    servoPositions.current_position = servoPositions.MAXIMUM;
  } else {
    servoPositions.current_position = servoPositions.MINIMUM;
  }
  move_servo(servoPositions.current_position);
}

void reset_servo(){
  move_servo(servoPositions.RESET);
  servoPositions.current_position = servoPositions.RESET;
}


void set_timer(uint8_t match_duration_seconds){
  timer = match_duration_seconds;
}

void update_timer(){
  if(timer_tick == 1){
    ticks_counter++;
    timer_tick = 0;
  }
  if(ticks_counter >= 100){
    timer--;
    ticks_counter = 0;
  }
}

uint8_t is_match_finished(){
  if(timer == 0){
    return 1;
  }
  return 0;
}

//Verifica a cada 10ms se a leitura está pronta
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        ready_to_read = 1;
        timer_tick = 1;
        servo_tick = 1;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  // Inicializa estruturas dos jogadores
  player1.index = 0;
  player1.bpm = 0.0f;
  player1.spo2 = 0.0f;
  player1.samples_between_readings = 0;
  player1.detected_pulse = 0;
  player1.rolling_avg = 0;
  
  player2.index = 0;
  player2.bpm = 0.0f;
  player2.spo2 = 0.0f;
  player2.samples_between_readings = 0;
  player2.detected_pulse = 0;
  player2.rolling_avg = 0;

  servoPositions.RESET = 89;
  servoPositions.MAXIMUM = 175;
  servoPositions.MINIMUM = 5;


  if(HAL_TIM_Base_Start_IT(&htim1) != HAL_OK) {
    Error_Handler();
  }

  if(HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK){
    Error_Handler();
  }

  initialize_sensors();
  int len = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    switch (currentState) {
      case AWAIT_INICIALIZATION:
        //  Display in waiting screen
        //  Await button input
        if(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == 0){
          len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "AWAIT\n");
          HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); 
          currentState = RESET_INPUTS_AND_OUTPUTS;
        }
        break;
      case RESET_INPUTS_AND_OUTPUTS:
        // Reset servo position
        reset_servo();
        // Wait to detect pulse
        // Display preparation and pulse detection
        // if(player1.has_detected_pulse == 1 && player1.has_detected_pulse == 1) setTimer(); state = RUN_GAME
        set_timer(match_duration_seconds);
        len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "RESET\n");
        HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); 
        currentState = RUN_GAME;
        break;
      case RUN_GAME:
        // 1 - Update timer
        update_timer();
        // 2 - Read BPM from both players
        read_BPM_from_players();
        // 3 - Calculate points and position
        // 4 - Update display and servo
        //update_servo_position(player1.bpm, player2.bpm);
        update_servo_position(player1.bpm, player2.bpm);
        // Update display
        if(is_match_finished() == 1){
          currentState = GAME_FINISHED;
        }
        break;
      case GAME_FINISHED:
          servo_display_winner();
          if(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == 0){
            currentState = AWAIT_INICIALIZATION;
            HAL_Delay(100);
          }

        break;
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_TIM1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
