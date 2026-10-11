#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include "msp430fr6989.h"
#include <stdint.h>
#include <stddef.h>  // Crucial: Defines NULL safely


uint8_t Lcd_Orientation;
uint16_t Lcd_ScreenWidth, Lcd_ScreenHeigth;
uint8_t Lcd_PenSolid, Lcd_FontSolid, Lcd_FlagRead;
uint16_t Lcd_TouchTrim;

// Forward declarations for low-level internal driver functions
void HAL_LCD_PortInit(void);
void HAL_LCD_SpiInit(void);
void HAL_LCD_writeCommand(uint8_t command);
void HAL_LCD_writeData(uint8_t data);

static void Crystalfontz128x128_PixelDraw(void *pvDisplayData, int16_t lX, int16_t lY, uint16_t ulValue);
static void Crystalfontz128x128_PixelDrawMultiple(void *pvDisplayData, int16_t lX, int16_t lY, int16_t lX0, int16_t lCount, int16_t lBPP, const uint8_t *pucData, const uint32_t *pucPalette);
static void Crystalfontz128x128_LineDrawH(void *pvDisplayData, int16_t lX1, int16_t lX2, int16_t lY, uint16_t ulValue);
static void Crystalfontz128x128_LineDrawV(void *pvDisplayData, int16_t lX, int16_t lY1, int16_t lY2, uint16_t ulValue);
static void Crystalfontz128x128_RectFill(void *pvDisplayData, const Graphics_Rectangle *psRect, uint16_t ulValue);
static uint32_t Crystalfontz128x128_ColorTranslate(void *pvDisplayData, uint32_t ulValue);
static void Crystalfontz128x128_Flush(void *pvDisplayData);
static void Crystalfontz128x128_ClearDisplay(void *pvDisplayData, uint16_t ulValue);

// MASTER DISPLAY DATA STRUCTURE BINDING: Maps grlib math commands to driver functions
const Graphics_Display g_sCrystalfontz128x128 =
{
    sizeof(Graphics_Display),
    NULL,
    Crystalfontz128x128_PixelDraw,
    Crystalfontz128x128_PixelDrawMultiple,
    Crystalfontz128x128_LineDrawH,
    Crystalfontz128x128_LineDrawV,
    Crystalfontz128x128_RectFill,
    Crystalfontz128x128_ColorTranslate,
    Crystalfontz128x128_Flush,
    Crystalfontz128x128_ClearDisplay
};

void Crystalfontz128x128_Init(void)
{
    HAL_LCD_PortInit();
    HAL_LCD_SpiInit();

    // Reset pulse to wake up the screen controller chip
    P3OUT &= ~BIT7;             
    __delay_cycles(20000);      
    P3OUT |= BIT7;              
    __delay_cycles(150000);     

    HAL_LCD_writeCommand(CM_SLPOUT); 
    __delay_cycles(200000);          

    HAL_LCD_writeCommand(CM_GAMSET); 
    HAL_LCD_writeData(0x04);

    HAL_LCD_writeCommand(CM_SETPWCTR); 
    HAL_LCD_writeData(0x0A);
    HAL_LCD_writeData(0x14);

    HAL_LCD_writeCommand(CM_SETSTBA);
    HAL_LCD_writeData(0x0A);
    HAL_LCD_writeData(0x00);

    HAL_LCD_writeCommand(CM_COLMOD); 
    HAL_LCD_writeData(0x05);
    __delay_cycles(20000);

    HAL_LCD_writeCommand(CM_MADCTL);
    HAL_LCD_writeData(CM_MADCTL_BGR);

    HAL_LCD_writeCommand(CM_NORON); 

    Lcd_ScreenWidth  = LCD_VERTICAL_MAX;
    Lcd_ScreenHeigth = LCD_HORIZONTAL_MAX;
    Lcd_PenSolid  = 0;
    Lcd_FontSolid = 1;
    Lcd_FlagRead  = 0;
    Lcd_TouchTrim = 0;

    Crystalfontz128x128_SetDrawFrame(0, 0, 127, 127);
    HAL_LCD_writeCommand(CM_RAMWR);
    int i;
    for (i = 0; i < 16384; i++)
    {
        HAL_LCD_writeData(0x00); 
        HAL_LCD_writeData(0x1F); // pure blue backdrop fill
    }

    __delay_cycles(20000);
    HAL_LCD_writeCommand(CM_DISPON); 
}

