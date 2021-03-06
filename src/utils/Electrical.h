// Electrical.h

/**
 * Contains functions for:
 *
 *   - reading keypad, and GPIO (ie. battery test circuit, photoresistor, thermistor)
 *   - outputting to displays and warning LED light
 *
 *
 * GPIO Pinout:
 * 	 - GPIO0 3:0 are outputs to row select lines of keypad
 *   - GPIO0 8:5 are inputs from column lines
 */

#include <stdio.h>
 
#ifndef INTELLICALC_ELECTRICAL
#define INTELLICALC_ELECTRICAL

#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4
#define UINT_MAX 4294967295

// global for current button presses on keypad 
volatile int currentKeypad[KEYPAD_ROWS][KEYPAD_COLS] = {
														{0, 0, 0, 0},
														{0, 0, 0, 0},
														{0, 0, 0, 0},
														{0, 0, 0, 0}
													   };
// global for past button presses on keypad
volatile int pastKeypad[KEYPAD_ROWS][KEYPAD_COLS] = {
													{0, 0, 0, 0},
													{0, 0, 0, 0},
													{0, 0, 0, 0},
													{0, 0, 0, 0}
													};



volatile unsigned int* GPIO0_ADDR = (unsigned int*)(0xFF708000);
volatile unsigned int* GPIO1_ADDR = (unsigned int*)(0xFF709000);
volatile unsigned int* GPIO2_ADDR = (unsigned int*)(0xFF70A000);

// initialize (required)
void initKeypadGPIO();
void initBatteryGPIO()

// update global 2D arrays for keypad presses
void scanKeypad();

// @return seven-segment code for given character
unsigned char decodeChar(const char character);

// @param character: 0-9, ., [, ], s, t, c (sin, cos, tan), l, n, ^ 
void writeToDisplay(const char character, const unsigned int displayNum);

// turn all 6 displays off
void turnDisplaysOff();

// display most recent 6 character from input sequence on hex displays as we input stuff
void displayCurrentInput(volatile char* input, const unsigned int size);

// display a result from calculation on seven segment displays (first 6 characters)
void displayResult(const double x);

// fill array with 20% power intervals (ie. 1, 1, 1, 0, 0 means up to 60% power left)
void updateBatteryPercentage(volatile unsigned int percentagePower20Intervals[]);

void initKeypadGPIO()
{
	// take GPIO out of reset state
	// permodrst register has address 0xFFD05014
	// when bits are 1 - peripheral in reset
	volatile unsigned int* PERIPHERAL_RESET = (unsigned int*)(0xFFD05014);
	*(PERIPHERAL_RESET) &= (0b111 << 25); // bits 27, 26, 25 are GPIO 0, 1, 2


	/// https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/hb/cyclone-v/cv_54001.pdf
	// set gpio_debounce register bits so that buttons are debounced

	// configure 4 output pins [GPIO0 3:0] (ROWS)
	*(GPIO0_ADDR+0x4) = 0b1111;

	// configure 4 input pins (COLUMNS)
	// by above row GPIO [28:4] are inputs (0)

	// disable interrupts
	*(GPIO0_ADDR+0x30) = 0;
}

const unsigned int rowSelect[KEYPAD_ROWS] = {0b1000, 0b100, 0b10, 0b1};
const unsigned int colSelect[KEYPAD_COLS] = {0b10000000, 0b1000000, 0b100000, 0b10000};

// update global 2D arrays for keypad presses
void scanKeypad()
{
	// See:  gpio_swporta_dr   register for writing to output (rows)
	volatile unsigned int row = 0;
	volatile unsigned int col = 0;

	for (; row < KEYPAD_ROWS; row++)
	{
		// only affect bits 3:0 of GPIO0 (row select pins)
		*(GPIO0_ADDR) |= rowSelect[row];
		*(GPIO0_ADDR) &= (UINT_MAX & rowSelect[row]);

		// we've now isolated one row of the keypad -> scan for button presses
		// See:  gpio_ext_porta  register for reading external input signals
		col = 0;
		for (; col < KEYPAD_COLS; col++)
		{
			// save what the keypad state was the last time we checked
			pastKeypad[row][col] = currentKeypad[row][col];


			if ( (*GPIO0_ADDR + 0x50) & colSelect[col])
			{
				// button is pressed
				currentKeypad[row][col] = 1;
			}
			else
			{
				// button not pressed
				currentKeypad[row][col] = 0;
			}
		}
	}
}


