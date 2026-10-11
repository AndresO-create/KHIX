#include <msp430.h> 
#include <stdlib.h>
#include <string.h>
#include "GrLib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"

/*******************CONSTANTS********************/
#define startX 60       // Starting x position of the snake
#define startY 60       // Starting y position of snake
#define BLOCK_SIZE 6    // Screen block grid layout sizing scale
#define FRAME_TIME 333  // Up-mode interval steps for a 10KHz internal VLO clock (30 FPS)
#define SCREEN_MAX 120  // Max boundaries of display panel

/*******************STRUCTS********************/
typedef struct SnakeNode {
	int x;
	int y;
	struct SnakeNode *next;
} SnakeNode;

typedef struct SnakeGame {
	SnakeNode *head; 
	SnakeNode *tail; 
	int dirX, dirY, length;
} SnakeGame;

typedef struct Apple {
	int x, y;
} Apple;

/*******************GLOBALS********************/
SnakeGame snake;

Graphics_Context g_sContext;
extern const Graphics_Display g_sCrystalfontz128x128; 

Apple apple;

volatile int foodEatenFlag = 0;
volatile int gameOver = 0;       // CRITICAL FIX: Marked volatile to prevent optimization freezes
volatile uint16_t adcResult; 

/*******************FUNCTION PROTOTYPES********************/
void initSnake(void);
void freeSnake(void);
void enqueue(int posX, int posY); // Direct variable updates to prevent malloc bleed loops
void dequeue(void);
SnakeNode *addSnake(int posX, int posY);
void moveSnake(int foodFlag);
int handleCollision(void);
void init_Graphics(void);
void placeApple(void);
void config_System_Clocks(void);
void init_ADC(void);

/*******************MAIN LOOP********************/
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	// 1. Clear core I/O multiplexer locks immediately
	PM5CTL0 &= ~LOCKLPM5;
	
	// Sanity Check Pin: Set Port 9 Pin 7 (Green LED) as an Output
	P9DIR |= BIT7;
	P9OUT &= ~BIT7; // Start OFF

	// 2. Configure System Clocks cleanly (Sets SMCLK to a stable 8MHz for your SPI bus)
	config_System_Clocks();

	// 3. Initialize display graphics context and clear screen to Blue
	init_Graphics();

	// 4. Setup Game State Data Structures
	initSnake();
	placeApple();
	init_ADC();

	// 5. Configure and enable the Timer_A0 frame clock
	TA0CCR0 = FRAME_TIME;             
	TA0CCTL0 = CCIE;                  
	TA0CTL = TASSEL_1 | MC_1 | TACLR; // Sourced from ACLK (VLO clock), Up-mode operation

	// 6. CRITICAL FIX: Run in fully active processing mode with Global Interrupts enabled.
	// Bypasses LPM sleep states entirely during pipeline testing to keep the CPU unblocked!
	__bis_SR_register(GIE); 

	while(1)
	{
		// Check if a fail-state reset sequence was flagged
		if (gameOver) {
			__bic_SR_register(GIE); // Temporarily halt interrupts during cleanup
			
			Graphics_clearDisplay(&g_sContext);
			Graphics_drawStringCentered(&g_sContext, (int8_t *)"GAME OVER", AUTO_STRING_LENGTH, 60, 60, TRANSPARENT_TEXT);
			
			freeSnake();
			initSnake();
			placeApple();
			gameOver = 0; 

			__bis_SR_register(GIE); // Re-engage timing cycles
		}
	}
}

/*******************INTERRUPTS********************/
#pragma vector = TIMER0_A0_VECTOR 
__interrupt void nextFrame(void) {
    P9OUT ^= BIT7; // Flicker the green LED to verify the loop is running smoothly

    int status = handleCollision();
    if (status == 1) {
        gameOver = 1;
        return;
    }
	
    if (snake.head != NULL && snake.head->x == apple.x && snake.head->y == apple.y) {
        foodEatenFlag = 1;
        placeApple(); 
    }
	
    moveSnake(foodEatenFlag);
    foodEatenFlag = 0; 
}

/*******************FUNCTION BODIES********************/

void config_System_Clocks(void) {
    CSCTL0_H = CSKEY_H;                     // Unlock Clock System registers
    
    // Configure the internal DCO to run at 8 MHz precisely
    CSCTL1 = DCOFSEL_3;                     // DCO = 8 MHz selection
    
    // Map MCLK and SMCLK straight to the high-speed 8 MHz DCO clock source
    // SELA__VLOCLK maps ACLK to the internal 10 KHz VLO clock oscillator safely
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK; 
    
    // Set all system clock dividers to 1 (Full 8MHz speed)
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;   
    
    // Clear any loose clock oscillator fault flags that could stall loops
    CSCTL5 &= ~(LFXTOFFG | HFXTOFFG);
    
    CSCTL0_H = 0;                           // Re-lock Clock System registers
}


