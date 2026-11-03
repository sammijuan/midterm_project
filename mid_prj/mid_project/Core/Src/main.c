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
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define RX_BUF_SIZE    128
#define LINE_BUF_SIZE  32

#define MORSE_UNIT     200

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* UART 每次中斷收到的一個字元 */
uint8_t rx_byte;

/* Ring Buffer */
volatile uint8_t rx_buf[RX_BUF_SIZE];
volatile uint16_t rx_head = 0;
volatile uint16_t rx_tail = 0;

/* 完整單字暫存 */
char line_buf[LINE_BUF_SIZE];
uint8_t line_index = 0;

/* 儲存目前播放的單字，給 Replay 使用 */
char current_word[LINE_BUF_SIZE] = "";

/* B1 要求重播 */
volatile uint8_t replay_request = 0;

/* 去彈跳用 */
volatile uint32_t last_button_time = 0;


/* A ~ Z 摩斯碼表 */
const char *morse_table[26] =
{
    ".-",      // A
    "-...",    // B
    "-.-.",    // C
    "-..",     // D
    ".",       // E
    "..-.",    // F
    "--.",     // G
    "....",    // H
    "..",      // I
    ".---",    // J
    "-.-",     // K
    ".-..",    // L
    "--",      // M
    "-.",      // N
    "---",     // O
    ".--.",    // P
    "--.-",    // Q
    ".-.",     // R
    "...",     // S
    "-",       // T
    "..-",     // U
    "...-",    // V
    ".--",     // W
    "-..-",    // X
    "-.--",    // Y
    "--.."     // Z
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

uint8_t UART_ReadByte(uint8_t *data);

void Morse_PlaySymbol(char symbol);
void Morse_PlayWord(const char *word);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*=========================================================
 * 播放一個摩斯碼符號
 *
 * . = 1 unit
 * - = 3 units
 *=========================================================*/
void Morse_PlaySymbol(char symbol)
{
    /* LED ON */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_5,
        GPIO_PIN_SET
    );

    /* 點 */
    if (symbol == '.')
    {
        HAL_Delay(MORSE_UNIT);
    }

    /* 劃 */
    else if (symbol == '-')
    {
        HAL_Delay(MORSE_UNIT * 3);
    }

    /* LED OFF */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_5,
        GPIO_PIN_RESET
    );

    /*
     * 同一字母中：
     * 點與劃之間間隔 1 unit
     */
    HAL_Delay(MORSE_UNIT);
}

/*=========================================================
 * 播放一個完整英文單字
 *=========================================================*/
void Morse_PlayWord(const char *word)
{
    uint16_t i;

    for (i = 0; word[i] != '\0'; i++)
    {
        char c = word[i];

        /* 小寫 → 大寫 */
        if (c >= 'a' && c <= 'z')
        {
            c = c - 'a' + 'A';
        }

        /* 只處理 A ~ Z */
        if (c >= 'A' && c <= 'Z')
        {
            const char *code;

            /* A = 0, B = 1 ... Z = 25 */
            code = morse_table[c - 'A'];


            /* 播放目前字母 */
            for (uint8_t j = 0;
                 code[j] != '\0';
                 j++)
            {
                Morse_PlaySymbol(code[j]);
            }


            /*
             * Morse_PlaySymbol 最後
             * 已經等待 1 unit
             *
             * 字母間隔總共要 3 unit
             *
             * 所以再補 2 unit
             */
            HAL_Delay(MORSE_UNIT * 2);
        }
    }
}

/*=========================================================
 * 從 Ring Buffer 取出一個字元
 *
 * return 0：沒有資料
 * return 1：成功讀到一個字元
 *=========================================================*/
uint8_t UART_ReadByte(uint8_t *data)
{
    /* head == tail 代表 Buffer 是空的 */
    if (rx_head == rx_tail)
    {
        return 0;
    }

    /* 取出 tail 指向的資料 */
    *data = rx_buf[rx_tail];

    /* tail 往下一格 */
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;

    return 1;
}


