#include <msp430.h> 
#include <stdlib.h>
#include <string.h>
#include "GrLib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"

/*******************CONSTANTS********************/
#define startX 63       // Starting x position of the snake
#define startY 63       // Starting y position of snake
#define BLOCK_SIZE 8    // Screen block grid layout sizing scale
#define FRAME_TIME 3199  // Up-mode interval steps for a 10KHz internal VLO clock (30 FPS)
#define SCREEN_MAX 127  // Max boundaries of display panel
#define DEADZONE_LOW 1023//minimum value of the analogue stick for an input to register low
#define DEADZONE_HIGH 3071 //minimum value of the analouge stick for an input to register high

//buttons and LEDs
#define S1 BIT1
#define S2 BIT2
#define redLED BIT0
#define greenLED BIT7

/*******************STRUCTS********************/
typedef struct Vec2 {
    int x;
    int y;
} Vec2;

typedef struct SnakeNode {
	Vec2 pos;
	struct SnakeNode *next;
    struct SnakeNode *prev;
} SnakeNode;

typedef struct Snake {
	int length;
    Vec2 direction;
    SnakeNode *front; 
	SnakeNode *back; 
} Snake;

typedef struct Apple {
	Vec2 pos;
} Apple;

/*******************GLOBALS********************/
Snake snake = {0, {1, 0}, NULL, NULL};

Graphics_Context g_sContext;
extern const Graphics_Display g_sCrystalfontz128x128; 

Apple apple;

volatile int foodEatenFlag = 0;
volatile int gameOver = 0;
volatile uint16_t adcResult; 

//directions
Vec2 UP = {0, 1};
Vec2 DOWN = {0, -1};
Vec2 LEFT = {-1, 0};
Vec2 RIGHT = {1, 0};
Vec2 ZERO = {0,0};
/*******************FUNCTION PROTOTYPES********************/
//snake functions
void initSnake(void);
void freeSnake(void);
void push(Vec2 pos); // Direct variable updates to prevent malloc bleed loops
void pop(void);
SnakeNode *addSnake(Vec2 pos);
void moveSnake(int foodFlag);

//collision functions
int handleCollision(void);
bool isColliding(Vec2 objectA, Vec2 objectB);

//apple functions
void placeApple(void);

//configuration functions
void Initialize_Clock_System(void);
void config_ACLK_to_32KHz_crystal();
void Initialize_ADC(void);

//drawing functions
Graphics_Rectangle drawRect(Vec2 minVals, uint32_t color);

//accept input
void setDir(Vec2 wishDir);
Vec2 getJoyAxis();

//compare two vec2 variables to see if they are equal
bool compareVec2(Vec2 vecA, Vec2 vecB);

//start the game
void initGame();


/*******************MAIN LOOP********************/
int main(void)
{
	// Configure WDT & GPIO
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    // Configure LEDs
    P1DIR |= redLED;                P9DIR |= greenLED;
    P1OUT &= ~redLED;               P9OUT &= ~greenLED;

    // Configure buttons
    P1DIR &= ~(S1|S2);
    P1REN |= (S1|S2);
    P1OUT |= (S1|S2);
    P1IES |= (S1 | S2);
    P1IE |= (S1 | S2);
    P1IFG &= ~(S1|S2);          // Flags are used for latched polling

    //configure timer
    TA0CCTL0 = CCIE; // Enable interrupt for CCR0
    TA0CTL = TASSEL_1 | MC_1 | TACLR; // ACLK, Up mode, clear TAR
    TA0CCR0 = FRAME_TIME; //1 second delay

    // Set the LCD backlight to highest level
    //P2DIR |= BIT6;
    //P2OUT |= BIT6;

    // Configure clock system
    Initialize_Clock_System();

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Graphics functions
    Crystalfontz128x128_Init();         // Initialize the display

    // Set the screen orientation
    Crystalfontz128x128_SetOrientation(0);

    // Initialize the context
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);

    // Set background and foreground colors
    Graphics_setBackgroundColor(&g_sContext, ClrWhite);
    Graphics_setForegroundColor(&g_sContext, ClrBlack);

    // Set the default font for strings
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);

    //Clear the screen
    Graphics_clearDisplay(&g_sContext);
    ////////////////////////////////////////////////////////////////////////////////////////////

    Initialize_ADC();

    initGame();
    __enable_interrupt();

    __low_power_mode_0();
}

/*******************INTERRUPTS********************/
#pragma vector = TIMER0_A0_VECTOR 
__interrupt void nextFrame(void) {
    int status = handleCollision();
    if (status == 1) {
        drawRect(snake.front->pos, ClrFireBrick);
        __delay_cycles(20000000);
        freeSnake();
        drawRect(apple.pos, ClrWhite);
        initGame();
        return;
    }
	
    if (snake.front != NULL && isColliding(snake.front->pos, apple.pos)) {
        foodEatenFlag = 1;
        placeApple(); 
    }
	
    setDir(getJoyAxis());
    moveSnake(foodEatenFlag);
    foodEatenFlag = 0; 

}

