#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include "msp430fr6989.h"
#include <stdint.h>
#include <stddef.h>

uint8_t Lcd_Orientation;
uint16_t Lcd_ScreenWidth, Lcd_ScreenHeigth;
uint8_t Lcd_PenSolid, Lcd_FontSolid, Lcd_FlagRead;
uint16_t Lcd_TouchTrim;

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

    P3OUT |= BIT7;              
    __delay_cycles(100000);     
    
    P3OUT &= ~BIT7;             
    __delay_cycles(200000);     
    
    P3OUT |= BIT7;              
    __delay_cycles(800000);     

    HAL_LCD_writeCommand(CM_SLPOUT); 
    __delay_cycles(1000000);         

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
    __delay_cycles(50000);

    HAL_LCD_writeCommand(CM_MADCTL);
    HAL_LCD_writeData(CM_MADCTL_BGR);

    HAL_LCD_writeCommand(CM_NORON);  
    __delay_cycles(50000);

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
        HAL_LCD_writeData(0x1F); 
    }

    __delay_cycles(100000);
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
    // 1. Keep the BoosterPack RGB LEDs completely turned OFF
    P2DIR &= ~BIT6; P2OUT &= ~BIT6; 
    P3DIR &= ~BIT6; P3OUT &= ~BIT6; 

    // 2. Configure official LCD Control Pins as clean outputs
    P3DIR |= BIT7;  // P3.7 = LCD_RESET
    P3DIR |= BIT1;  // P3.1 = LCD_DC (Data/Command Register Select Pin)
    P3DIR |= BIT4;  // P3.4 = LCD_CS (Chip Select)

    // 3. Set standard initial idle states
    P3OUT |= BIT4;  // LCD_CS = 1 (Display de-selected/idle)
    P3OUT &= ~BIT1; // LCD_DC = 0

    // === CRITICAL TI ARCHITECTURE CROSS-REFERENCE FIX ===
    // Force P1.5 (UCB0STE) to act as a general output driven LOW. 
    // This stops the eUSCI_B0 module from thinking an external master is interrupting
    // the line, completely unblocking the SPI register transmission flags.
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;

    // 4. Route P1.4 to UCB0CLK and P1.6 to UCB0SIMO (Master Data Out)
    P1SEL0 |= BIT4 | BIT6;
    P1SEL1 &= ~(BIT4 | BIT6);
}


void HAL_LCD_SpiInit(void)
{
    // 1. Put the eUSCI_B0 peripheral module into Software Reset to safely write configurations
    UCB0CTLW0 |= UCSWRST;

    // 2. CONNECT TO HIGH-SPEED CLOCK: Point the module to use the newly stabilized 8 MHz SMCLK
    UCB0CTLW0 &= ~UCSSEL_3;       // Clear old clock selection bits
    UCB0CTLW0 |= UCSSEL__SMCLK;   // Select SMCLK (8 MHz)
    
    // SPI Mode 0 alignment: Phase = 0, Polarity = 0
    UCB0CTLW0 &= ~UCCKPH; 
    UCB0CTLW0 &= ~UCCKPL; 
    
    // Master Mode, MSB first, Synchronous SPI mode configuration
    UCB0CTLW0 |= UCMSB | UCMST | UCSYNC;

    // 3. Set a safe bit clock divider (8 MHz SMCLK divided by 2 = 4 MHz)
    // 4 MHz is the perfect speed to clear 16,384 pixels instantly without data dropping
    UCB0BR0 = 0x02; 
    UCB0BR1 = 0x00;

    // 4. Release software reset to activate physical hardware lines
    UCB0CTLW0 &= ~UCSWRST;
}



void HAL_LCD_writeCommand(uint8_t command)
{
    P3OUT &= ~BIT1; // P3.1 = LCD_DC LOW -> Entering Command Mode
    P3OUT &= ~BIT4; // P3.4 = LCD_CS LOW -> Selecting Screen Module

    // Wait until the hardware transmit buffer is empty and ready for a new byte
    while ((UCB0IFG & UCTXIFG) == 0); 
    UCB0TXBUF = command;              // Load byte into the serial register

    // Wait for the hardware shifter to completely finish pushing the byte out over the wire
    while ((UCB0STATW & UCBUSY) != 0); 

    P3OUT |= BIT4;  // Pull LCD_CS HIGH -> Put display bus to idle
}

void HAL_LCD_writeData(uint8_t data)
{
    P3OUT |= BIT1;  // P3.1 = LCD_DC HIGH -> Entering Pixel Data Mode
    P3OUT &= ~BIT4; // P3.4 = LCD_CS LOW -> Selecting Screen Module

    // Wait until the hardware transmit buffer is empty and ready for a new byte
    while ((UCB0IFG & UCTXIFG) == 0); 
    UCB0TXBUF = data;             

    // Wait for the hardware shifter to completely finish pushing the byte out over the wire
    while ((UCB0STATW & UCBUSY) != 0); 

    P3OUT |= BIT4;  // Pull LCD_CS HIGH -> Put display bus to idle
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
    return(((ulValue & 0x00F80000) >> 8) | ((ulValue & 0x0000FC00) >> 5) | ((ulValue & 0x000000F8) >> 3));
}

static void Crystalfontz128x128_Flush(void *pvDisplayData) {}

static void Crystalfontz128x128_ClearDisplay(void *pvDisplayData, uint16_t ulValue)
{
    Graphics_Rectangle rect = {0, 0, 127, 127};
    Crystalfontz128x128_RectFill(pvDisplayData, &rect, ulValue);
}