/** Definitions for hex outputs **/
// for digits 0-9
unsigned char lookup[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 
							0x6D, 0x7D, 0x07, 0x7F, 0x67};

unsigned char decPointChar = 0b1000; // bottom LED
unsigned char expChar = 0b100011;
unsigned char sinChar = 0b1101101; // S
unsigned char cosChar = 0b1011000; // C
unsigned char tanChar = 0b1111000; // t
unsigned char lnChar = 0b111000; // L

unsigned char plusChar = 0b1000110; 
unsigned char multChar = 0b1110110;
unsigned char subChar = 0b1000000;
unsigned char divChar = 0b1100100; 
unsigned char factChar = 0b1110001; // F

unsigned char leftBracketChar = 0b111001; // [
unsigned char rightBracketChar = 0b1111; // ]

/** Addresses for 7-segment displays **/
unsigned char * hexBaseAddr_Low = (unsigned char *)0xFF200020;
unsigned char * hexBaseAddr_High = (unsigned char *)0xFF200030;

// return corresponding hex code
unsigned char decodeChar(const char character)
{
	if (character >= '0' && character <= '9')
		return lookup[character-'0'];

	switch (character)
	{
	case '.':
		return decPointChar;
	case '^':
		return expChar;
	case 's':
		return sinChar;
	case 'c':
		return cosChar;
	case 't':
		return tanChar;
	case 'l':
		return lnChar;
	case '+':
		return plusChar;
	case '*':
		return multChar;
	case '-':
		return subChar;
	case '/':
		return divChar;
	case '!':
		return factChar;
	case '(':
		return leftBracketChar;
	case ')':
		return rightBracketChar;
	default:
		return 0;
	}
}
// @param character: 0-9, ., [, ], s, t, c (sin, cos, tan), l, n, ^ 
// @param displayNum - 0 to 5, 0 is right side, 5 is left
void writeToDisplay(const char character, const unsigned int displayNum)
{
	if (displayNum < 4)
	{
		*(hexBaseAddr_Low + displayNum) = decodeChar(character);
	}
	else
	{
		*(hexBaseAddr_High + (displayNum-4)) = decodeChar(character);
	}
}

void turnDisplaysOff()
{
	int i = 0;
	while (i <= 5)
	{
		writeToDisplay(0, i++);
	}
}

// display most recent 6 character from input sequence on hex displays as we input stuff
void displayCurrentInput(volatile char* input, const unsigned int size)
{
	if (size == 0)
		return;

	int i = size-1;
	int hexNum = 0;

	if (i < 0)
	{
		turnDisplaysOff();
	}

	while (i >= 0)
	{
		writeToDisplay(input[i], hexNum);
		i--;
		hexNum++;
	}
}

// display a result from calculation on seven segment displays (first 6 characters)
void displayResult(const double x)
{
	static char buffer[50];

	// confirmed this works
	sprintf(buffer, "%f", x);

	size_t bufferFilledLen = strlen(buffer);

	int i = 0;
	for (; i < 6 && i < bufferFilledLen; i++)
	{
		writeToDisplay(buffer[i], 5-i);
	}
}

// setup inputs on GPIO0 [11, 12, 13, 14, 15]
void initBatteryGPIO()
{
	// this function doesn't have to do anything since all input pins beside 3:0 are
	// already set as inputs by initKeypadGPIO()
	// GPIO is also already taken out of reset mode, interrupts disabled
}

void updateBatteryPercentage(volatile unsigned int percentagePower20Intervals[])
{
	volatile unsigned int checkBits[] = {0b1 << 11, 0b1 << 12, 0b1 << 13, 0b1 << 14, 0b1 << 15};
	volatile unsigned int i = 0;
	for (; i < 5; i++)
	{
		if ((*GPIO0_ADDR + 0x50) & checkBits[i])
		{
			// this interval is logic 1
			percentagePower20Intervals[i] = 1;
		}
		else
		{
			percentagePower20Intervals[i] = 0;
		}
	}
}

#endif
