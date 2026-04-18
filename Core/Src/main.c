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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct{
  uint16_t buffer_IR[50];
  uint16_t buffer_LED[50];
  uint8_t index;
  uint8_t buffer_full;

  float bpm;
  float spo2;

  uint32_t samples_between_readings;

} Player;

union myConfigMAX30100 {
  uint8_t reg;

  struct {
    uint8_t reserved : 8;
  };
};
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
uint8_t TIME_BETWEEN_READINGS = 10; //em ms
Player player1;
Player player2;
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
  if(MAX30100_Write(0x06, 0x02) != HAL_OK) return;
  if(MAX30100_Write(0x09, 0x77) != HAL_OK) return;
  if(MAX30100_Write(0x07, 0x01) != HAL_OK) return;
}

void MAX30100_Read(){
  uint8_t buffer[4];
  HAL_I2C_Mem_Read(&hi2c1, (0x57 << 1), 0x05, I2C_MEMADD_SIZE_8BIT, buffer, 4, 100);
  // uint16_t ir_value = (buffer[0] << 8) | buffer[1];
  // uint16_t led_value = (buffer[2] << 8) | buffer[3];
  //Falta processar o valor do sensor
}

void prototype_read_BPM(){
  uint8_t buffer[4];
  //Lê valor do IR diretamente do registrador 0x05
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, (0x57 << 1), 0x05, I2C_MEMADD_SIZE_8BIT, buffer, 4, 100);
  
  if(status != HAL_OK) {
    int len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "ERRO AO LER\n");
    HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); // Timeout de 1 segundo em vez de infinito

    return; // Se falhar na leitura, sai da função
  }
  // int len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "LEU \n");
  // HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); // Timeout de 1 segundo em vez de infinito

  
  uint16_t ir_value = (buffer[0] << 8) | buffer[1];
  player1.buffer_IR[player1.index] = ir_value;
  // len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "VALOR: %d \n", ir_value);
  // HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); // Timeout de 1 segundo em vez de infinito

  
  //Somente para simplificar o processo de verificação de variação
  uint8_t past_value;
  uint8_t present_value;
  uint8_t next_value;
  if(player1.index == 0){
    past_value = player1.buffer_IR[48];
    present_value = player1.buffer_IR[49];
    next_value = player1.buffer_IR[0];
  } else if(player1.index == 1){
    past_value = player1.buffer_IR[49];
    present_value = player1.buffer_IR[0];
    next_value = player1.buffer_IR[1];
  } else{
    uint8_t index = player1.index;
    past_value = player1.buffer_IR[index-2];
    present_value = player1.buffer_IR[index-1];
    next_value = player1.buffer_IR[index];
  }

  uint8_t average = 0;

  //Quando o valor tem um ponto de inversão, é considerado um pulso
  if(past_value > present_value && present_value < next_value){
  //Calcula média e verifica se é menor
    for(uint8_t i = 0; i < 49; i++){
          average += player1.buffer_IR[i];
    }
    average = average / 50;
    if(present_value < average){
      if(player1.samples_between_readings > 0) {
        player1.bpm = 60000/(player1.samples_between_readings * TIME_BETWEEN_READINGS);
        int len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "VALOR: %f \n", player1.bpm);
        HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); // Timeout de 1 segundo em vez de infinito

      }
      player1.samples_between_readings = 0;
    }
  }
  player1.samples_between_readings++;
  
  // Incrementa o índice e faz wrap-around
  player1.index++;
  if(player1.index >= 50) {
    player1.index = 0;
    player1.buffer_full = 1;
  }
}

//Verifica a cada 10ms se a leitura está pronta
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        ready_to_read = 1;
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
  /* USER CODE BEGIN 2 */

  // Inicializa estruturas dos jogadores
  player1.index = 0;
  player1.buffer_full = 0;
  player1.bpm = 0.0f;
  player1.spo2 = 0.0f;
  player1.samples_between_readings = 0;
  
  player2.index = 0;
  player2.buffer_full = 0;
  player2.bpm = 0.0f;
  player2.spo2 = 0.0f;
  player2.samples_between_readings = 0;

  if(HAL_TIM_Base_Start_IT(&htim1) != HAL_OK) {
    Error_Handler();
  }

  //Select_I2C_Channel(0);
  MAX30100_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    int len = snprintf((char*)tx_buffer, sizeof(tx_buffer), "BPM:%d\n", player1.bpm);

    if(ready_to_read == 1){
      prototype_read_BPM();
      ready_to_read = 0;
      HAL_UART_Transmit(&huart2, tx_buffer, len, 1000); // Timeout de 1 segundo em vez de infinito
      
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