/*=========================================================
 * UART 接收完成 Callback
 *
 * USART2 每收到 1 byte 就會進來一次
 * ISR 裡只做：
 *
 * 1. 把資料放進 Ring Buffer
 * 2. 更新 head
 * 3. 立刻重新開啟下一次接收
 *
 * 不在 ISR 裡處理字串
 *=========================================================*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uint16_t next_head;

        /* 算下一個 head */
        next_head = (rx_head + 1) % RX_BUF_SIZE;

        /*
         * 如果 next_head == rx_tail
         * 代表 Ring Buffer 已滿
         */
        if (next_head != rx_tail)
        {
            /* 先存資料 */
            rx_buf[rx_head] = rx_byte;

            /* 再公布新的 head */
            rx_head = next_head;
        }

        /*
         * 重新啟動下一個 byte 的 UART 中斷接收
         */
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
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
  /* USER CODE BEGIN 2 */

    /*
     * 啟動 USART2 第一次中斷接收
     *
     * 收到一個 byte 後
     * HAL_UART_RxCpltCallback() 會被呼叫
     */
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);


    /* 開機訊息，方便確認 STM32 UART 有正常工作 */
    const char start_message[] = "STM32 UART READY\r\n";

    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)start_message,
        strlen(start_message),
        HAL_MAX_DELAY
    );

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

    while (1)
    {
        uint8_t data;

        /* =====================================================
         * Replay 處理
         * B1 中斷只把 replay_request 設為 1
         * 真正播放放在主迴圈
         * ===================================================== */
        if (replay_request)
        {
            replay_request = 0;

            /* 確認之前真的有收到單字 */
            if (current_word[0] != '\0')
            {
                const char replay_msg[] = "REPLAY\r\n";

                HAL_UART_Transmit(
                    &huart2,
                    (uint8_t *)replay_msg,
                    strlen(replay_msg),
                    HAL_MAX_DELAY
                );

                /* 重播上一個單字 */
                Morse_PlayWord(current_word);

                const char done_msg[] = "DONE\r\n";

                HAL_UART_Transmit(
                    &huart2,
                    (uint8_t *)done_msg,
                    strlen(done_msg),
                    HAL_MAX_DELAY
                );
            }
        }


        /* =====================================================
         * UART Ring Buffer
         * 有資料就一次拿一個 byte 出來
         * ===================================================== */
        if (UART_ReadByte(&data))
        {
            /*
             * Python 送：
             *
             * SOS\n
             *
             * \n 代表一整個單字結束
             */
            if (data == '\n')
            {
                /* C 字串結尾 */
                line_buf[line_index] = '\0';


                /* 不處理空字串 */
                if (line_index > 0)
                {
                    char message[64];

                    /* =========================================
                     * 回傳收到什麼
                     * ========================================= */
                    snprintf(
                        message,
                        sizeof(message),
                        "RX: %s\r\n",
                        line_buf
                    );

                    HAL_UART_Transmit(
                        &huart2,
                        (uint8_t *)message,
                        strlen(message),
                        HAL_MAX_DELAY
                    );


                    /* =========================================
                     * 儲存目前單字
                     *
                     * 之後按 B1 Replay 就用這個
                     * ========================================= */
                    strncpy(
                        current_word,
                        line_buf,
                        LINE_BUF_SIZE - 1
                    );

                    current_word[LINE_BUF_SIZE - 1] = '\0';


                    /* =========================================
                     * 告訴電腦開始播放
                     * ========================================= */
                    const char playing_msg[] = "PLAYING\r\n";

                    HAL_UART_Transmit(
                        &huart2,
                        (uint8_t *)playing_msg,
                        strlen(playing_msg),
                        HAL_MAX_DELAY
                    );


                    /* =========================================
                     * 真正播放摩斯碼
                     * ========================================= */
                    Morse_PlayWord(current_word);


                    /* =========================================
                     * 播放完成
                     * ========================================= */
                    const char done_msg[] = "DONE\r\n";

                    HAL_UART_Transmit(
                        &huart2,
                        (uint8_t *)done_msg,
                        strlen(done_msg),
                        HAL_MAX_DELAY
                    );
                }


                /*
                 * 清空輸入位置
                 * 準備接下一個單字
                 */
                line_index = 0;
            }


            /*
             * Windows 有些 terminal 會送 \r\n
             * 所以忽略 \r
             */
            else if (data != '\r')
            {
                /*
                 * 防止 line_buf 超出陣列
                 */
                if (line_index < LINE_BUF_SIZE - 1)
                {
                    line_buf[line_index] = data;
                    line_index++;
                }
            }
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/*=========================================================
 * B1 按鈕中斷
 * PC13
 *=========================================================*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)
    {
        uint32_t now = HAL_GetTick();

        /* 50 ms 去彈跳 */
        if (now - last_button_time >= 50)
        {
            last_button_time = now;

            /* ISR 只記事件 */
            replay_request = 1;
        }
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

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
