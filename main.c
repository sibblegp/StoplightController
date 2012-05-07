/*

	EE 276
	Traffic Light Project
	Program File
	
	George Sibble
	Lauren Mitchell
	Sean Begley
	
	
	The purpose of this code is to create a working traffic light 
	simulated on the HC12 controller.  The project includes the 
	ability to react to input from cars waiting to turn and 
	ambulances approaching the light.
*/

/******************************************************
			INCLUDES
******************************************************/

#include "includes.h"		//HC12 Board and Ucos Includes

/******************************************************
			DEFINITIONS
******************************************************/

//Standard task stack size
#define TASK_STK_SIZE   1024

//Light bits
//These are the bits designating the individual light status
//0 = RED
//1 = GREEN

#define LIGHT_NORTH 	8
#define LIGHT_SOUTH 	4
#define LIGHT_EAST 	2
#define LIGHT_WEST	1

#define TURN_NORTH	128
#define TURN_SOUTH	64
#define TURN_EAST	32
#define TURN_WEST	16

#define WALK_NS	1
#define WALK_EW	2

//Exact States
//These are states defined by the sum of the status of several lights

//All Reds
#define ALL_STOP	0

//Green in both directions
#define NS_GO		LIGHT_NORTH + LIGHT_SOUTH
#define EW_GO		LIGHT_EAST + LIGHT_WEST

//Green for both directions to turn
#define NS_TURN		TURN_NORTH + TURN_SOUTH
#define EW_TURN	TURN_EAST + TURN_WEST

//Green to turn and go straight for one direction
#define N_TURN		LIGHT_NORTH + TURN_NORTH
#define S_TURN		LIGHT_SOUTH + TURN_SOUTH
#define E_TURN		LIGHT_EAST + TURN_EAST
#define W_TURN		LIGHT_WEST + TURN_WEST

//FLAGS
//These flags correspond to the input pins for the sensors

//Cars waiting to turn pins
#define NORTH_TURN_FLAG 1			//Pin 1
#define SOUTH_TURN_FLAG 2		//Pin 2
#define EAST_TURN_FLAG 4			//Pin 3
#define WEST_TURN_FLAG 8			//Pin 4

#define NORTH_AMBULANCE_FLAG 16	//Pin 5
#define SOUTH_AMBULANCE_FLAG 32	//Pin 6
#define EAST_AMBULANCE_FLAG 64	//Pin 7
#define WEST_AMBULANCE_FLAG 128	//Pin 8

//LEDs
//These correspond to the pins attached to the ports with the specific lights

//Red lights
#define LED_NORTH_RED	2
#define LED_SOUTH_RED	32
#define LED_EAST_RED		2
#define LED_WEST_RED	32

//Yellow lights
#define LED_NORTH_YELLOW	1
#define LED_SOUTH_YELLOW	4
#define LED_EAST_YELLOW		16
#define LED_WEST_YELLOW		64

//North lights
#define LED_NORTH_GREEN		1
#define LED_SOUTH_GREEN		16
#define LED_EAST_GREEN		1
#define LED_WEST_GREEN		16

//Turn signal yellow lights
#define LED_NORTH_TURN_YELLOW	2
#define LED_SOUTH_TURN_YELLOW	8
#define LED_EAST_TURN_YELLOW	32
#define LED_WEST_TURN_YELLOW	128

//Turn signal green lights
#define LED_NORTH_TURN_GREEN	4
#define LED_SOUTH_TURN_GREEN	64
#define LED_EAST_TURN_GREEN		4
#define LED_WEST_TURN_GREEN		64

//Walk signal LEDs
#define LED_WALK_NS_WHITE		8+16
#define LED_WALK_EW_WHITE		128+32

//Unused red walk LEDs
#define LED_WALK_NS_RED
#define LED_WALK_EW_RED

/******************************************************
			TYPE DEFINITIONS
******************************************************/

//Lightflags type
//Used as a subtype to contant lightflags
//Could have used INT8U directly but better to use named type
typedef INT8U lightFlags;

//lightState type
//Contains state defining what lights are on and off
typedef struct{
	INT8U lstate;	//Light State
	INT8U astate;	//Ambulance State
} lightState;
//Lightstate contains the state of the lights
//Ambulance state actually only contains walk state


/******************************************************
			GLOBAL VARS
			(SO SUE ME)
******************************************************/

//cState
//Current State of the lights
lightState cState;

//cflags
//Current input flags
lightFlags cflags;

