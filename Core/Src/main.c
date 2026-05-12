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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

typedef enum {
    STATE_PASS,
    STATE_DOUBLE,
    STATE_BID_LEVEL,
    STATE_BID_SUIT
} BiddingState;

typedef enum {
    CLUBS,
    DIAMONDS,
    HEARTS,
    SPADES,
    NT
} Suit;

typedef enum {
    CALL_PASS,
    CALL_DOUBLE,
    CALL_BID
} CallType;

typedef struct {
    CallType type;
    uint8_t level;
    Suit suit;
} Call;

#define MAX_CALLS 20

volatile Call bidding_history[MAX_CALLS];
volatile uint8_t bidding_count = 0;

volatile BiddingState current_state = STATE_PASS;

volatile uint8_t current_level = 1;
volatile Suit current_suit = CLUBS;

volatile uint8_t double_available = 1;
volatile uint8_t redouble_available = 0;

volatile uint8_t last_bid_level = 0;   // 0 = jeszcze nic nie było
volatile Suit last_bid_suit = CLUBS;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define TFT_DC_PORT GPIOA
#define TFT_DC_PIN  GPIO_PIN_0

#define TFT_RST_PORT GPIOA
#define TFT_RST_PIN  GPIO_PIN_1

#define TFT_CS_PORT GPIOA
#define TFT_CS_PIN  GPIO_PIN_4

void TFT_Select(void)
{
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET);
}

void TFT_Unselect(void)
{
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
}

void TFT_DC_Command(void)
{
    HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_RESET);
}

void TFT_DC_Data(void)
{
    HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET);
}

void TFT_Reset(void)
{
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);
}

void TFT_WriteCommand(uint8_t cmd)
{
    TFT_DC_Command();
    TFT_Select();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    TFT_Unselect();
}

void TFT_WriteData(uint8_t data)
{
    TFT_DC_Data();
    TFT_Select();
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    TFT_Unselect();
}

void TFT_WriteBuffer(uint8_t *buff, uint32_t len)
{
    TFT_DC_Data();
    TFT_Select();
    HAL_SPI_Transmit(&hspi1, buff, len, HAL_MAX_DELAY);
    TFT_Unselect();
}

void ILI9488_Init_Minimal(void)
{
    TFT_Reset();

    TFT_WriteCommand(0x01);
    HAL_Delay(120);

    TFT_WriteCommand(0x11);
    HAL_Delay(120);

    TFT_WriteCommand(0x3A);
    TFT_WriteData(0x66);    // 18-bit

    TFT_WriteCommand(0x36);
    TFT_WriteData(0x48);

    TFT_WriteCommand(0x29);
    HAL_Delay(20);
}

void ILI9488_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    TFT_WriteCommand(0x2A);
    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;
    TFT_WriteBuffer(data, 4);

    TFT_WriteCommand(0x2B);
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    TFT_WriteBuffer(data, 4);

    TFT_WriteCommand(0x2C);
}

void ILI9488_FillScreen(uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_SetAddressWindow(0, 0, 319, 479);

    TFT_DC_Data();
    TFT_Select();

    uint8_t pixel[3] = {r, g, b};

    for (uint32_t i = 0; i < 320UL * 480UL; i++)
    {
        HAL_SPI_Transmit(&hspi1, pixel, 3, HAL_MAX_DELAY);
    }

    TFT_Unselect();
}

void ILI9488_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= 320 || y >= 480) return;
    if ((x + w) > 320) w = 320 - x;
    if ((y + h) > 480) h = 480 - y;

    ILI9488_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    TFT_DC_Data();
    TFT_Select();

    uint8_t pixel[3] = {r, g, b};

    for (uint32_t i = 0; i < (uint32_t)w * h; i++)
    {
        HAL_SPI_Transmit(&hspi1, pixel, 3, HAL_MAX_DELAY);
    }

    TFT_Unselect();
}

/* ===== 7-segment digits ===== */

void DrawSegmentA(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_FillRect(x + 10, y, 40, 10, r, g, b);
}

void DrawSegmentB(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_FillRect(x + 50, y + 10, 10, 40, r, g, b);
}

void DrawSegmentC(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_FillRect(x + 50, y + 60, 10, 40, r, g, b);
}

void DrawSegmentD(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_FillRect(x + 10, y + 100, 40, 10, r, g, b);
}

void DrawSegmentE(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_FillRect(x, y + 60, 10, 40, r, g, b);
}

void DrawSegmentF(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_FillRect(x, y + 10, 10, 40, r, g, b);
}

