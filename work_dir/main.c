/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "zlg7290.h"
#include "stdio.h"
#include "LM75A.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
#define ZLG_READ_ADDRESS1 0x01
#define ZLG_READ_ADDRESS2 0x10
#define ZLG_WRITE_ADDRESS1 0x10
#define ZLG_WRITE_ADDRESS2 0x11
#define BUFFER_SIZE1 (countof(Tx1_Buffer))
#define BUFFER_SIZE2 (countof(Rx2_Buffer))
#define countof(a) (sizeof(a) / sizeof(*(a)))

uint8_t flag;      //不同的按键有不同的标志位值
uint8_t flag1 = 0; //中断标志位，每次按键产生一次中断，并开始读取8个数码管的值
uint8_t Rx2_Buffer[8] = {0};
uint8_t Tx1_Buffer[8] = {0};
uint8_t Rx1_Buffer[1] = {0};
double default_threshold = 36.5;
/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void switch_key(void);
void switch_flag(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
uint16_t Temp;
/* USER CODE END 0 */

int main(void) {
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration----------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    printf("\n\r");
    printf("\n\r-------------------------------------------------\r\n");
    printf("\n\r FS-STM32开发板 周立功芯片测试例程\r\n");
    LM75SetMode(CONF_ADDR, NORMOR_MODE);
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    double nowTemp = -1;
    while (1) {
        if ((Temp = LM75GetTempReg()) != EVL_ER) {
            nowTemp = LM75GetTempValue(Temp);
        }
        if (nowTemp > default_threshold) {
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);   // 打开蜂鸣器
            HAL_Delay(2);                                         // 50HZ 频率
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET); // 关闭蜂鸣器
            HAL_Delay(2);                                         // 50HZ 频率
        }

        while (flag1 == 1) {
            double temp = 0;
            I2C_ZLG7290_Read(&hi2c1, 0x71, 0x01, Rx1_Buffer, 1);      // 读键值
            printf("\n\r按键键值 = %#x\r\n", Rx1_Buffer[0]);            // 向串口发送键值
            if (Rx1_Buffer[0] == '#') {
                default_threshold = temp;
                flag1 = 0;
                break;
            } else {
                switch_key();                                        // 扫描键值，写标志位
                I2C_ZLG7290_Read(&hi2c1, 0x71, 0x10, Rx2_Buffer, 8); // 读 8 位数码管
                switch_flag();                                       // 扫描到相应的按键并且向数码管写进数值
                temp = temp * 10 + Rx2_Buffer[0];
            }
        }
    }

    HAL_Delay(1000);
    /* USER CODE END 3 */
}

/** System Clock Configuration
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_ClkInitTypeDef RCC_ClkInitStruct;

    __PWR_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
                                  | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

// void SystemClock_Config(void) {

//     RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
//     RCC_OscInitStruct.HSIState = RCC_HSI_ON;
//     RCC_OscInitStruct.HSICalibrationValue = 16;
//     RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
//     if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
//         Error_Handler();
//     }

//     RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
//                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
//     RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
//     RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
//     RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
//     RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
//     if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
//         Error_Handler();
//     }

// }

/* USER CODE BEGIN 4 */
void switch_key(void) {
    switch (Rx1_Buffer[0]) {
    case 0x1C:
        flag = 1;
        break;
    case 0x1B:
        flag = 2;
        break;
    case 0x1A:
        flag = 3;
        break;
    case 0x14:
        flag = 4;
        break;
    case 0x13:
        flag = 5;
        break;
    case 0x12:
        flag = 6;
        break;
    case 0x0C:
        flag = 7;
        break;
    case 0x0B:
        flag = 8;
        break;
    case 0x0A:
        flag = 9;
        break;
    // 0
    case 0x03:
        flag = 15;
        break;
    // A
    case 0x19:
        flag = 10;
        break;
    // B
    case 0x11:
        flag = 11;
        break;
    // C
    case 0x09:
        flag = 12;
        break;
    // D
    case 0x01:
        flag = 13;
        break;
    // #
    case 0x02:
        flag = 14;
        break;
    default:
        break;
    }
}

void switch_flag(void) {
    switch (flag) {
    case 1:
        Tx1_Buffer[0] = 0x0c;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 2:
        Tx1_Buffer[0] = 0xDA;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 3:
        Tx1_Buffer[0] = 0xF2;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 4:
        Tx1_Buffer[0] = 0x66;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 5:
        Tx1_Buffer[0] = 0xB6;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 6:
        Tx1_Buffer[0] = 0xBE;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 7:
        Tx1_Buffer[0] = 0xE0;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 8:
        Tx1_Buffer[0] = 0xFE;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 9:
        Tx1_Buffer[0] = 0xE6;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 10:
        Tx1_Buffer[0] = 0xEE;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 11:
        Tx1_Buffer[0] = 0x3E;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 12:
        Tx1_Buffer[0] = 0x9C;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 13:
        Tx1_Buffer[0] = 0x7A;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    case 14:
        Tx1_Buffer[0] = 0x00;
        I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 8);
        break;
    case 15:
        Tx1_Buffer[0] = 0xFC;
        if (Rx2_Buffer[0] == 0) {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        } else {
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS2, Rx2_Buffer, BUFFER_SIZE2);
            I2C_ZLG7290_Write(&hi2c1, 0x70, ZLG_WRITE_ADDRESS1, Tx1_Buffer, 1);
        }
        break;
    default:
        break;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    flag1 = 1;
}
int fputc(int ch, FILE* f) {
    uint8_t tmp[1] = {0};
    tmp[0] = (uint8_t)ch;
    HAL_UART_Transmit(&huart1, tmp, 1, 10);
    return ch;
}
/* USER CODE BEGIN 4 */
// int fputc(int ch, FILE* f) {
//     while ((USART1->SR & 0X40) == 0)
//         ;
//     USART1->DR = (uint8_t)ch;
//     return ch;
// }
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler */
    /* User can add his own implementation to report the HAL error return state */
    while (1) {
    }
    /* USER CODE END Error_Handler */
}
/* USER CODE END 4 */

#ifdef USE_FULL_ASSERT

/**
 * @brief Reports the name of the source file and the source line number
 * where the assert_param error has occurred.
 * @param file: pointer to the source file name
 * @param line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
      ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}

#endif

/**
 * @}
 */

/**
 * @}
 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/