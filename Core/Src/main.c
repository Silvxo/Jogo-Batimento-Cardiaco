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
void MAX30100_Write(uint8_t reg, uint8_t data);
void MAX30100_Read();
void add_reading(Player *player);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Função para selecionar canal I2C do multiplexador I2C
void Select_I2C_Channel(uint8_t channel){
  uint8_t data = 1 << channel;
  HAL_I2C_Master_Transmit(&hi2c1, (0x70 << 1), &data, 1, 100);
}

// Função para escrever nos registradores do sensor MAX30100
void MAX30100_Write(uint8_t reg, uint8_t data){
  HAL_I2C_Mem_Write(&hi2c1, (0x57 << 1), reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

void MAX30100_Read(){
  HAL_I2C_Mem_Read(&hi2c1, (0x57 << 1), 0x05, I2C_MEMADD_SIZE_8BIT, buffer, 4, 100);
  uint16_t ir_value = (buffer[0] << 8) | buffer[1];
  uint16_t led_value = (buffer[2] << 8) | buffer[3];
  //Falta processar o valor do sensor
}

void prototype_read_BPM(){
  //Lê valor do IR diretamente do registrador 0x05
  HAL_I2C_Mem_Read(&hi2c1, (0x57 << 1), 0x05, I2C_MEMADD_SIZE_8BIT, buffer, 4, 100);
  uint16_t ir_value = (buffer[0] << 8) | buffer[1];
  Player1->buffer_ir[Player1->index] = ir_value;
  
  //Somente para simplificar o processo de verificação de variação
  uint8_t past_value;
  uint8_t present_value;
  uint8_t next_value;
  if(Player1->index == 0){
    past_value = Player1->buffer_ir[48];
    present_value = Player1->buffer_ir[49];
    next_value = Player1->buffer_ir[0];
  } else if(Player1->index == 1){
    past_value = Player1->buffer_ir[49];
    present_value = Player1->buffer_ir[0];
    next_value = Player1->buffer_ir[1];
  } else{
    uint8_t index = Player1->index;
    past_value = Player1->buffer_ir[index-2];
    present_value = Player1->buffer_ir[index-1];
    next_value = Player1->buffer_ir[index];
  }

  uint8_t average = 0;

  //Quando o valor tem um ponto de inversão, é considerado um pulso
  if(past_value > present_value && present_value < next_value)
  //Calcula média e verifica se é menor
    for(int i - 0; i < 49; i++){
          average += player1->buffer_ir[i];
    }
    average = average / 50;
    if(present_value < average){
      player1->bpm = 60000/(player1->samples_between_readings * TIME_BETWEEN_READINGS);
      player1->samples_between_readings = 0;
  }
  player1->samples_between_readings++;
}

//Verifica a cada 10ms se a leitura está pronta
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM6) {
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

  HAL_TIM_Base_Start_IT(&htim6);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    if(ready_to_read == 1){
      prototype_read_BPM();
      ready_to_read = 0;
    }
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