void DrawSegmentG(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
    ILI9488_FillRect(x + 10, y + 50, 40, 10, r, g, b);
}


void SetNextBidAfterLast(void)
{
    if (last_bid_level == 0)
    {
        current_level = 1;
        current_suit = CLUBS;
        return;
    }

    current_level = last_bid_level;
    current_suit = (Suit)(last_bid_suit + 1);

    if (current_suit > NT)
    {
        current_suit = CLUBS;
        current_level++;

        if (current_level > 7)
        {
            current_level = 7;
            current_suit = NT;
        }
    }
}

void HandleNext(void)
{
    switch(current_state)
    {
        case STATE_PASS:
            if(double_available)
                current_state = STATE_DOUBLE;
            else
                current_state = STATE_BID_LEVEL;
            break;

        case STATE_DOUBLE:
            current_state = STATE_BID_LEVEL;
            break;

        case STATE_BID_LEVEL:
            current_level++;
            if(current_level > 7)
                current_level = 1;
            break;

        case STATE_BID_SUIT:
            current_suit = (Suit)(current_suit + 1);
            if(current_suit > NT)
                current_suit = CLUBS;
            break;
    }
}

void HandleBack(void)
{
    switch(current_state)
    {
        case STATE_PASS:
            break;

        case STATE_DOUBLE:
            current_state = STATE_PASS;
            break;

        case STATE_BID_LEVEL:
            if(double_available)
                current_state = STATE_DOUBLE;
            else
                current_state = STATE_PASS;
            break;

        case STATE_BID_SUIT:
            current_state = STATE_BID_LEVEL;
            break;
    }
}

void HandleOK(void)
{
    switch(current_state)
    {
        case STATE_PASS:
            AddCallToHistory(CALL_PASS, 0, CLUBS);
            current_state = STATE_PASS;
            break;

        case STATE_DOUBLE:
            AddCallToHistory(CALL_DOUBLE, 0, CLUBS);
            current_state = STATE_PASS;
            break;

        case STATE_BID_LEVEL:
            current_state = STATE_BID_SUIT;
            break;

        case STATE_BID_SUIT:
            AddCallToHistory(CALL_BID, current_level, current_suit);

            last_bid_level = current_level;
            last_bid_suit = current_suit;

            SetNextBidAfterLast();
            current_state = STATE_PASS;
            break;
    }
}

void DrawSmallPass(uint16_t x, uint16_t y)
{
    // niebieskie P
    ILI9488_FillRect(x, y, 4, 20, 0, 0, 255);
    ILI9488_FillRect(x + 4, y, 8, 4, 0, 0, 255);
    ILI9488_FillRect(x + 12, y + 4, 4, 6, 0, 0, 255);
    ILI9488_FillRect(x + 4, y + 10, 8, 4, 0, 0, 255);
}
void DrawBigPass(uint16_t x, uint16_t y)
{
	ClearColorTopPanel();
	ClearNumberTopPanel();
    // białe P
    ILI9488_FillRect(x, y, 16, 80, 255, 255, 255);
    ILI9488_FillRect(x + 16, y, 32, 16, 255, 255, 255);
    ILI9488_FillRect(x + 48, y + 16, 16, 24, 255, 255, 255);
    ILI9488_FillRect(x + 16, y + 40, 32, 16, 255, 255, 255);
}

void DrawSmallX(uint16_t x, uint16_t y)
{
    // czerwone X
    for (int i = 0; i < 12; i++)
    {
        ILI9488_FillRect(x + i, y + i, 2, 2, 255, 0, 0);
        ILI9488_FillRect(x + 12 - i, y + i, 2, 2, 255, 0, 0);
    }
}

void DrawTinyDigit(uint16_t x, uint16_t y, uint8_t digit)
{
    // bardzo mała wersja 7-seg
    uint8_t t = 3;
    uint8_t w = 12;
    uint8_t h = 20;

    if (digit == 0 || digit > 7) return;

    #define SA() ILI9488_FillRect(x + t, y, w, t, 255,255,255)
    #define SB() ILI9488_FillRect(x + w + t, y + t, t, h/2, 255,255,255)
    #define SC() ILI9488_FillRect(x + w + t, y + h/2 + t, t, h/2, 255,255,255)
    #define SD() ILI9488_FillRect(x + t, y + h + t, w, t, 255,255,255)
    #define SE() ILI9488_FillRect(x, y + h/2 + t, t, h/2, 255,255,255)
    #define SF() ILI9488_FillRect(x, y + t, t, h/2, 255,255,255)
    #define SG() ILI9488_FillRect(x + t, y + h/2, w, t, 255,255,255)

    switch(digit)
    {
        case 1: SB(); SC(); break;
        case 2: SA(); SB(); SG(); SE(); SD(); break;
        case 3: SA(); SB(); SG(); SC(); SD(); break;
        case 4: SF(); SG(); SB(); SC(); break;
        case 5: SA(); SF(); SG(); SC(); SD(); break;
        case 6: SA(); SF(); SG(); SE(); SC(); SD(); break;
        case 7: SA(); SB(); SC(); break;
    }

    #undef SA
    #undef SB
    #undef SC
    #undef SD
    #undef SE
    #undef SF
    #undef SG
}