//Task Stacks
OS_STK  MainLightCycleStk[TASK_STK_SIZE];
OS_STK  ambulanceHandlerStk[TASK_STK_SIZE];
OS_STK  testLEDsStk[TASK_STK_SIZE];

//New ambulance main task priority variable
INT8U mainTaskPrio;


/******************************************************
			FUNCTION PROTOTYPES
******************************************************/

//Main:  Main system initializing function
int main();

//determineNextState:  Receives a state and determines the next appropriate one
lightState determineNextState(lightState currState);

//changeLighgts:  Changes the lights from their current state into the passed state
void changeLights(lightState nextState);

//initializeLights:  Initializes the LEDs to all red and sets up the ports
void initializeLights();

//checkSensors:  Sets the cflags variable to match the current input from the sensors
void checkSensors();

//printStatus:  Outputs the current status over the serial port in ASCII
void printStatus(lightState currState);  //TESTING FUNCTION


/******************************************************
			TASK PROTOTYPES
******************************************************/

//mainLightCycle
//Handles all of the main light timing and switching
void mainLightCycle(void* PDATA);

//ambulanceHandler
//Handles ambulance notifications
void ambulanceHandler(void* PDATA);


/******************************************************
			FUNCTION DEFINITIONS
******************************************************/


//determineNextState
//Receives a state and returns the next state the system should be in
lightState determineNextState(lightState currState)
{
	//Create a lightState variable for the next state
	lightState nextState;
	
	//Set the nextState variable to all 0s
	nextState.lstate = 0;
	nextState.astate = 0;
	
	//Check the sensors for any input (also sets flags)
	checkSensors();
	
	/***** SWITCH STATEMENT THEORY *****
	
	The theory behind this switch statement is to
	take in the current lightstate, check the status
	of the sensors, and determine the next appropriate
	state.  Each case is very similar for the most part.
	
	For the main states (EW_GO, NS_GO, ALL_STOP)
	-If both flags are set, go to the next all turn state
	-If one flag is set, go to that turn state
	-If no flags are set, go to the straight in both state
	
	For the turn states (N_TURN, etc.)
	-Switch to the both go state
	
	*Only the first example of each type is commented
		to prevent the code from being confusing
	
	******************************************/
	
	//Switch the current state
	switch (currState.lstate){
		
		//If current state is all stop
		case ALL_STOP:
			if(cflags & NORTH_TURN_FLAG && cflags & SOUTH_TURN_FLAG)
			//If both the north and south turn flags are set, set the next state to NS_TURN
				nextState.lstate = NS_TURN;
			else if(cflags & NORTH_TURN_FLAG)
				//If just the north turn flag is set
				nextState.lstate = N_TURN;
			else if(cflags & SOUTH_TURN_FLAG)
				//If just the south turn flag is set
				nextState.lstate = S_TURN;
			else
				//If neither turn flag is set
				nextState.lstate = NS_GO;
			break;
			
		case EW_GO:
			if(cflags & NORTH_TURN_FLAG && cflags & SOUTH_TURN_FLAG)
				nextState.lstate = NS_TURN;
			else if(cflags & NORTH_TURN_FLAG)
				nextState.lstate = N_TURN;
			else if(cflags & SOUTH_TURN_FLAG)
				nextState.lstate = S_TURN;
			else
				nextState.lstate = NS_GO;
			break;
			
		//If the current state is NS_TURN
		case NS_TURN:
			//Set the next state to go in both directions
			nextState.lstate = NS_GO;
			break;
		
		case N_TURN:
			nextState.lstate = NS_GO;
			break;
		
		case S_TURN:
			nextState.lstate = NS_GO;
			break;
		
		case NS_GO:
			if(cflags & EAST_TURN_FLAG && cflags & WEST_TURN_FLAG)
				nextState.lstate = EW_TURN;
			else if(cflags & EAST_TURN_FLAG)
				nextState.lstate = E_TURN;
			else if(cflags & WEST_TURN_FLAG)
				nextState.lstate = W_TURN;
			else
				nextState.lstate = EW_GO;
			break;

			
		case EW_TURN:
			nextState.lstate = EW_GO;
			break;
		
		case E_TURN:
			nextState.lstate = EW_GO;
			break;
		
		case W_TURN:
			nextState.lstate = EW_GO;
			break;
		
	}

	//If the next state is for traffic to proceed east and west
	if(nextState.lstate == EW_GO)
		//Allow people to walk east and west
		nextState.astate = WALK_EW;
	
	if(nextState.lstate == NS_GO)
		nextState.astate = WALK_NS;
		
	
	//Clear the flags
	cflags = 0;
	
	//Return the next state
	return nextState;
}