void Crystalfontz128x128_SetDrawFrame(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    switch (Lcd_Orientation) {
        case 0:  x0 += 2; y0 += 3; x1 += 2; y1 += 3; break;
        case 1:  x0 += 3; y0 += 2; x1 += 3; y1 += 2; break;
        case 2:  x0 += 2; y0 += 1; x1 += 2; y1 += 1; break;
        case 3:  x0 += 1; y0 += 2; x1 += 1; y1 += 2; break;
        default: break;
    }

    HAL_LCD_writeCommand(CM_CASET);
    HAL_LCD_writeData((uint8_t)(x0 >> 8)); HAL_LCD_writeData((uint8_t)(x0));
    HAL_LCD_writeData((uint8_t)(x1 >> 8)); HAL_LCD_writeData((uint8_t)(x1));

    HAL_LCD_writeCommand(CM_RASET);
    HAL_LCD_writeData((uint8_t)(y0 >> 8)); HAL_LCD_writeData((uint8_t)(y0));
    HAL_LCD_writeData((uint8_t)(y1 >> 8)); HAL_LCD_writeData((uint8_t)(y1));
}

void Crystalfontz128x128_SetOrientation(uint8_t orientation)
{
    Lcd_Orientation = orientation;
    HAL_LCD_writeCommand(CM_MADCTL);
    switch (Lcd_Orientation) {
        case LCD_ORIENTATION_UP:    HAL_LCD_writeData(CM_MADCTL_MX | CM_MADCTL_MY | CM_MADCTL_BGR); break;
        case LCD_ORIENTATION_LEFT:  HAL_LCD_writeData(CM_MADCTL_MY | CM_MADCTL_MV | CM_MADCTL_BGR); break;
        case LCD_ORIENTATION_DOWN:  HAL_LCD_writeData(CM_MADCTL_BGR); break;
        case LCD_ORIENTATION_RIGHT: HAL_LCD_writeData(CM_MADCTL_MX | CM_MADCTL_MV | CM_MADCTL_BGR); break;
    }
}

void HAL_LCD_PortInit(void)
{
    // 1. Permanently keep the distracting onboard RGB LED channels turned OFF
    P2DIR &= ~BIT6; P2OUT &= ~BIT6; 
    P3DIR &= ~BIT6; P3OUT &= ~BIT6; 

    // 2. Configure MSP430FR6989 LCD Control Pins as Clean Outputs
    P3DIR |= BIT7;  // P3.7 = LCD_RESET (Reset Line)
    P2DIR |= BIT4;  // P2.4 = LCD_DC    (Data / Command Toggle)
    P3DIR |= BIT4;  // P3.4 = LCD_CS    (Chip Select)

    // 3. Set standard starting idle states
    P3OUT |= BIT4;  // Drive LCD_CS HIGH (Idle state, display not selected)
    P2OUT &= ~BIT4; // Drive LCD_DC LOW

    // 4. Map P1.5 and P1.6 to their eUSCI_B0 SPI Alternate Functions
    // On the MSP430FR6989, P1.5 is UCB0CLK and P1.6 is UCB0SIMO
    P1SEL0 |= BIT5 | BIT6;
    P1SEL1 &= ~(BIT5 | BIT6);
}

void HAL_LCD_SpiInit(void)
{
    // 1. Assert software reset to safely alter clock multiplexer registers
    UCB0CTLW0 |= UCSWRST;

    // 2. CRITICAL REALIGNMENT: Change UCSSEL__SMCLK to UCSSEL__ACLK 
    // This tells the SPI master to drive communication cycles using your ultra-stable 32KHz crystal clock
    UCB0CTLW0 |= UCCKPH | UCMSB | UCMST | UCSYNC | UCSSEL__ACLK;

    // 3. Set your clock divider parameters
    // Since ACLK runs at 32,768 Hz, dividing it by 1 or 2 yields a safe, rock-solid baseline
    UCB0BR0 = 0x01; // Divide clock frequency by 1
    UCB0BR1 = 0x00;

    // 4. Release software reset to lock configurations and activate physical pins
    UCB0CTLW0 &= ~UCSWRST;
}

