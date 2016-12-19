/*----------------------------------------------------------------------------
 *
 * Name:    Jacob Violet & Jon Wesner
 * Purpose: Tic-Tac-Toe Game
 * Note(s): Joystick is used to control game
 *
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *      INCLUDES
 *---------------------------------------------------------------------------*/
#include <stdio.h>
#include <math.h>
#include "LPC17xx.H"                         
#include "GLCD.h"
#include "Serial.h"
#include "LED.h"
#include "ADC.h"
#include "mcb1700_joystick.h"
#include <stdbool.h>
#include "cmsis_os.h"
#include "LPC17xx.h"
#include "lpc17xx.h"

/* Font index 16x24               */
#define __FI        1                       

/*----------------------------------------------------------------------------
 *      Global Declarations
 *---------------------------------------------------------------------------*/
 uint8_t display;
 uint8_t rowIndex = 1;
 uint8_t columnIndex = 1;
 uint8_t previousRowIndex = 1;
 uint8_t previousColumnIndex = 1;
 uint8_t userTimer[] = "9";
 // 0 for X and 1 for O
 uint8_t player = 0;  
 uint8_t reset = 0;
 // Keeps track of game status -- used by both threads
 uint8_t gameOver = 0;															
 uint8_t playerWent = 0;
 // Simply write a sound value into bits 15:6 of DAC
 uint8_t soundVictory = 0;

/*----------------------------------------------------------------------------
 *      Thread Declaration
 *---------------------------------------------------------------------------*/
// Thread id of task: phase_a
osThreadId tid_timer;                  
osThreadId tid_joystick;
osThreadId tid_sound;


/*----------------------------------------------------------------------------
 *      Semaphore Declaration
 *---------------------------------------------------------------------------*/
osSemaphoreId joystickSem;
osSemaphoreDef(joystickSem);
osSemaphoreId timerSem;                         
osSemaphoreDef(timerSem);
osSemaphoreId soundSem;
osSemaphoreDef (soundSem);

// --------------------------------------------------------------------------//
//
//  Function Name:    clearBoard()
//	Parameters:				Takes the game board, the board row of LCD and 
//										board column of LCD.
//	Function Purpose: Clears board for reset button.
// 	Date Created:			December 01, 2016
//
// --------------------------------------------------------------------------//
void clearBoard(char board[3][3],uint8_t boardRow[3],uint8_t boardColumn[3]){
		uint8_t i = 0;
		uint8_t j = 0;
		
		// clear board column
		for(i = 0; i < 3; i++){
				// clear board row
				for(j = 0; j < 3; j++){
						GLCD_DisplayString(boardRow[j],  boardColumn[i], __FI, "  ");
						board[j][i] = '0';
				}
		}
}


// --------------------------------------------------------------------------//
//
//  Function Name:    displayCursor()
//	Parameters:				Takes current position and previous position.
//	Function Purpose: Display cursor to player on LCD so they know where they
//										are at on the board.
// 	Date Created:			December 03, 2016
//
// --------------------------------------------------------------------------//
void displayCursor(uint8_t currentPosition[2], uint8_t previousPosition[2]){
		// Add cursor to current position but don't remove display of X or O
		GLCD_DisplayString(currentPosition[0],  currentPosition[1]-1, __FI, "+");
		GLCD_DisplayString(previousPosition[0],  previousPosition[1]-1, __FI, " ");
}