/*******************FUNCTION BODIES********************/

void Initialize_Clock_System() {
  // DCO frequency = 16 MHz
  // MCLK = fDCO/1 = 16 MHz
  // SMCLK = fDCO/1 = 16 MHz

  // Activate memory wait state
  FRCTL0 = FRCTLPW | NWAITS_1;    // Wait state=1
  CSCTL0 = CSKEY;
  // Set DCOFSEL to 4 (3-bit field)
  CSCTL1 &= ~DCOFSEL_7;
  CSCTL1 |= DCOFSEL_4;
  // Set DCORSEL to 1 (1-bit field)
  CSCTL1 |= DCORSEL;
  // Change the dividers to 0 (div by 1)
  CSCTL3 &= ~(DIVS2|DIVS1|DIVS0);    // DIVS=0 (3-bit)
  CSCTL3 &= ~(DIVM2|DIVM1|DIVM0);    // DIVM=0 (3-bit)
  CSCTL0_H = 0;

  config_ACLK_to_32KHz_crystal();
  return;
}


void config_ACLK_to_32KHz_crystal(){ 
    // By default, ACLK runs on LFMODCLK at 5MHz/128 = 39 KHz 
    // Reroute pins to LFXIN/LFXOUT functionality 
    PJSEL1 &= ~BIT4; 
    PJSEL0 |= BIT4; 
    // Wait until the oscillator fault flags remain cleared 
    CSCTL0 = CSKEY; // Unlock CS registers 
    do { 
        CSCTL5 &= ~LFXTOFFG; // Local fault flag 
        SFRIFG1 &= ~OFIFG; // Global fault flag 
    } while((CSCTL5 & LFXTOFFG) != 0); 
    CSCTL0_H = 0; // Lock CS registers 
    return; 
} 

void Initialize_ADC() {
    // Configure the pins to analog functionality
    // X-axis: A10/P9.2, for A10 (P9DIR=x, P9SEL1=1,P9SEL0=1)
    P9SEL1 |= BIT2;
    P9SEL0 |= BIT2;
    // Y-axis: A4/P8.7, for A4 (P8DIR = x, P8SEL1 = 1,P8SEL0 = 1)
    P8SEL1 |= BIT7;
    P8SEL0 |= BIT7;

    //configure ADC for accelerometer
    //X-axis: J3.23 (P8.4), Y-axis: J3.24 (P8.5), Z-axis: J3.25 (P8.6)
    P8SEL1 |= (BIT4 | BIT5 | BIT6);
    P8SEL0 |= (BIT4 | BIT5 | BIT6);

    // Turn on the ADC module
    ADC12CTL0 |= ADC12ON;
    // Turn off ENC (Enable Conversion) bit while modifying the configuration
    ADC12CTL0 &= ~ADC12ENC;
    //*************** ADC12CTL0 ***************
    // Set ADC12SHT0 (select the number of cycles that you computed)
    ADC12CTL0 |= ADC12SHT0_2 | ADC12MSC; //corresponds to 16
    //*************** ADC12CTL1 ***************
    // Set ADC12SHS (select ADC12SC bit as the trigger)
    // Set ADC12SHP bit
    // Set ADC12DIV (select the divider you determined)
    // Set ADC12SSEL (select MODOSC)
    ADC12CTL1 = ADC12SHS_0 | ADC12SHP | ADC12DIV_0 |
    ADC12SSEL_0 | ADC12CONSEQ_1;
    //*************** ADC12CTL2 ***************
    // Set ADC12RES (select 12-bit resolution)
    // Set ADC12DF (select unsigned binary format)
    ADC12CTL2 = ADC12RES_2;
    //*************** ADC12CTL3 ***************
    // Leave all fields at default values
    ADC12CTL3 &= ~ADC12CSTARTADD0;
    //*************** ADC12MCTL0 ***************
    // Set ADC12VRSEL (select VR+=AVCC, VR-=AVSS)
    // Set ADC12INCH (select channel A10)
    ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_10;
    // Turn on ENC (Enable Conversion) bit at the end of the configuration
    ADC12MCTL1 = ADC12VRSEL_0 | ADC12INCH_4;
    ADC12MCTL2 = ADC12VRSEL_0 | ADC12INCH_7;
    ADC12MCTL3 = ADC12VRSEL_0 | ADC12INCH_6;
    ADC12MCTL4 = ADC12VRSEL_0 | ADC12INCH_5 | ADC12EOS;
    ADC12CTL0 |= ADC12ENC;
    return;
}

void initSnake(void){
	int i;
	for(i = 2; i >= 0; i--){
        Vec2 startPos = {startX - (BLOCK_SIZE * i), startY};
		push(startPos);
	}
}

