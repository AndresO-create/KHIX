#include <msp430.h> 
#include <stdlib.h>
#include "GrLib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"

/*******************CONSTANTS********************/

#define startX 60 //starting x positon of the snake
#define startY 60 //starting y position of snake
#define BLOCK_SIZE 6 //120x120 screen size meaning that this is approx 5% of the screen
#define FRAME_TIME 1066 //approx 1/30th of a second meaning that the game will run at 30FPS

// Track absolute center reference thresholds from the hardware joystick pots
#define JOY_THRESHOLD_LOW  1500  // Less than this value means LEFT or DOWN
#define JOY_THRESHOLD_HIGH 2500  // Greater than this value means RIGHT or UP

/*******************STRUCTS********************/

//Snake uses a linked list to track each part of the snake's body
typedef struct SnakeNode {
	int x;
	int y;
	struct SnakeNode *next;
} SnakeNode;

//snake body implements linked list using a queue and tracks head and tail nodes alongside current direction and length of snake
typedef struct SnakeGame{
	SnakeNode *head; //head is technically the back of the queue
	SnakeNode *tail; //tail is technically the front of queue
	int dirX, dirY, length;
} SnakeGame;

//apple increments score by 1 if snake collides with it
typedef struct Apple{
	int x, y;
} Apple;

/*******************GLOBALS********************/

//global variable tracking snake queue
SnakeGame snake;

//graphics context
Graphics_Context g_sContext;

//configures grlib directly to lcd screen
extern const Graphics_Display g_sCrystalfontz128x128; 

Apple apple;

//tracks if the snake ran into an apple on the previous frame
int foodEatenFlag = 0;

//tracks if the game should continue or no
int gameOver = 0;

// Buffer to store conversion results: index 0 = X-axis, index 1 = Y-axis
volatile uint16_t adcResult[2]; 

/*******************FUNCTION PROTOTYPES********************/
//initializes snake body
void initSnake();

//frees up memory of snake List
void freeSnake(void);

//enqueues snake into body
void enqueue(SnakeNode *snakePart);

//dequeues the first node in the queue
void dequeue();

//creates a new snakeNode and assigns its position using the head of the queue
SnakeNode *addSnake(int posX, int posY);


//increments snake position each frame and checks if snake is eating an apple
void moveSnake(int foodFlag);

//checks if snake is colliding with a wall
int handleCollision(void);

//draws a new rectangle on the screenG
Graphics_Rectangle drawRect(int xMin, int yMin, uint32_t color);

//randomly assigns position of apple and checks if it is in the same position as the snake
void placeApple();

//configures timer to 32KHz
void config_ACLK_to_32KHz_crystal();

void init_ADC(void);
void read_Joystick(void);

//clears all previous graphics from the screen and begins drawing
void init_graphics(void);

/*******************MAIN LOOP********************/

int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer

	// Disable the GPIO high-impedance mode to activate physical pin drivers
	PM5CTL0 &= ~LOCKLPM5;
	
	// Configure ACLK and Timer_A
	config_ACLK_to_32KHz_crystal();

	// The screen communicates over UCA1 SPI interface (Pins P1.4, P1.5)
	P1SEL0 |= BIT4 | BIT5;  // Configure pins for SPI clock and data lines
	P1SEL1 &= ~(BIT4 | BIT5); 

	// The LCD Control lines (Data/Command and Reset) map to generic GPIOs
	P3DIR |= BIT7;  // DC Line (example pin - confirm via your lcd_driver.h sheet)
	P4DIR |= BIT3;  // RESET Line 

	//initialize game
	init_graphics();
	initSnake();
	placeApple();
	init_ADC();
	

	// Configure Timer_A0 matching constraints correctly
	TA0CCR0 = FRAME_TIME;             // Set frame countdown interval step window
	TA0CCTL0 = CCIE;                  // Enable local Capture/Compare interrupts for CCR0 Channel
	TA0CTL = TASSEL_1 | MC_1 | TACLR; // ACLK source, Up-Mode operation, clear tracking counters

	// Main execution loop: handles reset sweeps outside time critical ISR windows
	while(1)
	{
		// Fall completely asleep inside Low-Power Mode 3 with active Global Interrupts enabled
		__bis_SR_register(LPM3_bits | GIE); 

		//check for gameover
		if (gameOver) {
			__bis_SR_register(GIE);
			//Clear screen and draw a quick "Game Over" message
			Graphics_clearDisplay(&g_sContext);
			Graphics_drawStringCentered(&g_sContext, (int8_t *)"GAME OVER", AUTO_STRING_LENGTH, 60, 60, TRANSPARENT_TEXT);
			freeSnake();
			initSnake();
			placeApple();
			gameOver = 0; // Clear reset tracking states

			__bis_SR_register(GIE);
		}
	}
}