// --------------------------------------------------------------------------//
//
//  Function Name:    checkForWinner()
//	Parameters:				Takes a matrix of the game board and returns either a 
//										winner (-1 or 1) or no winner (0).
//	Function Purpose: Determine a winner of the game by checking all possible
//										combinations of victory
// 	Date Created:			December 01, 2016
//
// --------------------------------------------------------------------------//
int checkForWinner(char board[3][3])
{
    int count = 0;
    int row, col;

    // Check each of 3 rows:
    for(row = 0; row < 3; ++row)
    {
        count = 0;
        for(col=0; col < 3; ++col)
        {
            count += (board[row][col] == 'X')?  1 :
                     (board[row][col] == 'O')? -1 : 0;
        }
        if (count == 3 || count == -3)
        {
						// Return either 1 or -1
            return count / abs(count); 
        }
    }

    // Check each of 3 columns.
    for(col = 0; col < 3; ++col)
    {
        count = 0;
        for(row=0; row < 3; ++row)
        {
            count += (board[row][col] == 'X')?  1 :
                     (board[row][col] == 'O')? -1 : 0;
        }
        if (count == 3 || count == -3)
        {
						// Return either 1 or -1
            return count / abs(count); 
        }
    }

    // Check Left-to-Right downward Diagonal:
    count = 0;
    for(col = 0; col < 3; ++col)
    {
        count += (board[col][col] == 'X')?  1 :
                 (board[col][col] == 'O')? -1 : 0;
    }
    if (count == 3 || count == -3)
    {
				// Return either 1 or -1
        return count / abs(count); 
    }

    // Check Left-to-Right upward Diagonal
    count = 0;
    for(col = 0; col < 3; ++col)
    {
        count += (board[col][2-col] == 'X')?  1 :
                 (board[col][2-col] == 'O')? -1 : 0;
    }
    if (count == 3 || count == -3)
    {
				// Return either 1 or -1
        return count / abs(count); 
    }

    return 0;
}


// --------------------------------------------------------------------------//
//
//  Function Name:    hex2bcd()
//	Parameters:				A unsigned char text
//	Function Purpose: Used to format timer for output to LCD
// 	Date Created:			November 29, 2016
//
// --------------------------------------------------------------------------//
unsigned char hex2bcd (unsigned char x)
{
    unsigned char y;
    y = (x / 10) << 4;
    y = y | (x % 10);
    return (y);
}


// ------------------------------THREAD 1------------------------------------//
//
//  Thread Name:    	timer()
//	Parameters:				
//	Function Purpose: Used to display game timer on the upper left hand side
//										of LCD and used to output player time on upper right
//										hand side of LCD
// 	Date Created:			December 01, 2016
//
// --------------------------------------------------------------------------//
void timer (void const *argument) {
	
	//-------------------------------------------------------------------------//
	//  LOCAL VARIABLES
	//-------------------------------------------------------------------------//
	/* Used for displaying time */
	char displayGameTimer[10];												
	uint8_t sec=00;
  uint8_t min=00;
	
	//-------------------------------------------------------------------------//
	//  TIMER LOOP
	//-------------------------------------------------------------------------//
  for (;;) {
    osSemaphoreWait (timerSem, osWaitForever);
		
		//********************************************//
		//  CHECK RESET BUTTON 
		//********************************************//
		/* Reset is set in the joystick loop*/
		if(reset){
			// reset min
			min = 00;
			// reset sec
			sec = 00;
			// reset player timer
			userTimer[0] = '9';
			// Output player turn and clear player winner on bottom of LCD
			GLCD_DisplayString(1,  9, __FI,  "X's Turn:");
			GLCD_DisplayString(9,  2, __FI, "              ");
			// Set 'X' as default starting player
			player = 0;
			// Set reset back to 0 until reset button pushed again
			reset = 0;
			// Ensure gameOver is set back to 0 if the game was over
			gameOver = 0;
			soundVictory = 0;
		}
		
		//********************************************//
		//  INCREMENT TIMERS
		//********************************************//
		if(gameOver == 0){
				sec++;
				userTimer[0] = userTimer[0] - 1;
				
				if(userTimer[0] == '0' || playerWent){
					userTimer[0] = '9';
					playerWent = 0;
					// Change player
					if(player == 1){
						// player 'X'
						player = 0;
						GLCD_DisplayString(1,  9, __FI,  "X's Turn:");
					}
					else{
						// player 'O'
						player = 1;
						GLCD_DisplayString(1,  9, __FI,  "O's Turn:");
					}
				}			
				if (sec==60){	
					 sec=0;
					 min++;
				}
				
				//********************************************//
				//  DISPLAY TIMERS ON LCD
				//********************************************//
				GLCD_DisplayString(1,  18, __FI,  userTimer);
				// Print out seconds
				sprintf(displayGameTimer, "%02X", hex2bcd(sec));       
				GLCD_DisplayString(1,  3, __FI,  (unsigned char *)displayGameTimer);
				// print minutes
				sprintf(displayGameTimer, "%02X", hex2bcd(min));       
				GLCD_DisplayString(1,  2, __FI, ":");
				GLCD_DisplayString(1,  0, __FI,  (unsigned char *)displayGameTimer);
		}
		
		//********************************************//
		//  RELEASE LCD TO JOYSTICK THREAD
		//********************************************//
    osSemaphoreRelease (joystickSem);   						
		osDelay(8000);																	
	}																										
}																										