void init_Graphics(void){
    // Clean out the graphics context memory block before initializing
    memset(&g_sContext, 0, sizeof(Graphics_Context));

    // Wait 50ms for display internal capacitors to fully level out
    __delay_cycles(400000);

    // Initialize the physical controller chip lines over eUSCI_B0
    Crystalfontz128x128_Init();
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);
    
    // Bind the generic g_sContext structure to your physical display driver
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);

    // Configure your screen matrix boundaries inside the grlib layer explicitly
    g_sContext.clipRegion.xMin = 0;
    g_sContext.clipRegion.yMin = 0;
    g_sContext.clipRegion.xMax = 127;
    g_sContext.clipRegion.yMax = 127;

    // Apply your game board layout canvas stylings
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLUE);  
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE); 
    Graphics_setFont(&g_sContext, &g_sFontCm12);
    
    // This call will now execute instantly past your previous crash point!
    Graphics_clearDisplay(&g_sContext);
}




void initSnake(void){
	snake.head = NULL;
	snake.tail = NULL;
	snake.length = 0;
	snake.dirX = 1;
	snake.dirY = 0;

	int i;
	for(i = 2; i >= 0; i--){
		enqueue(startX - (BLOCK_SIZE * i), startY);
	}
}

void enqueue(int posX, int posY){
	SnakeNode *newNode = (SnakeNode *)malloc(sizeof(SnakeNode));
	if(newNode == NULL) return;
	
	newNode->x = posX;
	newNode->y = posY;
	newNode->next = NULL;

	if(snake.tail == NULL){
		snake.head = newNode;
		snake.tail = newNode;
	}
	else {
		snake.head->next = newNode;
		snake.head = newNode;
	}
	snake.length++;
}

void dequeue(void){
	if (snake.tail == NULL) return;
	
	SnakeNode *temp = snake.tail;
	drawRect(temp->x, temp->y, GRAPHICS_COLOR_BLUE);
	
	snake.tail = snake.tail->next; 
	if (snake.tail == NULL){
		snake.head = NULL;
	}

	free(temp); 
	snake.length--;
}

void moveSnake(int foodFlag){
    if (snake.head == NULL) return;
	int nextX = snake.head->x + (snake.dirX * BLOCK_SIZE);
	int nextY = snake.head->y + (snake.dirY * BLOCK_SIZE);
	
	enqueue(nextX, nextY);
	drawRect(nextX, nextY, GRAPHICS_COLOR_WHITE);
	if (!foodFlag) dequeue(); 
}

Graphics_Rectangle drawRect(int xMin, int yMin, uint32_t color){
	Graphics_Rectangle rect;
	rect.xMin = xMin;
	rect.yMin = yMin;
	rect.xMax = xMin + (BLOCK_SIZE - 1);
	rect.yMax = yMin + (BLOCK_SIZE - 1);
	Graphics_setForegroundColor(&g_sContext, color);
	Graphics_fillRectangle(&g_sContext, &rect);
	return rect;
}

int handleCollision(void){
	if (snake.head == NULL) return 0;

	if (
		snake.head->x < 0 || 
		snake.head->x > (SCREEN_MAX - BLOCK_SIZE) || 
		snake.head->y < 0 || 
		snake.head->y > (SCREEN_MAX - BLOCK_SIZE)
	)   return 1;

	SnakeNode *temp = snake.tail;
	while (temp != snake.head && temp != NULL){
		if (snake.head->x == temp->x && snake.head->y == temp->y){
			return 1; 
		}
		temp = temp->next;
	}
	return 0;
}

void freeSnake(void){
	while (snake.tail != NULL) {
		dequeue();
	}
}

void placeApple(void) {
    int wishX, wishY;
    int collision;
    SnakeNode *temp;
    do {
        collision = 0; 
        wishX = (rand() % 19) * BLOCK_SIZE; 
        wishY = (rand() % 19) * BLOCK_SIZE;
        temp = snake.tail;
        while (temp != NULL) {
            if (wishX == temp->x && wishY == temp->y) {
                collision = 1; 
                break;         
            }
            temp = temp->next; 
        }
    } while (collision); 
    apple.x = wishX;
    apple.y = wishY;
    drawRect(apple.x, apple.y, GRAPHICS_COLOR_RED);
}

void init_ADC(void) {
    P1SEL0 |= BIT6 | BIT7;
    P1SEL1 |= BIT6 | BIT7;
    ADC12CTL0 = ADC12SHT0_4 | ADC12ON;       
    ADC12CTL1 = ADC12SHP | ADC12CONSEQ_1;    
    ADC12MCTL0 = ADC12INCH_6;                 
    ADC12MCTL1 = ADC12INCH_7 | ADC12EOS;     
}