void DrawSmallSuit(uint16_t x, uint16_t y, Suit suit)
{
    switch(suit)
    {
        case CLUBS:
            ILI9488_FillRect(x , y + 8, 16, 3, 0, 255, 0);
            ILI9488_FillRect(x + 4, y + 14, 8, 1, 0, 255, 0);
            ILI9488_FillRect(x + 5, y + 13, 6, 1, 0, 255, 0);
            ILI9488_FillRect(x + 6, y + 12, 4, 1, 0, 255, 0);
            ILI9488_FillRect(x + 7, y + 11, 2, 1, 0, 255, 0);
            ILI9488_FillRect(x + 7, y + 7, 2, 1, 0, 255, 0);
            ILI9488_FillRect(x + 6, y + 6, 4, 1, 0, 255, 0);
            ILI9488_FillRect(x + 5, y + 2, 6, 1, 0, 255, 0);
            ILI9488_FillRect(x + 5, y + 1, 6, 1, 0, 255, 0);
            ILI9488_FillRect(x + 6, y, 4, 1, 0, 255, 0);
            ILI9488_FillRect(x + 4, y + 3, 8, 3, 0, 255, 0);

            ILI9488_FillRect(x + 1, y + 7, 5, 1, 0, 255, 0);
            ILI9488_FillRect(x + 2, y + 6, 3, 1, 0, 255, 0);
            ILI9488_FillRect(x + 1, y + 11, 5, 1, 0, 255, 0);
            ILI9488_FillRect(x + 2, y + 12, 3, 1, 0, 255, 0);

            ILI9488_FillRect(x + 10, y + 7, 5, 1, 0, 255, 0);
            ILI9488_FillRect(x + 11, y + 6, 3, 1, 0, 255, 0);
            ILI9488_FillRect(x + 10, y + 11, 5, 1, 0, 255, 0);
            ILI9488_FillRect(x + 11, y + 12, 3, 1, 0, 255, 0);

            break;

        case DIAMONDS:
            ILI9488_FillRect(x + 4, y, 4, 4, 255, 170, 0);
            ILI9488_FillRect(x + 2, y + 4, 8, 8, 255, 170, 0);
            ILI9488_FillRect(x + 4, y + 12, 4, 4, 255, 170, 0);
            break;

        case HEARTS:
            ILI9488_FillRect(x + 2, y, 4, 4, 255, 0, 0);
            ILI9488_FillRect(x + 6, y, 4, 4, 255, 0, 0);
            ILI9488_FillRect(x, y + 4, 12, 6, 255, 0, 0);
            ILI9488_FillRect(x + 2, y + 10, 8, 4, 255, 0, 0);
            break;

        case SPADES:
            ILI9488_FillRect(x + 4, y, 4, 4, 0, 0, 255);
            ILI9488_FillRect(x, y + 4, 12, 6, 0, 0, 255);
            ILI9488_FillRect(x + 4, y + 10, 4, 6, 0, 0, 255);
            break;

        case NT:
            ILI9488_FillRect(x, y, 4, 16, 255, 255, 255);
            ILI9488_FillRect(x + 8, y, 4, 16, 255, 255, 255);
            ILI9488_FillRect(x + 4, y + 6, 4, 4, 255, 255, 255);
            break;
    }
}