// ------------------------------THREAD 2------------------------------------//
//
//  Thread Name:    	joystick()
//	Parameters:				
//	Function Purpose: Used to handle the joystick funcionality and output 
//										player placements on board
// 	Date Created:			December 01, 2016
//
// --------------------------------------------------------------------------//
void joystick (void  const *argument) {
	//-------------------------------------------------------------------------//
	// 	JOYSTICK DEFINITIONS
	//-------------------------------------------------------------------------//
	#define JOY_POS_DOWN     (1<<3)		
  #define JOY_POS_RIGHT	 	 (1<<4)
  #define JOY_POS_UP	 		 (1<<5)
  #define JOY_POS_LEFT     (1<<6)
	#define JOY_POS_CENTER   (1<<0)

	//-------------------------------------------------------------------------//
	//  LOCAL VARIABLES
	//-------------------------------------------------------------------------//
	// Used for reset button
	uint8_t int0_val;																	
  char board[3][3]={{'0','0','0'},{'0','0','0'},{'0','0','0'}};		
	// Used for cursor	
	uint8_t currentPosition[2] = {5,9};		
	// Used for cursor
	uint8_t previousPosition[2] = {3,5};				
	// Used to output winner										
	int winner;
	// Board row pos. on LCD
	uint8_t boardRow[3] = {3,5,7};		
	// Board column pos. on LCD
  uint8_t boardColumn[3] = {5,9,13};								

	//-------------------------------------------------------------------------//
	//  CONTROL LOOP FOR JOYSTICK
	//-------------------------------------------------------------------------//
	for(;;){
		osSemaphoreWait (joystickSem, osWaitForever);  //wait for joystick release

		//********************************************//
		//  CHECK RESET BUTTON
		//********************************************//
		/* Get value of reset button */
		int0_val = ~(LPC_GPIO2->FIOPIN >> 10) & 0x01;
		
		// Check if reset button pressed
		if(int0_val == 1){
			// Reset Board
			clearBoard(board, boardRow, boardColumn);
			// Reset Flag for timer
			reset = 1;
		}
		
		//********************************************//
		//  GET JOYSTICK VALUE
		//********************************************//
		/* Continue if game is over */
		if(gameOver == 0){
			
			//******************************************//
			//  CHECK JOYSTICK UP
			//******************************************//
			if(JoyPosGet() == JOY_POS_UP){
				// Save previous position for moving of cursor
				previousPosition[0] = boardRow[rowIndex];
				previousPosition[1] = boardColumn[columnIndex];
				previousRowIndex = rowIndex;
				
				// Logic for row index on board
				if(rowIndex==2)
					rowIndex=2;
				else
					rowIndex++;	
				
				// Current position in game
				currentPosition[0] = boardRow[rowIndex];
				currentPosition[1] = boardColumn[columnIndex];
			}
			
			//******************************************//
			//  CHECK JOYSTICK DOWN
			//******************************************//
			if(JoyPosGet() == JOY_POS_DOWN){
				// Save previous position for moving of cursor
				previousPosition[0] = boardRow[rowIndex];
				previousPosition[1] = boardColumn[columnIndex];
				previousRowIndex = rowIndex;
				
				// Logic for row index on board
				if(rowIndex==0)
					rowIndex=0;
				else
					rowIndex=rowIndex-1;
				
				// Current position in game
				currentPosition[0] = boardRow[rowIndex];
				currentPosition[1] = boardColumn[columnIndex];
			}

			//******************************************//
			//  CHECK JOYSTICK LEFT
			//******************************************//
			if(JoyPosGet() == JOY_POS_LEFT){
				// Save previous position for moving of cursor
				previousPosition[0] = boardRow[rowIndex];
				previousPosition[1] = boardColumn[columnIndex];
				previousColumnIndex = columnIndex;
				
				// Logic for column index on board
				if(columnIndex==0)
					columnIndex=0;
				else
					columnIndex=columnIndex-1;	
				
				// Current position in game
				currentPosition[0] = boardRow[rowIndex];
				currentPosition[1] = boardColumn[columnIndex];
			}
		
			//******************************************//
			//  CHECK JOYSTICK RIGHT
			//******************************************//
			if(JoyPosGet() == JOY_POS_RIGHT){
				// Save previous position for moving of cursor
				previousPosition[0] = boardRow[rowIndex];
				previousPosition[1] = boardColumn[columnIndex];
				previousColumnIndex = columnIndex;
				
				// Logic for column index on board
				if(columnIndex==2)
					columnIndex=2;
				else
					columnIndex++;		
				
				// Current position in game
				currentPosition[0] = boardRow[rowIndex];
				currentPosition[1] = boardColumn[columnIndex];
			}
			
			//******************************************//
			//  CHECK JOYSTICK CENTER
			//******************************************//
			if(JoyPosGet() == JOY_POS_CENTER){
				// Ensures that player's cannot overwrite whats already there
				if((board[rowIndex][columnIndex] == 'X') || (board[rowIndex][columnIndex] == 'O')){
				}
				// Place either 'X' or 'O' on board 
				else{
					if((player == 0) && (playerWent == 0)){
						board[rowIndex][columnIndex] = 'X';
						playerWent = 1;
						display=1;
					}
					if((player == 1) && (playerWent == 0)){
						board[rowIndex][columnIndex] = 'O';
						playerWent = 1;
						display=1;
					}
				}
			}
			
			//******************************************//
			//  CHECK FOR WINNER
			//******************************************//
			winner = checkForWinner(board);
		} 																										 		
		
		//********************************************//
		//  DISPLAY PLAYER CHOICE ON BOARD
		//********************************************//
		if(display==1){
			if(player==0)
				GLCD_DisplayString(boardRow[rowIndex],  boardColumn[columnIndex], __FI,  "X");
			else
				GLCD_DisplayString(boardRow[rowIndex],  boardColumn[columnIndex], __FI,  "O");
			// Reset display to 0 until a player chooses another spot
			display=0;
		}
		
		//********************************************//
		//  UPDATE AND DISPLAY CURSOR ON BOARD
		//********************************************//
		displayCursor(currentPosition, previousPosition);
		
		//********************************************//
		//  CHECK IF GAME IS OVER
		//********************************************//
		/* 1 = 'X' player wins */
		if(winner==1) {
			GLCD_DisplayString(9,  2, __FI, " Player X Won!");
			// Output victory sound to speaker using DAC
			if(soundVictory == 0){
				soundVictory = 1;
				osSemaphoreRelease (soundSem);
			}
			// Set game to be over to disablecontrol loops for
			// both timer and joystick
			gameOver = 1;
		}
		// -1 = 'O' player wins
		if(winner == -1){
			GLCD_DisplayString(9,  2, __FI, " Player O Won!");
			// Output victory sound to speaker using DAC
			if(soundVictory == 0){
				soundVictory = 1;
				osSemaphoreRelease (soundSem);
			}
			// Set game to be over to disablecontrol loops for
			// both timer and joystick
			gameOver = 1;
		} 
		
		//********************************************//
		//  RELEASE LCD TO TIMER THREAD
		//********************************************//
		osSemaphoreRelease (timerSem);
		
		// Debouncer
		osDelay(50); 																			  
	}																											
}																												