//initializeLights
//Initializes the port directions and sets lights to start condition
void initializeLights()
{
	//TURN ALL LEDs ON TO RED AND SET CROSSWALK LEDs TO RED
	
	//Set the DDRs defining port direction
	DDRA = 0;
	DDRB = 0xff;
	DDRK = 0xff;
	DDRH = 0xff;
	DDRT = 0xff;
	
	//Turn on the red LEDs
	PORTB = LED_NORTH_RED + LED_SOUTH_RED;
	PTH = LED_EAST_RED + LED_WEST_RED;
	
	//Turn off walk LEDs and yellow LEDs
	PTT = 0;
	PORTK = 0;
}


//changeLights
//Receives the next lightstate and changes the lights to it
//Handles yellow lights
void changeLights(lightState nextState)
{
	//******************************************
	//Theory
	/*
		This task receives the state the system should be in.
		When the state is received, the first step is to compare the new state to the current state using XOR
		After comparing, if the state for any certain light changes, we need to decide if it is changing from green to red or red to green
		Depending on which, it either changes the light to yellow and sets a flag or changes it to green
		It then waits several seconds for the yellow to show
		It then transitions the remaining lights that are yellow to red and starts waiting again
	
		Furthermore, if the light opposite a light turning to green (IE. North turning to green...so south light) is passing through yellow
			set a flag (gFlags) so that the system waits to make it green until after the opposing light has passed through yellow
	*/
	//******************************************
	
	
	INT8U  	lightDiff,	//Differences between states
			yFlags,	//Flags for lights passing through yellow
			gFlags;	//Flags for lights waiting for opposing yellow
	
	//Compare desired light state to current light state and change accordingly
	
		//Set all flags to 0
		yFlags = 0;
		gFlags = 0;
		
		/****************************************************/
		//CRITICAL SECTION - cState CANNOT BE CHANGED
		/****************************************************/
		OS_ENTER_CRITICAL();
		
		//USE EXCLUSIVE OR (XOR) to compare states
		//Sets all bits to 1 for lights that are changing
		//**************************************//
		lightDiff  = nextState.lstate ^ cState.lstate;
		
		
		/*Next section all the same for the most part so only the first
			segment is commented*/
		
		
		//Turn signals
		
		//If the light is to be changed
		if(lightDiff & TURN_NORTH)
			//Check if the light is currently green
			if(cState.lstate & TURN_NORTH)
			{
				//Turn on the yellow light
				PTT += LED_NORTH_TURN_YELLOW;
				
				//Turn off the green light
				PORTB -= LED_NORTH_TURN_GREEN;
				
				//Set the yellow flag so the system knows to change it to red later
				yFlags += TURN_NORTH;
			}
			else
				//Otherwise, turn on the green light
				PORTB += LED_NORTH_TURN_GREEN;
		
		if(lightDiff & TURN_SOUTH)
			if(cState.lstate & TURN_SOUTH)
			{
				PTT += LED_SOUTH_TURN_YELLOW;
				PORTB -= LED_SOUTH_TURN_GREEN;
				yFlags += TURN_SOUTH;
			}
			else
				PORTB += LED_SOUTH_TURN_GREEN;
		
		if(lightDiff & TURN_EAST)
			if(cState.lstate & TURN_EAST)
			{
				PTT += LED_EAST_TURN_YELLOW;
				PTH -= LED_EAST_TURN_GREEN;
				yFlags += TURN_EAST;
			}
			else
				PTH += LED_EAST_TURN_GREEN;
		
		if(lightDiff & TURN_WEST)
			if(cState.lstate & TURN_WEST)
			{
				PTT += LED_WEST_TURN_YELLOW;
				PTH -= LED_WEST_TURN_GREEN;
				yFlags += TURN_WEST;
			}
			else
				PTH += LED_WEST_TURN_GREEN;		

			
		/*Next section all the same for the most part so only the first
			segment is commented*/
			
		//Straight Lights

		//Check if the light's status is changing
		if(lightDiff & LIGHT_NORTH)
			//Check if the light is currently green
			if(cState.lstate & LIGHT_NORTH)
			{
				//Turn on yellow light
				PTT += LED_NORTH_YELLOW;
				
				//Turn off green light
				PORTB -= LED_NORTH_GREEN;
				
				//Set flag so that system knows to turn to red later
				yFlags += LIGHT_NORTH;
			}
			//If light is currently red
			else
			{
				//If the signal opposite is currently yellow
				if(yFlags & TURN_SOUTH)
					//Set a flag to turn on the green later
					gFlags += LIGHT_NORTH;
				else
				{
					//Turn on the green LED
					PORTB += LED_NORTH_GREEN;
					
					//Turn off the red LED
					PORTB -= LED_NORTH_RED;
				}
			}
		
		if(lightDiff & LIGHT_SOUTH)
			if(cState.lstate & LIGHT_SOUTH)
			{
				PTT += LED_SOUTH_YELLOW;
				PORTB -= LED_SOUTH_GREEN;
				yFlags += LIGHT_SOUTH;
			}
			else
			{
				if(yFlags & TURN_NORTH)
					gFlags += LIGHT_SOUTH;
				else
				{
					PORTB += LED_SOUTH_GREEN;
					PORTB -= LED_SOUTH_RED;
				}
			}
		
		if(lightDiff & LIGHT_EAST)
			if(cState.lstate & LIGHT_EAST)
			{
				PTT += LED_EAST_YELLOW;
				PTH -= LED_EAST_GREEN;
				yFlags += LIGHT_EAST;
			}
			else
			{
				if(yFlags & TURN_WEST)
					gFlags += LIGHT_EAST;
				else
				{
					PTH += LED_EAST_GREEN;
					PTH -= LED_EAST_RED;
				}
			}
		
		if(lightDiff & LIGHT_WEST)
			if(cState.lstate & LIGHT_WEST)
			{
				PTT += LED_WEST_YELLOW;
				PTH -= LED_WEST_GREEN;
				yFlags += LIGHT_WEST;
			}
			else
			{
				if(yFlags & TURN_EAST)
					gFlags += LIGHT_WEST;
				else
				{
					PTH += LED_WEST_GREEN;
					PTH -= LED_WEST_RED;
				}
			}
		
		

		
		
		
		//If the next state is cars going in both directions, turn on the walk lights
		
		if(nextState.lstate == NS_GO)
			PORTK = LED_WALK_NS_WHITE;
   		else if(nextState.lstate == EW_GO)
			PORTK = LED_WALK_EW_WHITE;
		else
			//Otherwise, turn off all walk lights
			PORTK = 0;

		//If not passing through a transitional state, set the current state
		/*Deals with ambulance handling....the system needs to know the state
			of the LEDs at this time*/
		if(nextState.lstate != ALL_STOP)
			cState = nextState;
                
		//Wait 2 seconds for the yellow lights
		OSTimeDlyHMSM(0,0,2,0);
		
		/*DEBUG CODE
		puts("\nMIDDLEOFYELLOW\n");
		printLEDStatus();
		printf("\n");*/
			
		
		/*Next section all the same for the most part so only the first
			segment is commented*/
		
		//Check for yellow lights
		
		//If the yellow LED is on
		if(yFlags & LIGHT_NORTH)
		{
			//Turn off yellow LED
			PTT -= LED_NORTH_YELLOW;
			
			//Turn on red LED
			PORTB += LED_NORTH_RED;
		}

		if(yFlags & LIGHT_SOUTH)
		{
			PTT -= LED_SOUTH_YELLOW;
			PORTB += LED_SOUTH_RED;
		}
		if(yFlags & LIGHT_EAST)
		{
			PTT -= LED_EAST_YELLOW;
			PTH += LED_EAST_RED;
		}
		if(yFlags & LIGHT_WEST)
		{
			PTT -= LED_WEST_YELLOW;
			PTH += LED_WEST_RED;
		}
		
		//Turn signals have no red LEDs, just yellow
		
		if(yFlags & TURN_NORTH)
			PTT -= LED_NORTH_TURN_YELLOW;
		if(yFlags & TURN_SOUTH)
			PTT -= LED_SOUTH_TURN_YELLOW;
		if(yFlags & TURN_EAST)
			PTT -= LED_EAST_TURN_YELLOW;
		if(yFlags & TURN_WEST)
			PTT -= LED_WEST_TURN_YELLOW;

		/*Next section all the same for the most part so only the first
			segment is commented*/
		
		//Next section is green flags
		//Deals with a car opposite a changing light
		
		//If the green flag is set
		if(gFlags & LIGHT_NORTH)
		{
			//Turn on the green LED
			PORTB += LED_NORTH_GREEN;
			
			//Turn off the red LED
			PORTB -= LED_NORTH_RED;
		}

		if(gFlags & LIGHT_SOUTH)
		{
			PORTB += LED_SOUTH_GREEN;
			PORTB -= LED_SOUTH_RED;
		}

		if(gFlags & LIGHT_EAST)
		{
			PTH += LED_EAST_GREEN;
			PTH -= LED_EAST_RED;
		}

		if(gFlags & LIGHT_WEST)
		{
			PTH += LED_WEST_GREEN;
			PTH -= LED_WEST_RED;
		}


		/****************************************************/
		//END CRITICAL SECTION
		/****************************************************/
		OS_EXIT_CRITICAL();
		
		//Return to main task
		return;
}


