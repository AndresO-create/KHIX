#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include "msp430fr6989.h"
#include <stdint.h>
#include <stddef.h>

uint8_t Lcd_Orientation;
//uint16_t Lcd_ScreenWidth, Lcd_ScreenHeigth;
uint8_t Lcd_PenSolid, Lcd_FontSolid, Lcd_FlagRead;
uint16_t Lcd_TouchTrim;

// Low-level internal port setup prototypes
void HAL_LCD_PortInit(void);
void HAL_LCD_SpiInit(void);
void HAL_LCD_writeCommand(uint8_t command);
void HAL_LCD_writeData(uint8_t data);

// Forward declarations for function table linking unblocking
void Crystalfontz128x128_PixelDraw(void *pvDisplayData, int16_t lX, int16_t lY, uint16_t ulValue);
void Crystalfontz128x128_PixelDrawMultiple(void *pvDisplayData, int16_t lX, int16_t lY, int16_t lX0, int16_t lCount, int16_t lBPP, const uint8_t *pucData, const uint32_t *pucPalette);
void Crystalfontz128x128_LineDrawH(void *pvDisplayData, int16_t lX1, int16_t lX2, int16_t lY, uint16_t ulValue);
void Crystalfontz128x128_LineDrawV(void *pvDisplayData, int16_t lX, int16_t lY1, int16_t lY2, uint16_t ulValue);
void Crystalfontz128x128_RectFill(void *pvDisplayData, const Graphics_Rectangle *psRect, uint16_t ulValue);
uint32_t Crystalfontz128x128_ColorTranslate(void *pvDisplayData, uint32_t ulValue);
void Crystalfontz128x128_Flush(void *pvDisplayData);
void Crystalfontz128x128_ClearDisplay(void *pvDisplayData, uint16_t ulValue);

// OFFICIAL TI GRLIB STRUCT BINDING MATRIX
// This 44-byte format perfectly matches the structure layouts inside grlib.h and context.c
const Graphics_Display g_sCrystalfontz128x128 =
{
    sizeof(Graphics_Display),
    NULL,                                  // displayData pointer
    128,                                   // width -> Matches the 5908 memory slot exactly!
    128,                                   // heigth -> Matches the 4166 memory slot exactly!
    Crystalfontz128x128_PixelDraw,         // Functions realign to their correct tracks
    Crystalfontz128x128_PixelDrawMultiple,
    Crystalfontz128x128_LineDrawH,
    Crystalfontz128x128_LineDrawV,
    Crystalfontz128x128_RectFill,
    Crystalfontz128x128_ColorTranslate,
    Crystalfontz128x128_Flush,
    Crystalfontz128x128_ClearDisplay       // Now populates slot 10 safely, preventing the NULL pointer crash!
};




void Crystalfontz128x128_Init(void)
{
    HAL_LCD_PortInit();
    HAL_LCD_SpiInit();

    // Hardware Reset Handshake Pulse
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

    //Lcd_ScreenWidth  = LCD_VERTICAL_MAX;
    //Lcd_ScreenHeigth = LCD_HORIZONTAL_MAX;
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
    P2DIR &= ~BIT6; P2OUT &= ~BIT6; 
    P3DIR &= ~BIT6; P3OUT &= ~BIT6; 

    P3DIR |= BIT7;  // Reset -> Pin 17
    P3DIR |= BIT1;  // DC -> Pin J4.31 (P3.1)
    P3DIR |= BIT4;  // CS -> Pin J2.13 (P3.4)

    P3OUT |= BIT4;  
    P3OUT &= ~BIT1; 

    // SPI Port Configuration standard alignment (CLK = P1.4, MOSI = P1.6)
    P1SEL1 |= (BIT4 | BIT6);
    P1SEL0 &= ~(BIT4 | BIT6);
}