// ------------------------------THREAD 3------------------------------------//
//
//  Thread Name:    	sound()
//	Parameters:				
//	Function Purpose: Used to handle the sound output on game victory
// 	Date Created:			December 07, 2016
//
// --------------------------------------------------------------------------//
void sound (void  const *argument) {
	
	//-------------------------------------------------------------------------//
	//  LOCAL VARIABLES
	//-------------------------------------------------------------------------//
	int16_t i = 0;
	int16_t j = 0;
	int16_t frequencyTable[6] = {15,0,15,0,10,10};
	int16_t incrementFrequency = 0;
	
	//-------------------------------------------------------------------------//
	//  CONTROL LOOP FOR Sound
	//-------------------------------------------------------------------------//
	for(;;){
		osSemaphoreWait (soundSem, osWaitForever);  //wait for joystick release
		incrementFrequency = 0;
		j = 0;
		for(i=0;i<=300;i++){
				// fluctiate frequency
				j++;
				if(j == 50){
					incrementFrequency++;
					j = 0;
				}
				LPC_DAC->DACR = (125 & 0x3FF) << 6;  //write to port to trigger sound
				osDelay(frequencyTable[incrementFrequency]);
				LPC_DAC->DACR = 0; //write different value to port to change sound
				osDelay(frequencyTable[incrementFrequency]);
		}
	}
}