//checkSensors
//Sets the flags to input port data when called
void checkSensors()
{

     cflags = PORTA;
}


/******************************************************
			TASK DEFINITIONS
******************************************************/

//MainLightCycle
//Handles the main cycle between states
void MainLightCycle(void* pdata)
{
	
	//Debug output code
	puts("\nENTERING MAIN LIGHT CYCLE TASK\n");

	
	lightState 	nextState,	//Holder for the next state
			stopState;	//Blank all red state


	//Setup all red state
	stopState.lstate = ALL_STOP;
	stopState.astate = 0;
	
	//Clear the sensors
	cflags = 0;
    
	//Main cycle
	while(1)
	{
		//Change all of the lights to red
		//NOTE:  cState is NOT changed here
		//This is purely an INTERMEDIATE state
		changeLights(stopState);
		
		//Wait for a period
		OSTimeDlyHMSM(0,0,3,0);

		//Determine the next state after the current state
		nextState = determineNextState(cState);
		
		//Set cState to stopState so that LEDs change appropriately
		//Already determined next state so cState doesn't need to reflect actual state
		cState = stopState;
		
		//Change the lights to the next state
		changeLights(nextState);
		
		//When done changing lights, change current state to reflect
		cState = nextState;
		
		//DEBUG:  Print the current state
		printStatus(cState);

		//Turn signal handling
		//Since the waiting time is different for go states and turn states
		
		//If there is a turning state active (IE. not a GO state)
		if(cState.lstate != EW_GO && cState.lstate != NS_GO)
		{
			//Wait a period with the turning light green
			OSTimeDlyHMSM(0,0,4,0);
			
			//Determine the next state
			nextState = determineNextState(cState);
			
			//DEBUG CODE
			//puts("\nTURN STATE\n");
			
			//Change the lights
			changeLights(nextState);
			
			//Set the current state to reflect change
			cState = nextState;
			
			//DEBUG:  Print current state
			printStatus(cState);
			
			//Wait a period with the go light green
			OSTimeDlyHMSM(0,0,7,0);
		}
		else
			//If not a turning state, wait a period with lights green
			OSTimeDlyHMSM(0,0,10,0);
	}
}