void DrawBigSuit(uint16_t x, uint16_t y, Suit suit)
{
    switch(suit)
    {
        case CLUBS:
            ILI9488_FillRect(x , y + 32, 64, 12, 0, 255, 0);
            ILI9488_FillRect(x + 16, y + 56, 32, 4, 0, 255, 0);
            ILI9488_FillRect(x + 20, y + 52, 24, 4, 0, 255, 0);
            ILI9488_FillRect(x + 24, y + 48, 16, 4, 0, 255, 0);
            ILI9488_FillRect(x + 28, y + 44, 8, 4, 0, 255, 0);
            ILI9488_FillRect(x + 28, y + 28, 8, 4, 0, 255, 0);
            ILI9488_FillRect(x + 24, y + 24, 16, 4, 0, 255, 0);
            ILI9488_FillRect(x + 20, y + 20, 24, 4, 0, 255, 0);
            ILI9488_FillRect(x + 20, y + 8, 24, 4, 0, 255, 0);
            ILI9488_FillRect(x + 20, y + 4, 24, 4, 0, 255, 0);
            ILI9488_FillRect(x + 24, y, 16, 4, 0, 255, 0);
            ILI9488_FillRect(x + 16, y + 8, 32, 12, 0, 255, 0);

            ILI9488_FillRect(x + 4, y + 28, 20, 4, 0, 255, 0);
            ILI9488_FillRect(x + 8, y + 24, 12, 4, 0, 255, 0);
            ILI9488_FillRect(x + 4, y + 44, 20, 4, 0, 255, 0);
            ILI9488_FillRect(x + 8, y + 48, 12, 4, 0, 255, 0);

            ILI9488_FillRect(x + 40, y + 28, 20, 4, 0, 255, 0);
            ILI9488_FillRect(x + 44, y + 24, 12, 4, 0, 255, 0);
            ILI9488_FillRect(x + 40, y + 44, 20, 4, 0, 255, 0);
            ILI9488_FillRect(x + 44, y + 48, 12, 4, 0, 255, 0);

            break;

        case DIAMONDS:

        	for (int i = 0; i < 41; i++)
        	    {
        	        ILI9488_FillRect(x - i/2, y + i, 2+i, 1, 255, 170, 0);
        	        ILI9488_FillRect(x - i/2 , y + 80 - i, 2+i, 1, 255, 170, 0);
        	    }

            break;

        case HEARTS:

        	for (int i = 0; i < 41; i++)
        	       {
        	        ILI9488_FillRect(x - i/2, y + 80 - i, 2+i, 1, 255, 0, 0);

        	       }

            break;

        case SPADES:
        	for (int i = 0; i < 61; i++)
        	        {
        	        ILI9488_FillRect(x - i/2, y + i, 2+i, 1, 0, 0, 255);

        	        }
            break;

        case NT:
            ILI9488_FillRect(x, y, 4, 16, 255, 255, 255);
            ILI9488_FillRect(x + 8, y, 4, 16, 255, 255, 255);
            ILI9488_FillRect(x + 4, y + 6, 4, 4, 255, 255, 255);
            break;
    }
}

void AddCallToHistory(CallType type, uint8_t level, Suit suit)
{
    if (bidding_count >= MAX_CALLS) return;

    bidding_history[bidding_count].type = type;
    bidding_history[bidding_count].level = level;
    bidding_history[bidding_count].suit = suit;
    bidding_count++;
}

void DrawHistory(void)
{
    // czyścimy dolną połowę
    ILI9488_FillRect(0, 240, 320, 240, 0, 0, 0);

    for (uint8_t i = 0; i < bidding_count; i++)
    {
        uint8_t col = i % 4;
        uint8_t row = i / 4;

        if (row >= 5) break;

        uint16_t cell_x = 20 + col * 75;
        uint16_t cell_y = 255 + row * 40;

        ILI9488_FillRect(cell_x - 4, cell_y - 10, 46, 34, 40, 40, 40);

        if (bidding_history[i].type == CALL_PASS)
        {
            DrawSmallPass(cell_x, cell_y);
        }
        else if (bidding_history[i].type == CALL_DOUBLE)
        {
            DrawSmallX(cell_x, cell_y);
        }
        else if (bidding_history[i].type == CALL_BID)
        {
            DrawTinyDigit(cell_x, cell_y, bidding_history[i].level);
            DrawSmallSuit(cell_x + 20, cell_y + 2, bidding_history[i].suit);
        }
    }
}

void ClearTopPanel(void)
{
    ILI9488_FillRect(0, 0, 320, 240, 0, 0, 0);
}
void ClearColorTopPanel(void)
{
    ILI9488_FillRect(20, 20, 140, 120, 30, 30, 30);
}
void ClearNumberTopPanel(void)
{
    ILI9488_FillRect(180, 20, 120, 120, 30, 30, 30);
}


void ShowDouble(void)
{
	ClearColorTopPanel();
	ClearNumberTopPanel();
    ILI9488_FillRect(80, 60, 80, 40, 255, 0, 0);
}