/*******************INTERRUPTS********************/
#pragma vector = TIMER0_A0_VECTOR // Link the ISR to the vector
    __interrupt void nextFrame() {
		//read current input
		read_Joystick();

		//implement game loop here
        
		//check if snake is colliding with something
		int status = handleCollision();
		if(status) gameOver = 1;
		
		// Evaluate if our head position steps straight onto food targets
		if (snake.head->x == apple.x && snake.head->y == apple.y) {
			foodEatenFlag = 1;
			placeApple(); // Respawn apple
		}
		
		// Advance processing thread matrix step
		moveSnake(foodEatenFlag);
		foodEatenFlag = 0; // Reset consumption status tracking state

		if (gameOver) __bic_SR_register_on_exit(LPM3_bits);
}


/*******************FUNCTION BODIES********************/

//initializes snake body using DMA
void initSnake(){
	snake.head = NULL;
	snake.tail = NULL;
	snake.length = 0;
	snake.dirX = 1;
	snake.dirY = 0;

	int i;
	for(i = 2; i >= 0; i--){
		SnakeNode *tempNode = addSnake(startX - (BLOCK_SIZE * i), startY);
		enqueue(tempNode);
		free(tempNode);
	}
	snake.dirX = 1;
	snake.dirY = 0;
}

//enqueues snake into body
void enqueue(SnakeNode *snakePart){
	SnakeNode *newNode = (SnakeNode *)malloc(sizeof(SnakeNode));
	
	newNode->x = snakePart->x;
	newNode->y = snakePart->y;
	newNode->next = NULL;

	//append newNode to back of queue
	if(snake.tail == NULL){
		//if snake is first in queue
		snake.head = newNode;
		snake.tail = newNode;
	}
	else {
		//snake is not first in node
		snake.head->next = newNode;
		snake.head = newNode;
	}
	//increment snake length
	snake.length++;
}

void dequeue(){
	//check if snake is already empty
	if (snake.tail == NULL) return;
	
	SnakeNode *temp = snake.tail;

	Graphics_Rectangle eraseRectangle = drawRect(temp->x, temp->y, GRAPHICS_COLOR_BLUE);
	
	snake.tail = snake.tail->next; //go to next node in snake
	if (snake.tail == NULL){
		snake.head = NULL;
	}

	free(temp); //free tail of snake
	snake.length--;
}

SnakeNode *addSnake(int posX, int posY){
	SnakeNode *node = (SnakeNode *)malloc(sizeof(SnakeNode));
	
	//assign snake position
	node->x = posX;
	node->y = posY;
	
	return node;
}

void moveSnake(int foodFlag){
	//calculate position of snake head based off of current velocity direction
	int nextX = snake.head->x + (snake.dirX * BLOCK_SIZE);
	int nextY = snake.head->y + (snake.dirY * BLOCK_SIZE);
	
	enqueue(addSnake(nextX, nextY));

	Graphics_Rectangle headRect = drawRect(nextX, nextY, GRAPHICS_COLOR_WHITE);
	Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
	
	//coordinate cleanup management
	if (!foodFlag) dequeue(); //drop old tail degment
	
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
	//check if snake is out of bounds (based off of microcontroller extension's screen dimensions of 120x120 pixels)
	if (
		snake.head->x < 0 + BLOCK_SIZE || 
		snake.head->x > 120 - BLOCK_SIZE|| 
		snake.head->y < 0 + BLOCK_SIZE|| 
		snake.head->y > 120 - BLOCK_SIZE
	)   return 1;

	//iterate through snake list to see if snake is colliding with self
	SnakeNode *temp = snake.tail;
	while (temp != snake.head && temp != NULL){
		if (snake.head->x == temp->x && snake.head->y == temp->y){
			return 1; //collision with own tail
		}
		if (apple.x == temp->x && apple.y == snake.head->y){
			return -1; //collision with apple
		}
		temp = temp->next;
	}
	return 0;
}


void freeSnake(void){
	while (snake.tail != NULL){
		dequeue();
		
	}
}