//AmbulanceHandler task
//Handles when an ambulance is detected
//Higher priority than mainLightCycle
void ambulanceHandler(void* PDATA)
{
	//DEBUG:  Print code signalling task start
	printf("\nENTERING AMBULANCE HANDLER\n");
     
	lightState 	stopState,	//All red state
			nextState;	//Next state holder
	
	//Setup all red state
	stopState.lstate = ALL_STOP;
	stopState.astate = 0;
     
	//Set the walking state of nextstate to be 0 always
	nextState.astate = 0;

	while(1)
	{
		//Wait 1 second....IE. Poll the sensors every 1 second
		OSTimeDlyHMSM(0,0,1,0);
		
		//Poll the sensors
		//Sets flags in cflags
		checkSensors();
		
		/*Next section all the same for the most part so only the first
			segment is commented*/
		
		//If an ambulance is approaching from the north
		if(cflags & NORTH_AMBULANCE_FLAG)
		{
			//DEBUG:  Print ambulance coming
			puts("\nAmbulance Coming from North\n");
			
			//Suspend the main light cycle task
			OSTaskSuspend(mainTaskPrio);
			
			//Change the lights to all red
			changeLights(stopState);
			
			//Wait 3 seconds at all red
			OSTimeDlyHMSM(0,0,3,0);
			
			//Set the desired next state (N_TURN)
			nextState.lstate = N_TURN;
			
			//Set cState to reflect LED status correctly
			cState = stopState;
			
			//DEBUG
			//printStatus(cState);
			
			//Change the lights so the ambulance can go
			changeLights(nextState);
			
			//Set the current state correctly
			cState = nextState;
			
			//Give the ambulance 10 seconds to go
			OSTimeDlyHMSM(0,0,10,0);
			
			/* NEXT PART TRICKY
			I'd prefer to restart the mainTask but I have no
			idea where it would be.  So we have to create a new one.
			
			LONG TERM MEMORY LEAK!!!!!!
			VERY BAD CODING!  BAD BAD BAD GEORGE!!
			
			But works for our purposes.*/
			
			//Increase mainTaskPrio once
			mainTaskPrio++;
			
			//Start a new mainlightcycle task using new priority
			//New task correctly picks up where it should
			OSTaskCreate(MainLightCycle, (void *) 1, &MainLightCycleStk[TASK_STK_SIZE], mainTaskPrio);
		}
		else if(cflags & SOUTH_AMBULANCE_FLAG)
		{
			puts("\nAmbulance Coming from South\n");
			OSTaskSuspend(mainTaskPrio);
			changeLights(stopState);
			nextState.lstate = S_TURN;
			cState = stopState;
			changeLights(nextState);
			cState = nextState;
			OSTimeDlyHMSM(0,0,10,0);
			mainTaskPrio++;
			OSTaskCreate(MainLightCycle, (void *) 1, &MainLightCycleStk[TASK_STK_SIZE], mainTaskPrio);
		}
		else if(cflags & EAST_AMBULANCE_FLAG)
		{
			puts("\nAmbulance Coming from East\n");
			OSTaskSuspend(mainTaskPrio);
			changeLights(stopState);
			nextState.lstate = E_TURN;
			cState = stopState;
			changeLights(nextState);
			cState = nextState;
			OSTimeDlyHMSM(0,0,10,0);
			mainTaskPrio++;
			OSTaskCreate(MainLightCycle, (void *) 1, &MainLightCycleStk[TASK_STK_SIZE], mainTaskPrio);
		}
		else if(cflags & WEST_AMBULANCE_FLAG)
		{
			puts("\nAmbulance Coming from West\n");
			OSTaskSuspend(mainTaskPrio);
			changeLights(stopState);
			nextState.lstate = W_TURN;
			cState = stopState;
			changeLights(nextState);
			cState = nextState;
			OSTimeDlyHMSM(0,0,10,0);
			mainTaskPrio++;
			OSTaskCreate(MainLightCycle, (void *) 1, &MainLightCycleStk[TASK_STK_SIZE], mainTaskPrio);
		}
	}
}