void HAL_LCD_SpiInit(void)
{
    UCB0CTLW0 |= UCSWRST;
    UCB0CTLW0 &= ~UCCKPH; 
    UCB0CTLW0 &= ~UCCKPL; 
    UCB0CTLW0 |= UCMSB | UCMST | UCSYNC | UCSSEL__SMCLK;
    UCB0BR0 = 0x02; // Fast 4MHz SPI over high-speed 8MHz clock bus
    UCB0BR1 = 0x00;
    UCB0CTLW0 &= ~UCSWRST;
}

void HAL_LCD_writeCommand(uint8_t command)
{
    P3OUT &= ~BIT1; 
    P3OUT &= ~BIT4; 
    while ((UCB0IFG & UCTXIFG) == 0); 
    UCB0TXBUF = command;              
    while ((UCB0STATW & UCBUSY) != 0); 
    P3OUT |= BIT4;                    
}

void HAL_LCD_writeData(uint8_t data)
{
    P3OUT |= BIT1;  
    P3OUT &= ~BIT4; 
    while ((UCB0IFG & UCTXIFG) == 0); 
    UCB0TXBUF = data;             
    while ((UCB0STATW & UCBUSY) != 0); 
    P3OUT |= BIT4;                    
}

void Crystalfontz128x128_PixelDraw(void *pvDisplayData, int16_t lX, int16_t lY, uint16_t ulValue)
{
    Crystalfontz128x128_SetDrawFrame(lX,lY,lX,lY);
    HAL_LCD_writeCommand(CM_RAMWR);
    HAL_LCD_writeData(ulValue>>8);
    HAL_LCD_writeData(ulValue);
}

void Crystalfontz128x128_PixelDrawMultiple(void *pvDisplayData, int16_t lX, int16_t lY, int16_t lX0, int16_t lCount, int16_t lBPP, const uint8_t *pucData, const uint32_t *pucPalette)
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

void Crystalfontz128x128_LineDrawH(void *pvDisplayData, int16_t lX1, int16_t lX2, int16_t lY, uint16_t ulValue)
{
    int16_t i;
    Crystalfontz128x128_SetDrawFrame(lX1, lY, lX2, lY);
    HAL_LCD_writeCommand(CM_RAMWR);
    for(i = lX1; i <= lX2; i++) {
        HAL_LCD_writeData(ulValue >> 8); HAL_LCD_writeData(ulValue);
    }
}

void Crystalfontz128x128_LineDrawV(void *pvDisplayData, int16_t lX, int16_t lY1, int16_t lY2, uint16_t ulValue)
{
    int16_t i;
    Crystalfontz128x128_SetDrawFrame(lX, lY1, lX, lY2);
    HAL_LCD_writeCommand(CM_RAMWR);
    for(i = lY1; i <= lY2; i++) {
        HAL_LCD_writeData(ulValue >> 8); HAL_LCD_writeData(ulValue);
    }
}

void Crystalfontz128x128_RectFill(void *pvDisplayData, const Graphics_Rectangle *psRect, uint16_t ulValue)
{
    int16_t i, numPixels;
    Crystalfontz128x128_SetDrawFrame(psRect->xMin, psRect->yMin, psRect->xMax, psRect->yMax);
    HAL_LCD_writeCommand(CM_RAMWR);
    numPixels = (psRect->xMax - psRect->xMin + 1) * (psRect->yMax - psRect->yMin + 1);
    for(i = 0; i < numPixels; i++) {
        HAL_LCD_writeData(ulValue >> 8); HAL_LCD_writeData(ulValue);
    }
}

uint32_t Crystalfontz128x128_ColorTranslate(void *pvDisplayData, uint32_t ulValue)
{
    return(((ulValue & 0x00F80000) >> 8) | ((ulValue & 0x0000FC00) >> 5) | ((ulValue & 0x000000F8) >> 3));
}

void Crystalfontz128x128_Flush(void *pvDisplayData) {}

void Crystalfontz128x128_ClearDisplay(void *pvDisplayData, uint16_t ulValue)
{
    Graphics_Rectangle rect = {0, 0, 127, 127};
    Crystalfontz128x128_RectFill(pvDisplayData, &rect, ulValue);
}
