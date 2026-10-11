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
    // 1. Setup physical hardware pins and initialize the eUSCI_B0 module registers
    HAL_LCD_PortInit();
    HAL_LCD_SpiInit();

    // 2. HARDWARE RESET HANDSHAKE PULSE (Critical to drop the white state)
    P3OUT |= BIT7;              // Start with Reset Line High
    __delay_cycles(100000);     // Wait for power rails to flatten out
    
    P3OUT &= ~BIT7;             // Pulse Reset LOW (Resets the ST7735 chip)
    __delay_cycles(200000);     // Keep Reset LOW for a long duration to force state wipe
    
    P3OUT |= BIT7;              // Pull Reset HIGH again to release the chip
    __delay_cycles(800000);     // CRITICAL: Give the LCD driver board 100ms to boot up its internal oscillators!

    // 3. Official ST7735 Control Command Pipeline Setup
    HAL_LCD_writeCommand(CM_SLPOUT); // Exit Sleep Mode
    __delay_cycles(1000000);         // CRITICAL: Must wait at least 120ms before sending commands after sleep out!

    HAL_LCD_writeCommand(CM_GAMSET); // Default Gamma selection profile
    HAL_LCD_writeData(0x04);

    HAL_LCD_writeCommand(CM_SETPWCTR); // Set power control parameters
    HAL_LCD_writeData(0x0A);
    HAL_LCD_writeData(0x14);

    HAL_LCD_writeCommand(CM_SETSTBA);
    HAL_LCD_writeData(0x0A);
    HAL_LCD_writeData(0x00);

    HAL_LCD_writeCommand(CM_COLMOD); // 16-bit packed RGB565 color format configuration block
    HAL_LCD_writeData(0x05);
    __delay_cycles(50000);

    HAL_LCD_writeCommand(CM_MADCTL);
    HAL_LCD_writeData(CM_MADCTL_BGR);

    HAL_LCD_writeCommand(CM_NORON);  // Set display panel to normal operations mode
    __delay_cycles(50000);

    // 4. Register boundary properties matching your screen scale dimensions
    Lcd_ScreenWidth  = LCD_VERTICAL_MAX;
    Lcd_ScreenHeigth = LCD_HORIZONTAL_MAX;
    Lcd_PenSolid  = 0;
    Lcd_FontSolid = 1;
    Lcd_FlagRead  = 0;
    Lcd_TouchTrim = 0;

    // 5. Initial Master Blue Screen Clear-Wipe Loop
    Crystalfontz128x128_SetDrawFrame(0, 0, 127, 127);
    HAL_LCD_writeCommand(CM_RAMWR);
    int i;
    for (i = 0; i < 16384; i++)
    {
        // Force-inject pure blue pixel packets immediately (0x001F = Pure Blue in RGB565 format)
        HAL_LCD_writeData(0x00); 
        HAL_LCD_writeData(0x1F);
    }

    __delay_cycles(100000);
    HAL_LCD_writeCommand(CM_DISPON); // Turn the screen graphics matrix ON!
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
    // 1. Permanently disable the distraction of the flashing onboard RGB LED channels
    P2DIR &= ~BIT6; P2OUT &= ~BIT6; 
    P3DIR &= ~BIT6; P3OUT &= ~BIT6; 

    // 2. Configure official MSP430FR6989 LCD Control Pins as Clean Outputs
    P3DIR |= BIT7;  // P3.7 = LCD_RESET
    P3DIR |= BIT1;  // P3.1 = LCD_DC (Data/Command Register Select) -> CRITICAL CORRECTION
    P3DIR |= BIT4;  // P3.4 = LCD_CS (Chip Select)

    // 3. Set standard initial idle states
    P3OUT |= BIT4;  // LCD_CS = 1 (Display idle)
    P3OUT &= ~BIT1; // LCD_DC = 0

    // 4. OFFICIAL DUAL-DOCUMENTATION PIN MATCH:
    // Route P1.4 to UCB0CLK and P1.6 to UCB0SIMO (Master Data Out)
    P1SEL0 |= BIT4 | BIT6;
    P1SEL1 &= ~(BIT4 | BIT6);
}


void HAL_LCD_SpiInit(void)
{
    // 1. Put the eUSCI_B0 peripheral module into Software Reset to safely write configurations
    UCB0CTLW0 |= UCSWRST;

    // 2. CRITICAL PHASE MODE CORRECTION:
    // Remove UCCKPH to drop Clock Phase to 0, and ensure UCCKPL is 0.
    // This perfectly aligns the MSP430FR6989 to SPI Mode 0, matching the ST7735 controller 
    // chip specifications exactly. Data bits will now serialize without half-cycle shifts.
    UCB0CTLW0 &= ~UCCKPH; // Phase = 0
    UCB0CTLW0 &= ~UCCKPL; // Polarity = 0 (Inactive state is low)
    
    // Master Mode, MSB first, Synchronous SPI mode, Clocked from 8MHz SMCLK
    UCB0CTLW0 |= UCMSB | UCMST | UCSYNC | UCSSEL__SMCLK;

    // 3. Keep the relaxed, highly stable clock divider baseline from Thomas Stoeckert's design
    // 8MHz SMCLK divided by 16 = 500KHz. Prevents clock-frequency buffer overflows.
    UCB0BR0 = 16; 
    UCB0BR1 = 0x00;

    // 4. Release software reset to lock flags and activate physical hardware lines
    UCB0CTLW0 &= ~UCSWRST;
}



void HAL_LCD_writeCommand(uint8_t command)
{
    P3OUT &= ~BIT1; // Drive P3.1 LOW -> Command Mode
    P3OUT &= ~BIT4; // Drive P3.4 LOW -> Select Display

    while ((UCB0IFG & UCTXIFG) == 0); 
    UCB0TXBUF = command;              

    while ((UCB0STATW & UCBUSY) != 0); 
    P3OUT |= BIT4;                    
}

void HAL_LCD_writeData(uint8_t data)
{
    P3OUT |= BIT1;  // Drive P3.1 HIGH -> Data Mode
    P3OUT &= ~BIT4; // Drive P3.4 LOW -> Select Display

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