void push(Vec2 pos){
	SnakeNode *newNode = malloc(sizeof(SnakeNode));
	if(newNode == NULL) return;
	
	newNode->pos = pos;
	newNode->prev = NULL;

	if(snake.front == NULL){
		snake.front = newNode;
		snake.back = newNode;
        newNode->next = NULL;
	}

	else {
		snake.front->prev = newNode;
        newNode->next = snake.front;
	}

    snake.front = newNode;
	snake.length++;

    drawRect(snake.front->pos, ClrGreen);

}

void pop(void){
	if (snake.back == NULL) return;
	
	SnakeNode *temp = snake.back;

    if (temp->prev != NULL){
        temp->prev->next = NULL;
        snake.back = temp->prev;
    } else {
        snake.front = NULL;
        snake.back = NULL;
    }

    drawRect(temp->pos, ClrWhite);
	free(temp); 
	snake.length--;
}

void moveSnake(int foodFlag){
    if (snake.front == NULL) return;
    Vec2 next;
	next.x = snake.front->pos.x + (snake.direction.x * BLOCK_SIZE);
	next.y = snake.front->pos.y + (snake.direction.y * BLOCK_SIZE);
	
	push(next);
	if (!foodFlag) pop(); 
}

Graphics_Rectangle drawRect(Vec2 minVals, uint32_t color){
	Graphics_Rectangle rect;
	rect.xMin = minVals.x;
	rect.yMin = minVals.y;
	rect.xMax = minVals.x + (BLOCK_SIZE - 1);
	rect.yMax = minVals.y + (BLOCK_SIZE - 1);
	Graphics_setForegroundColor(&g_sContext, color);
	Graphics_fillRectangle(&g_sContext, &rect);
	return rect;
}

int handleCollision(void){
	if (snake.front == NULL) return 0;

    //check if snake is colliding with screen boundaries
	if (
		snake.front->pos.x < 0 || 
		snake.front->pos.x > (SCREEN_MAX - BLOCK_SIZE) || 
		snake.front->pos.y < 0 || 
		snake.front->pos.y > (SCREEN_MAX - BLOCK_SIZE)
	)   return 1;

    //check if snake is colliding with self
	SnakeNode *walker = snake.back;
	while (walker != snake.front && walker != NULL){
		if (isColliding(snake.front->pos, walker->pos)){
			return 1; 
		}
		walker = walker->next;
	}
	return 0;
}

bool isColliding(Vec2 objectA, Vec2 objectB){
    return compareVec2(objectA, objectB);
}

void freeSnake(void){
	while (snake.back != NULL) {
		pop();
	}
}

void placeApple(void) {
    Vec2 wishPos;
    int collision;
    SnakeNode *walker;
    do {
        collision = 0; 
        wishPos.x = (rand() % 15) * BLOCK_SIZE - 1; 
        wishPos.y = (rand() % 15) * BLOCK_SIZE - 1;

        walker = snake.front;
        while (walker != NULL) {
            if (isColliding(wishPos, walker->pos)) {
                collision = 1; 
                break;         
            }
            walker = walker->next; 
        }
    } while (collision); 
    apple.pos = wishPos;
    drawRect(apple.pos, GRAPHICS_COLOR_RED);
}

void setDir(Vec2 wishDir){
    //check for zero
    if (compareVec2(wishDir, ZERO)){
        return;
    }
    //check if input is not in the opposite direction
    int upDownCheck = !((compareVec2(wishDir, UP) && compareVec2(snake.direction, DOWN)) || compareVec2(wishDir, DOWN) && compareVec2(snake.direction, UP));

    int leftRightCheck = !((compareVec2(wishDir, LEFT) && compareVec2(snake.direction, RIGHT)) || compareVec2(wishDir, RIGHT) && compareVec2(snake.direction, LEFT));

    if (!upDownCheck || !leftRightCheck){
        return;
    }

    else {
        snake.direction = wishDir;
    }
}

Vec2 getJoyAxis(){
    ADC12CTL0 |= ADC12SC;
    //await clear
    while(ADC12BUSY & ADC12CTL1){}

    //read values from memory
    Vec2 axis;
    axis.x = ADC12MEM0;
    axis.y = ADC12MEM1;

    //check deadzones and convert values to either -1, 0 or 1
    if (axis.x < DEADZONE_LOW){
        axis = LEFT;
    } else if (axis.x > DEADZONE_HIGH){
        axis = RIGHT;
    } else if (axis.y < DEADZONE_LOW){
        axis = UP;
    } else if (axis.y > DEADZONE_HIGH) {
        axis = DOWN;
    } else {
        axis = ZERO;
    }

    return axis;
}

bool compareVec2(Vec2 vecA, Vec2 vecB){
    return (vecA.x == vecB.x && vecA.y == vecB.y);
}

void initGame(){
    snake.direction = RIGHT;
    initSnake();
    placeApple();
}