//Main
//First function run at the start of everything
int main()
{
	//Initialize the LEDs
	initializeLights();
	
	//Initialize uCos
	OSInit();
	
	//Set the main task priority
	mainTaskPrio = 10;
	
	//DEBUG
	//Print starting tasks
	printf("CREATING TASKS\n");
	
	//Create the main light cycle task using the mainTaskPrio variable
	OSTaskCreate(MainLightCycle, (void *) 1, &MainLightCycleStk[TASK_STK_SIZE], mainTaskPrio);
	
	//Create the ambulance handling task
	OSTaskCreate(ambulanceHandler, (void *) 1, &ambulanceHandlerStk[TASK_STK_SIZE],1);
	
	//DEBUG:  LED TESTING TASK
	//OSTaskCreate(testLEDs, (void *) 1, &testLEDsStk[TASK_STK_SIZE], mainTaskPrio);

	//DEBUG:  Print starting OS
	printf("\nSTARTING OS\n");

	//Start multitasking
	OSStart();

	/* NEVER EXECUTED */
	puts("main(): We should never execute this line\n");
}

//PrintStatus
//Outputs passed status
void printStatus(lightState currState)
{

	//This function switches the passed state
		//and outputs it in ASCII to the screen
	
	switch (currState.lstate){

		case ALL_STOP:
			puts("ALL STOP\n");
			break;

		case EW_GO:
			puts("EW GO\n");
			break;

		case NS_TURN:
			puts("NS TURN\n");
			break;

		case N_TURN:
			puts("N TURN\n");
			break;

		case S_TURN:
			puts("S TURN\n");
			break;

		case NS_GO:
			puts("NS GO\n");
			break;

		case EW_TURN:
			puts("EW TURN\n");
			break;

		case E_TURN:
			puts("E TURN\n");;
			break;

		case W_TURN:
			puts("W TURN\n");
			break;

	}

}