void ShowLevel(void)
{
    ClearTopPanel();

    uint16_t x = 195;
    uint16_t y = 25;

    switch (current_level)
    {
        case 1:
            DrawSegmentB(x, y, 255, 255, 255);
            DrawSegmentC(x, y, 255, 255, 255);
            break;
        case 2:
            DrawSegmentA(x, y, 255, 255, 255);
            DrawSegmentB(x, y, 255, 255, 255);
            DrawSegmentG(x, y, 255, 255, 255);
            DrawSegmentE(x, y, 255, 255, 255);
            DrawSegmentD(x, y, 255, 255, 255);
            break;
        case 3:
            DrawSegmentA(x, y, 255, 255, 255);
            DrawSegmentB(x, y, 255, 255, 255);
            DrawSegmentG(x, y, 255, 255, 255);
            DrawSegmentC(x, y, 255, 255, 255);
            DrawSegmentD(x, y, 255, 255, 255);
            break;
        case 4:
            DrawSegmentF(x, y, 255, 255, 255);
            DrawSegmentG(x, y, 255, 255, 255);
            DrawSegmentB(x, y, 255, 255, 255);
            DrawSegmentC(x, y, 255, 255, 255);
            break;
        case 5:
            DrawSegmentA(x, y, 255, 255, 255);
            DrawSegmentF(x, y, 255, 255, 255);
            DrawSegmentG(x, y, 255, 255, 255);
            DrawSegmentC(x, y, 255, 255, 255);
            DrawSegmentD(x, y, 255, 255, 255);
            break;
        case 6:
            DrawSegmentA(x, y, 255, 255, 255);
            DrawSegmentF(x, y, 255, 255, 255);
            DrawSegmentG(x, y, 255, 255, 255);
            DrawSegmentE(x, y, 255, 255, 255);
            DrawSegmentC(x, y, 255, 255, 255);
            DrawSegmentD(x, y, 255, 255, 255);
            break;
        case 7:
            DrawSegmentA(x, y, 255, 255, 255);
            DrawSegmentB(x, y, 255, 255, 255);
            DrawSegmentC(x, y, 255, 255, 255);
            break;
    }
}

void ShowSuit(void)
{
    ClearColorTopPanel();

    switch(current_suit)
    {
        case CLUBS:
            DrawBigSuit(50,70,CLUBS);
            break;
        case DIAMONDS:
        	DrawBigSuit(100,40,DIAMONDS);
            break;
        case HEARTS:
        	DrawBigSuit(100,40,HEARTS);
            break;
        case SPADES:
        	DrawBigSuit(100,40,SPADES);
            break;
        case NT:
            ILI9488_FillRect(100, 70, 60, 40, 255, 255, 0);
            break;
    }
}

void Render(void)
{

    switch(current_state)
    {
        case STATE_PASS:
        	ClearColorTopPanel();
        	ClearNumberTopPanel();
            DrawBigPass(80, 60);
            break;

        case STATE_DOUBLE:

            ShowDouble();
            break;

        case STATE_BID_LEVEL:
        	ClearNumberTopPanel();
            ShowLevel();
            break;

        case STATE_BID_SUIT:
            ShowSuit();
            break;
    }
    ILI9488_FillRect(40, 140, 260,10,0,0,0);
    if (current_state == STATE_BID_LEVEL) {
            ILI9488_FillRect(195, 140, 60, 5, 255, 255, 0);
    } else if (current_state == STATE_BID_SUIT) {
           ILI9488_FillRect(40, 140, 80, 5, 255, 255, 0);
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
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);

  ILI9488_Init_Minimal();
  ClearTopPanel();
  Render();
  DrawHistory();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // PA8 = NEXT
	        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_SET)
	        {
	            HandleNext();
	            Render();
	            HAL_Delay(200);

	            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_SET)
	            {
	                HAL_Delay(10);
	            }
	        }

	        // PA9 = OK
	        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET)
	        {
	            HandleOK();
	            Render();
	            DrawHistory();

	            HAL_Delay(200);

	            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET)
	            {
	                HAL_Delay(10);
	            }
	        }

	        // PA10 = BACK
	        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET)
	        {
	            HandleBack();
	            Render();
	            HAL_Delay(200);

	            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET)
	            {
	                HAL_Delay(10);
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  if (HAL_UART_Init(&huart2) != HAL_OK)
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
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

#ifdef  USE_FULL_ASSERT
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