void placeApple(void) {
    int wishX, wishY;
    int collision;
    SnakeNode *temp;

    do {
        collision = 0; // Reset tracking flag for each check run
        
        // Pick a random slot number on the grid, 
        // then multiply by BLOCK_SIZE to snap coordinates onto your 6px matrix paths.
        // Total slots = 120 / 6 = 20 total blocks (Indices 0 through 19)
        wishX = (rand() % 20) * BLOCK_SIZE; 
        wishY = (rand() % 20) * BLOCK_SIZE;

        // 2. Scan the body chain starting at the trailing tail segment
        temp = snake.tail;
        while (temp != NULL) {
            if (wishX == temp->x && wishY == temp->y) {
                collision = 1; // Apple picked a spot inside snake
                break;         // Halt checking to pick a clean coordinate match
            }
            temp = temp->next; // Advance the pointer to prevent a freeze!
        }
        
    } while (collision); // Loop wraps automatically without nested recursion risk

    // 3. Assign safe, grid-aligned coordinates
    apple.x = wishX;
    apple.y = wishY;
    
    // Draw the red apple block onto your display panel matrix layout
    drawRect(apple.x, apple.y, GRAPHICS_COLOR_RED);
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

void init_ADC(void) {
    // 1. Reroute physical pins P1.6 and P1.7 to function as secondary ADC inputs
    P1SEL0 |= BIT6 | BIT7;
    P1SEL1 |= BIT1; // Note: Check device pin mapping sheet if channel tracks differently

    // 2. Configure core conversion properties
    ADC12CTL0 = ADC12SHT0_4 | ADC12ON;       // Sample-and-hold time, turn ADC12 module ON
    ADC12CTL1 = ADC12SHP | ADC12CONSEQ_1;    // Source from sampling timer, sequence-of-channels mode
    
    // 3. Map Memory Control Registers to look up Analog Input Channels
    ADC12MCTL0 = ADC12INCH_6;                 // Store Channel A6 (X-axis) in ADC12MEM0
    ADC12MCTL1 = ADC12INCH_7 | ADC12EOS;     // Store Channel A7 (Y-axis) in ADC12MEM1, declare End-of-Sequence
}

void read_Joystick(void) {
    ADC12CTL0 |= ADC12ENC | ADC12SC;         // Enable conversion and initiate Sample Conversion
    
    // Wait actively for the hardware sequencer to finish filling the conversion buffers
    while(ADC12CTL1 & ADC12BUSY);
    
    adcResult[0] = ADC12MEM0; // Raw value between 0 and 4095 for X
    adcResult[1] = ADC12MEM1; // Raw value between 0 and 4095 for Y

    // --- MAP POSITION TRANSLATIONS TO GAME DIRECTIONS ---
    // Handle X-Axis (Horizontal)
    if (adcResult[0] < JOY_THRESHOLD_LOW && snake.dirX == 0) {
        snake.dirX = -1; // Move Left
        snake.dirY = 0;
    }
    else if (adcResult[0] > JOY_THRESHOLD_HIGH && snake.dirX == 0) {
        snake.dirX = 1;  // Move Right
        snake.dirY = 0;
    }
    
    // Handle Y-Axis (Vertical - inverted based on display driver mounting orientation)
    if (adcResult[1] < JOY_THRESHOLD_LOW && snake.dirY == 0) {
        snake.dirX = 0;
        snake.dirY = 1;  // Move Down
    }
    else if (adcResult[1] > JOY_THRESHOLD_HIGH && snake.dirY == 0) {
        snake.dirX = 0;
        snake.dirY = -1; // Move Up
    }
}

void init_graphics(void){
	P3DIR |= BIT6;  // Set P3.6 as an output pin
    P3OUT |= BIT6;  // Drive P3.6 HIGH to turn on the screen backlight array
	P2DIR &= ~BIT6; //turn on backlight
	P2OUT &= ~BIT6;
	
	__delay_cycles(400000);

	//initialize the physical controller chips on the boosterpack
	Crystalfontz128x128_Init();
	
	//set LCD orientation to upright
	Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP);

	//Bind the generic g_sContext structure to your specific physical display driver
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);

    //Configure your starting game design theme colors
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLUE);  // Game backdrop canvas color
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE); // Snake draw color

    //Establish your text style mapping for drawing scores or game-over splash screens
    Graphics_setFont(&g_sContext, &g_sFontFixed6x8);

    //Flush the entire display panel to your background color to wipe loose startup artifacts
    Graphics_clearDisplay(&g_sContext);
}