void HAL_LCD_writeCommand(uint8_t command)
{
    P2OUT &= ~BIT4; 
    P3OUT &= ~BIT4; 
    while ((UCB0IFG & UCTXIFG) == 0); 
    UCB0TXBUF = command;              
    while ((UCB0STATW & UCBUSY) != 0); 
    P3OUT |= BIT4;                    
}

void HAL_LCD_writeData(uint8_t data)
{
    P2OUT |= BIT4;  
    P3OUT &= ~BIT4; 
    while ((UCB0IFG & UCTXIFG) == 0); 
    UCB0TXBUF = data;             
    while ((UCB0STATW & UCBUSY) != 0); 
    P3OUT |= BIT4;                    
}

static void Crystalfontz128x128_PixelDraw(void *pvDisplayData, int16_t lX, int16_t lY, uint16_t ulValue)
{
    Crystalfontz128x128_SetDrawFrame(lX,lY,lX,lY);
    HAL_LCD_writeCommand(CM_RAMWR);
    HAL_LCD_writeData(ulValue>>8);
    HAL_LCD_writeData(ulValue);
}

static void Crystalfontz128x128_PixelDrawMultiple(void *pvDisplayData, int16_t lX, int16_t lY, int16_t lX0, int16_t lCount, int16_t lBPP, const uint8_t *pucData, const uint32_t *pucPalette)
{
    uint16_t Data;
    Crystalfontz128x128_SetDrawFrame(lX,lY,lX+lCount-1,lY);
    HAL_LCD_writeCommand(CM_RAMWR);
    switch(lBPP)
    {
        case 1:
            while(lCount > 0)
            {
                Data = *pucData++;
                for(; (lX0 < 8) && lCount; lX0++, lCount--)
                {
                    uint16_t color = ((uint16_t *)pucPalette)[(Data >> (7 - lX0)) & 1];
                    HAL_LCD_writeData(color >> 8); HAL_LCD_writeData(color);
                }
                lX0 = 0;
            }
            break;
    }
}

static void Crystalfontz128x128_LineDrawH(void *pvDisplayData, int16_t lX1, int16_t lX2, int16_t lY, uint16_t ulValue)
{
    int16_t i;
    Crystalfontz128x128_SetDrawFrame(lX1, lY, lX2, lY);
    HAL_LCD_writeCommand(CM_RAMWR);
    for(i = lX1; i <= lX2; i++) {
        HAL_LCD_writeData(ulValue >> 8); HAL_LCD_writeData(ulValue);
    }
}

static void Crystalfontz128x128_LineDrawV(void *pvDisplayData, int16_t lX, int16_t lY1, int16_t lY2, uint16_t ulValue)
{
    int16_t i;
    Crystalfontz128x128_SetDrawFrame(lX, lY1, lX, lY2);
    HAL_LCD_writeCommand(CM_RAMWR);
    for(i = lY1; i <= lY2; i++) {
        HAL_LCD_writeData(ulValue >> 8); HAL_LCD_writeData(ulValue);
    }
}

static void Crystalfontz128x128_RectFill(void *pvDisplayData, const Graphics_Rectangle *psRect, uint16_t ulValue)
{
    int16_t i, numPixels;
    Crystalfontz128x128_SetDrawFrame(psRect->xMin, psRect->yMin, psRect->xMax, psRect->yMax);
    HAL_LCD_writeCommand(CM_RAMWR);
    numPixels = (psRect->xMax - psRect->xMin + 1) * (psRect->yMax - psRect->yMin + 1);
    for(i = 0; i < numPixels; i++) {
        HAL_LCD_writeData(ulValue >> 8); HAL_LCD_writeData(ulValue);
    }
}

static uint32_t Crystalfontz128x128_ColorTranslate(void *pvDisplayData, uint32_t ulValue)
{
    // Translates 24-bit grlib hex colors into packed 16-bit RGB565 used by the ST7735 LCD
    return(((ulValue & 0x00F80000) >> 8) | ((ulValue & 0x0000FC00) >> 5) | ((ulValue & 0x000000F8) >> 3));
}

static void Crystalfontz128x128_Flush(void *pvDisplayData) {}

static void Crystalfontz128x128_ClearDisplay(void *pvDisplayData, uint16_t ulValue)
{
    Graphics_Rectangle rect = {0, 0, 127, 127};
    Crystalfontz128x128_RectFill(pvDisplayData, &rect, ulValue);
}