/*----------------------------------------------------------------------------
 *      Thread Definitions
 *---------------------------------------------------------------------------*/
osThreadDef(timer, osPriorityNormal, 1, 0);
osThreadDef(joystick,  osPriorityNormal, 1, 0);
osThreadDef(sound,  osPriorityNormal, 1, 0);


// ------------------------------Main----------------------------------------//
//
//  Thread Name:    	main()
//	Parameters:				
//	Function Purpose: Starts Thread loops
// 	Date Created:			November 29, 2016
//
// --------------------------------------------------------------------------//
int main (void) {
	
 /*-------------------------------------------------------------
 *      INITIALIZE INTO BUTTON FOR RESET
 *------------------------------------------------------------*/
 // Initialize the INT0 button, which will be used to restart the game
 LPC_PINCON->PINSEL4 &= ~(3<<20); // P2.10 is GPIO 
 LPC_GPIO2->FIODIR &= ~(1<<10); // P2.10 is input	

 /*-------------------------------------------------------------
 *      INITIALIZE DAC FOR OUTPUT OF SOUND
 *------------------------------------------------------------*/
 LPC_PINCON->PINSEL1 &= ~(3<<20);
 LPC_PINCON->PINSEL1 |= (2<<20);
 LPC_PINCON->PINMODE1 &= ~(3<<20);
 LPC_PINCON->PINMODE1 |= (2<<20); 

 /*--------------------------------------------------------------
 *      INITIALIZE LCD/LED/SER/ADC
 *-------------------------------------------------------------*/
	#ifdef __USE_LCD
  GLCD_Init();                               
	LED_Init();			
	SER_Init();
	ADC_Init();
	
 /*-------------------------------------------------------------
 *      DISPLAY TIC-TAC-TOE BOARD TO LCD
 *------------------------------------------------------------*/
  GLCD_Clear(White);                         
  GLCD_SetBackColor(Blue);
  GLCD_SetTextColor(White);
	GLCD_DisplayString(0, 0, __FI, "     TIC-TAC-TOE       ");
	GLCD_SetBackColor(White);
  GLCD_SetTextColor(Blue);
	GLCD_DisplayString (2, 7, __FI, "|"); 
	GLCD_DisplayString (3, 7, __FI, "|"); 
	GLCD_DisplayString (5, 7, __FI, "|"); 
	GLCD_DisplayString (7, 7, __FI, "|"); 
	GLCD_DisplayString (8, 7, __FI, "|"); 
	GLCD_DisplayString (4, 3, __FI, " -----------");
	GLCD_DisplayString (6, 3, __FI, " -----------"); 
	GLCD_DisplayString (2, 11, __FI, "|"); 
	GLCD_DisplayString (3, 11, __FI, "|"); 
	GLCD_DisplayString (5, 11, __FI, "|"); 
	GLCD_DisplayString (7, 11, __FI, "|"); 
	GLCD_DisplayString (8, 11, __FI, "|"); 
	GLCD_DisplayString(1,  9, __FI,  "X's Turn:");
	#endif

 /*-------------------------------------------------------------
 *      THREAD CREATION
 *------------------------------------------------------------*/
  tid_timer = osThreadCreate(osThread(timer), NULL);
	tid_joystick = osThreadCreate(osThread(joystick),  NULL);
	tid_sound = osThreadCreate(osThread(sound),  NULL);

 /*-------------------------------------------------------------
 *      SEMEPHORE CREATION
 *------------------------------------------------------------*/
	joystickSem = osSemaphoreCreate(osSemaphore(joystickSem), 0);                        
	timerSem = osSemaphoreCreate(osSemaphore(timerSem), 0);
	soundSem = osSemaphoreCreate(osSemaphore(soundSem), 0);
	
 /*-------------------------------------------------------------
 *      START THREADS
 *------------------------------------------------------------*/
	osSemaphoreRelease (timerSem);
	
  osDelay(osWaitForever);
  while(1);
}

