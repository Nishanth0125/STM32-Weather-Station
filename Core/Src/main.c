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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BMP280_ADDR     (0x76 << 1)  // SDO → GND
#define BMP280_REG_ID   0xD0
#define BMP280_REG_CTRL 0xF4
#define BMP280_REG_CONF 0xF5
#define BMP280_REG_PRES 0xF7
#define BMP280_REG_TEMP 0xFA
#define BMP280_REG_CAL  0x88
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
float temp     = 0.0f;
float pressure = 0.0f;
volatile uint8_t display_mode = 0;  // 0 = BMP screen, 1 = DHT screen

// Calibration values

uint16_t dig_T1;
int16_t  dig_T2, dig_T3;
uint16_t dig_P1;
int16_t  dig_P2, dig_P3, dig_P4;
int16_t  dig_P5, dig_P6, dig_P7;
int16_t  dig_P8, dig_P9;
int32_t  t_fine;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include "stdio.h"
#include "string.h"
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

// Write one register
void BMP280_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    HAL_I2C_Master_Transmit(&hi2c1, BMP280_ADDR, buf, 2, HAL_MAX_DELAY);
}

// Read multiple registers
void BMP280_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len)
{
    HAL_I2C_Master_Transmit(&hi2c1, BMP280_ADDR, &reg, 1, HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(&hi2c1, BMP280_ADDR, data, len, HAL_MAX_DELAY);
}

// Read calibration data from BMP280
void BMP280_ReadCalibration(void)
{
    uint8_t cal[24];
    BMP280_ReadRegs(BMP280_REG_CAL, cal, 24);

    dig_T1 = (cal[1]  << 8) | cal[0];
    dig_T2 = (cal[3]  << 8) | cal[2];
    dig_T3 = (cal[5]  << 8) | cal[4];
    dig_P1 = (cal[7]  << 8) | cal[6];
    dig_P2 = (cal[9]  << 8) | cal[8];
    dig_P3 = (cal[11] << 8) | cal[10];
    dig_P4 = (cal[13] << 8) | cal[12];
    dig_P5 = (cal[15] << 8) | cal[14];
    dig_P6 = (cal[17] << 8) | cal[16];
    dig_P7 = (cal[19] << 8) | cal[18];
    dig_P8 = (cal[21] << 8) | cal[20];
    dig_P9 = (cal[23] << 8) | cal[22];
}

// Compensate raw temperature (from BMP280 datasheet)
float BMP280_CompensateTemp(int32_t adc_T)
{
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1)))
            * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1))
            * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
            * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (float)((t_fine * 5 + 128) >> 8) / 100.0f;
}

// Compensate raw pressure (from BMP280 datasheet)
float BMP280_CompensatePressure(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8)
         + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1))
         * ((int64_t)dig_P1) >> 33;
    if(var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (float)p / 25600.0f;
}

// Initialize BMP280
uint8_t BMP280_Init(void)
{
    uint8_t id;

    // Check chip ID
    BMP280_ReadRegs(BMP280_REG_ID, &id, 1);
    if(id != 0x58 && id != 0x60)
    {
        printf("BMP280 not found! ID: 0x%02X\r\n", id);
        return 0;
    }
    printf("BMP280 found! ID: 0x%02X\r\n", id);

    // Read calibration
    BMP280_ReadCalibration();

    // Set normal mode, temp x2, pressure x16
    // 0xF4: osrs_t=010, osrs_p=101, mode=11
    BMP280_WriteReg(BMP280_REG_CTRL, 0x57);

    // Set filter x4, standby 250ms
    BMP280_WriteReg(BMP280_REG_CONF, 0x10);

    return 1;
}

// Read temperature and pressure
void BMP280_Read(float *temp, float *pressure)
{
    uint8_t data[6];
    int32_t adc_P, adc_T;

    // Read 6 bytes starting from 0xF7
    // [F7,F8,F9] = pressure, [FA,FB,FC] = temperature
    BMP280_ReadRegs(BMP280_REG_PRES, data, 6);

    adc_P = ((int32_t)data[0] << 12)
          | ((int32_t)data[1] << 4)
          | ((int32_t)data[2] >> 4);

    adc_T = ((int32_t)data[3] << 12)
          | ((int32_t)data[4] << 4)
          | ((int32_t)data[5] >> 4);

        *temp     = BMP280_CompensateTemp(adc_T);
        *pressure = BMP280_CompensatePressure(adc_P);
}
// Requires you to start the timer in main() first!
void delay_us (uint16_t us) {
    __HAL_TIM_SET_COUNTER(&htim6, 0);  // Reset timer counter
    while ((__HAL_TIM_GET_COUNTER(&htim6)) < us);
}
void Set_Pin_Output (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}
uint8_t Presence = 0;
uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2;
uint16_t SUM, RH, TEMP;

void DHT11_Start(void) {
    Set_Pin_Output(GPIOA, GPIO_PIN_1);  // Set pin to output
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 0);   // Pull pin low
    HAL_Delay(18);                             // Wait for 18ms
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, 1);   // Pull pin high
    delay_us(20);                              // Wait for 20us
    Set_Pin_Input(GPIOA, GPIO_PIN_1);   // Set pin as input
}

uint8_t DHT11_Check_Response(void)
{
    uint8_t Response = 0;
    uint32_t timeout;

    delay_us(40);
    if(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
    {
        delay_us(80);
        if((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1))) Response = 1;
        else Response = -1;
    }

    // Timeout protection
    timeout = HAL_GetTick();
    while((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
    {
        if(HAL_GetTick() - timeout > 100) return -1;  // 100ms timeout
    }
    return Response;
}

uint8_t DHT11_Read_Data(void)
{
    uint8_t i = 0, j;
    uint32_t timeout;

    for(j = 0; j < 8; j++)
    {
        // Timeout on waiting for HIGH
        timeout = HAL_GetTick();
        while(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
        {
            if(HAL_GetTick() - timeout > 100) return 0;
        }

        delay_us(40);

        if(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
        {
            i &= ~(1 << (7 - j));
        }
        else
        {
            i |= (1 << (7 - j));

            // Timeout on waiting for LOW
            timeout = HAL_GetTick();
            while((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)))
            {
                if(HAL_GetTick() - timeout > 100) return 0;
            }
        }
    }
    return i;
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
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  ssd1306_Init();
  HAL_TIM_Base_Start(&htim6);
  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE BEGIN 2 */
  if(!BMP280_Init())
  {
      Error_Handler();
  }
  ssd1306_Fill(Black);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  // Read DHT11
	      DHT11_Start();
	      Presence = DHT11_Check_Response();
	      if(Presence == 1)
	      {
	          Rh_byte1   = DHT11_Read_Data();
	          Rh_byte2   = DHT11_Read_Data();
	          Temp_byte1 = DHT11_Read_Data();
	          Temp_byte2 = DHT11_Read_Data();
	          SUM        = DHT11_Read_Data();

	          if(SUM == (Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2))
	          {
	              TEMP = Temp_byte1;
	              RH   = Rh_byte1;
	          }
	      }

	      // Read BMP280
	      BMP280_Read(&temp, &pressure);

	      // Update OLED based on display_mode
	      char buffer[64], buffer1[64], buffer2[64];
	      ssd1306_Fill(Black);

	      if(display_mode == 0)
	      {
	          // Screen 1 — BMP280 data
	          sprintf(buffer,  "BMP280 Sensor");
	          sprintf(buffer1, "Temp: %.1f C",  temp);
	          sprintf(buffer2, "Pres: %.0f hPa", pressure);

	          ssd1306_SetCursor(0, 0);
	          ssd1306_WriteString(buffer,  Font_7x10, White);
	          ssd1306_SetCursor(0, 15);
	          ssd1306_WriteString(buffer1, Font_7x10, White);
	          ssd1306_SetCursor(0, 30);
	          ssd1306_WriteString(buffer2, Font_7x10, White);
	          ssd1306_SetCursor(0, 50);
	          ssd1306_WriteString("BTN: DHT11 >>", Font_6x8, White);
	      }
	      else
	      {
	          // Screen 2 — DHT11 data
	          sprintf(buffer,  "DHT11 Sensor");
	          sprintf(buffer1, "Temp: %d C",  TEMP);
	          sprintf(buffer2, "Humi: %d %%", RH);

	          ssd1306_SetCursor(0, 0);
	          ssd1306_WriteString(buffer,  Font_7x10, White);
	          ssd1306_SetCursor(0, 15);
	          ssd1306_WriteString(buffer1, Font_7x10, White);
	          ssd1306_SetCursor(0, 30);
	          ssd1306_WriteString(buffer2, Font_7x10, White);
	          ssd1306_SetCursor(0, 50);
	          ssd1306_WriteString("<< BTN: BMP280", Font_6x8, White);
	      }

	      ssd1306_UpdateScreen();

	      // UART debug
	      printf("Mode: %d | BMP:%.1fC %.0fhPa | DHT:%dC %d%%\r\n",
	              display_mode, temp, pressure, TEMP, RH);

	      //HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
	      HAL_Delay(1000);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_13)
    {
        // Debounce — ignore if pressed < 200ms ago
        static uint32_t last_press = 0;
        if(HAL_GetTick() - last_press > 200)
        {
            display_mode ^= 1;  // toggle between 0 and 1
            last_press = HAL_GetTick();
        }
    }
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM2)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  // LED toggles every 500ms
    }
}

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
