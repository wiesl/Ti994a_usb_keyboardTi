/* TI99/4A USB Keyboard for the real TI99/4A feeling
 * when sitting behind a TI99/4A emulator.
 *
 * by Fred G. Kaal
 *
 *
 */
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h> 
#include <util/delay.h>
#if defined PROJECT_DBG
#include "usb_keyboard_debug.h" 
#include "print.h" 
#else
#include "usb_keyboard.h" 
#endif

#if defined PROJECT_DBG
//#define TI_KEY_DBG		//Debug TI keyboard functions
//#define TI_MOD_DBG		//Debug TI modifier functions
//#define TI_SCAN_DBG		//Debug TI keyb scan functions
#define USBKEY_DBG		//Debug USB keyboard functions
#endif

typedef unsigned char	byte;
typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned long	ulong;
typedef unsigned int	uint;
typedef unsigned char   bool;

static uchar WhatKey(uchar Pins, char *pThisKeys);

static uint16_t idle_count = 0; 
static bool Pin_D4_Led = 0;

static uint8_t uTsek = 0;	// Tenth of a second counter
static bool    bTsek = 0;	// Tenth of a second flag

#define BIT0	0x01		//Bit definitions
#define BIT1	0x02
#define BIT2	0x04
#define BIT3	0x08
#define BIT4	0x10
#define BIT5	0x20
#define BIT6	0x40
#define BIT7	0x80

#define ARKB_DELAY	10	// Auto Repeat Keyboard delay


#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n)) 

#define DDR_OUT 	DDRD
#define PORT_OUT	PORTD

#define DDR_INP 	DDRB
#define PORT_INP	PORTB
#define PINS_INP	PINB

//#define OUT_1_ON	(PORT_OUT |= (1<<0))
//#define OUT_2_ON	(PORT_OUT |= (1<<1))
//#define OUT_3_ON	(PORT_OUT |= (1<<2))
//#define OUT_4_ON	(PORT_OUT |= (1<<3))
//#define OUT_5_ON	(PORT_OUT |= (1<<4))
//#define OUT_6_ON	(PORT_OUT |= (1<<5))
//#define OUT_7_ON	(PORT_OUT |= (1<<6))
//#define OUT_8_ON	(PORT_OUT |= (1<<7))
//#define OUT_9_ON	(PORT_OUT |= (1<<8))

//#define OUT_1_OFF	(PORT_OUT &= ~(1<<0))
//#define OUT_2_OFF	(PORT_OUT &= ~(1<<1))
//#define OUT_3_OFF	(PORT_OUT &= ~(1<<2))
//#define OUT_4_OFF	(PORT_OUT &= ~(1<<3))
//#define OUT_5_OFF	(PORT_OUT &= ~(1<<4))
//#define OUT_6_OFF	(PORT_OUT &= ~(1<<5))
//#define OUT_7_OFF	(PORT_OUT &= ~(1<<6))
//#define OUT_8_OFF	(PORT_OUT &= ~(1<<7))
//#define OUT_9_ON	(PORT_OUT &= ~(1<<8))

#define LED_ON		(PORTD |=  (1<<6))
#define LED_OFF		(PORTD &= ~(1<<6))
#define LED_OUT		BIT6
 
//#define OUT_ALL_HIGH	(PORT_OUT |= 0xBF)	//Except D6 (LED)
//#define OUT_ALL_LOW	(PORT_OUT &= 0x00)

#define SBO(p) 		(PORT_OUT |=  p)	//Set Bit One
#define SBZ(p) 		(PORT_OUT &= ~p)	//Set Bit Zero
#define TB(p)		((pins & p) ? 0 : 1)	//Test Bit

/*
	KEYMAP as found here: http://www.nouspikel.com/ti99/titechpages.htm
 
 # = wire from left to right looking at keyboard from normal orientation
 
      #12  #13  #14  #15  #9  #8  #6
 #5    =    .    ,    M    N   /   
 #4  spac   L    K    J    H   ;
 #1  entr   O    I    U    Y   P
 #2         9    8    7    6   0
 #7  fctn   2    3    4    5   1  lock
 #3  shft   S    D    F    G   A
 #10 ctrl   W    E    R    T   Q
 #11        X    C    V    B   Z
 
 */
 
// All inputs need pullup resistors
// All outputs need serial resistor (espacially P5 <-> 1Y1)
// Output pin D4 is used as activity led
//
#define TIKB_INT_5	BIT2	// #01 Input  (P  O  I  U  Y  enter          )
#define TIKB_INT_6	BIT3	// #02 Input  (0  9  8  7  6                 )
#define TIKB_INT_8	BIT5	// #03 Input  (A  S  D  F  G  shift          )
#define TIKB_INT_4	BIT1	// #04 Input  (:; L  K  J  H  space          )
#define TIKB_INT_3	BIT0	// #05 Input  (-/ >. <, M  N  +=             )
#define TIKB_P_5	BIT7	// #06 Output [alpha-lock                    ]
#define TIKB_INT_7	BIT4	// #07 Input  (1  2  3  4  5  fctn alpha-lock)
#define TIKB_1_Y1	BIT1	// #08 Output [-/ :; P  O  1  A  Q Z         ]
#define TIKB_1_Y0	BIT0	// #09 Output [N  H  Y  6  5  G  T B         ]
#define TIKB_INT_9	BIT6	// #10 Input  (Q  W  E  R  T  ctrl           )
#define TIKB_INT_10	BIT7	// #11 Input  (Z  X  C  V  B                 )
#define TIKB_2_Y0	BIT2	// #12 Output [+= space enter shift ctrl fctn]
#define TIKB_2_Y1	BIT3	// #13 Output [>. L  O  9  2  S  W  X        ]
#define TIKB_2_Y2	BIT4	// #14 Output [<, K  I  8  3  D  E  C        ]
#define TIKB_2_Y3	BIT5	// #15 Output [M  J  U  7  4  F  R  V        ]

#define OUT_PULLUP	(TIKB_1_Y0 | TIKB_1_Y1 | TIKB_2_Y0 | TIKB_2_Y1 | TIKB_2_Y2 | TIKB_2_Y3 | TIKB_P_5)

typedef struct _keyconv_
{
	uchar Ti_key;
	uchar Usb_key;
	uchar Usb_modifier;
} KEYCONV;

static KEYCONV KeyConv[] =
{ //     Ti   Usb    Modifier
	{' ', KEY_SPACE,     0},		//Ti kb
	{'!', KEY_1,         KEY_SHIFT},	
	{'\"',KEY_COMMA,     KEY_SHIFT},	
	{'#', KEY_3,         KEY_SHIFT},	
	{'$', KEY_4,         KEY_SHIFT},	
	{'%', KEY_5,         KEY_SHIFT},	
	{'&', KEY_7,         KEY_SHIFT},	
	{'\'',KEY_BACKSLASH, 0},	
	{'(', KEY_9,         KEY_SHIFT},	
	{')', KEY_0,         KEY_SHIFT},	
	{'*', KEY_8,         KEY_SHIFT},	
	{'+', KEY_EQUAL,     KEY_SHIFT},	
	{',', KEY_COMMA,     0},		//Ti kb	
	{'-', KEY_MINUS,     0},		//Ti kb	
	{'.', KEY_PERIOD,    0},		//Ti kb	
	{'/', KEY_SLASH,     0},	
	{'0', KEY_0,         0},		//Ti kb
	{'1', KEY_1,         0},		//Ti kb
	{'2', KEY_2,         0},		//Ti kb
	{'3', KEY_3,         0},		//Ti kb
	{'4', KEY_4,         0},		//Ti kb
	{'5', KEY_5,         0},		//Ti kb
	{'6', KEY_6,         0},		//Ti kb
	{'7', KEY_7,         0},		//Ti kb
	{'8', KEY_8,         0},		//Ti kb
	{'9', KEY_9,         0},		//Ti kb
	{':', KEY_SEMICOLON, KEY_SHIFT},	//Ti kb	
	{';', KEY_SEMICOLON, 0},	
	{'<', KEY_COMMA,     KEY_SHIFT},	
	{'=', KEY_EQUAL,     0},		//Ti kb
	{'>', KEY_PERIOD,    KEY_SHIFT},	
	{'?', KEY_SLASH,     KEY_SHIFT},	
	{'@', KEY_2,         KEY_SHIFT},	
	{'A', KEY_A,         0},		//Ti kb
	{'B', KEY_B,         0},		//Ti kb
	{'C', KEY_C,         0},		//Ti kb
	{'D', KEY_D,         0},		//Ti kb
	{'E', KEY_E,         0},		//Ti kb
	{'F', KEY_F,         0},		//Ti kb
	{'G', KEY_G,         0},		//Ti kb
	{'H', KEY_H,         0},		//Ti kb
	{'I', KEY_I,         0},		//Ti kb
	{'J', KEY_J,         0},		//Ti kb
	{'K', KEY_K,         0},		//Ti kb
	{'L', KEY_L,         0},		//Ti kb
	{'M', KEY_M,         0},		//Ti kb
	{'N', KEY_N,         0},		//Ti kb
	{'O', KEY_O,         0},		//Ti kb
	{'P', KEY_P,         0},		//Ti kb
	{'Q', KEY_Q,         0},		//Ti kb
	{'R', KEY_R,         0},		//Ti kb
	{'S', KEY_S,         0},		//Ti kb
	{'T', KEY_T,         0},		//Ti kb
	{'U', KEY_U,         0},		//Ti kb
	{'V', KEY_V,         0},		//Ti kb
	{'W', KEY_W,         0},		//Ti kb
	{'X', KEY_X,         0},		//Ti kb
	{'Y', KEY_Y,         0},		//Ti kb
	{'Z', KEY_Z,         0},		//Ti kb
};


int main(void)
{
	bool caps  = 0;
	bool shift = 0;
	bool ctrl  = 0;
	bool fctn  = 0;

#if defined TI_MOD_DBG
	bool h_caps  = 0;
	bool h_shift = 0;
	bool h_ctrl  = 0;
	bool h_fctn  = 0;
#endif
	
	uchar pins;
	uchar key;
	uchar usbkey;
	uchar usbmod;
	
	uchar prev_usbkey = 0;
	uchar prev_usbmod = 0;
	
	uchar arKbDelay = 0;	// Auto repeat keyboard delay
	
	// set for 16 MHz clock, and make sure all outputs are off 
	CPU_PRESCALE(0);

	DDR_OUT   = 0x40;	//All Input, D6 output (LED)
	DDR_INP   = 0x00;	//All Input
	PORT_INP |= 0xFF;	//And Pull-up resistors

	// Initialize the USB, and then wait for the host to set configuration.
	// If the Teensy is powered without a PC connected to the USB port,
	// this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

	// Wait an extra second for the PC's operating system to load drivers
	// and do whatever it does to actually be ready for input
	_delay_ms(1000); 
	
	// Configure timer 0 to generate a timer overflow interrupt every
	// 256*1024 clock cycles, or approx 61 Hz when using 16 MHz clock
	// This demonstrates how to use interrupts to implement a simple
	// inactivity timeout.
	TCCR0A = 0x00;
	TCCR0B = 0x05;
	TIMSK0 = (1<<TOIE0);
 	
	while (1)
	{
		//OUT_ALL_HIGH;
/* The TI99/4A uses a TMS9901 I/O chip with a 74LS156 decoder
 * witj open collector output. If two keys are pressed (for
 * instance SHIFT and A) both keys kan be read.
 *
 * When using a TEENSY I/O port as an output port there are
 * no open collector outputs. To overcome this problem
 * only one pin is used as an output pin and all others are
 * switched to input.
 */

		DDR_OUT = LED_OUT | TIKB_P_5;
		PORT_OUT |= OUT_PULLUP & ~TIKB_P_5;
		SBZ(TIKB_P_5);
		_delay_ms(5);
		pins = PINS_INP;
		SBO(TIKB_P_5);
		caps = TB(TIKB_INT_7);
		
#if defined TI_MOD_DBG
		if (h_caps != caps)
		{
			h_caps = caps;
			print("CAPS\n");
		}
#endif

		DDR_OUT = LED_OUT | TIKB_2_Y0;
		PORT_OUT |= OUT_PULLUP & ~TIKB_2_Y0;
		SBZ(TIKB_2_Y0);
		_delay_ms(5);
		pins  = PINS_INP;
		SBO(TIKB_2_Y0);
		shift = TB(TIKB_INT_8);
		ctrl  = TB(TIKB_INT_9);
		fctn  = TB(TIKB_INT_7);

#if defined TI_MOD_DBG
		if (h_shift != shift)
		{
			h_shift = shift;
			print("SHIFT\n");
		}
		if (h_ctrl != ctrl)
		{
			h_ctrl = ctrl;
			print("CTRL\n");
		}
		if (h_fctn != fctn)
		{
			h_fctn = fctn;
			print("FCTN\n");
		}
#endif		
		do
		{
			DDR_OUT = LED_OUT | TIKB_1_Y0;
			PORT_OUT |= OUT_PULLUP & ~TIKB_1_Y0;
			SBZ(TIKB_1_Y0);
			_delay_ms(5);
			pins = ~PINS_INP; 
			SBO(TIKB_1_Y0);
#if defined TI_SCAN_DBG
			print("1_Y0="); phex(pins); print("\n");
#endif
			if ((key = WhatKey(pins, "NHY65GTB")) != 0) break; 

			DDR_OUT = LED_OUT | TIKB_1_Y1;
			PORT_OUT |= OUT_PULLUP & ~TIKB_1_Y1;
			SBZ(TIKB_1_Y1);
			_delay_ms(5);
			pins = ~PINS_INP;
			SBO(TIKB_1_Y1);
#if defined TI_SCAN_DBG
			print("1_Y1="); phex(pins); print("\n");
#endif
			if ((key = WhatKey(pins, "/;P01AQZ")) != 0) break; 

			DDR_OUT = LED_OUT | TIKB_2_Y0;
			PORT_OUT |= OUT_PULLUP & ~TIKB_2_Y0;
			SBZ(TIKB_2_Y0);
			_delay_ms(5);
			pins = ~PINS_INP;
			SBO(TIKB_2_Y0);
#if defined TI_SCAN_DBG
			print("2_Y0="); phex(pins); print("\n");
#endif
			if ((key = WhatKey(pins, "= \n")) != 0) break; 
		
			DDR_OUT = LED_OUT | TIKB_2_Y1;
			PORT_OUT |= OUT_PULLUP & ~TIKB_2_Y1;
			SBZ(TIKB_2_Y1);
			_delay_ms(5);
			pins = ~PINS_INP;
			SBO(TIKB_2_Y1);
#if defined TI_SCAN_DBG
			print("2_Y1="); phex(pins); print("\n");
#endif
			if ((key = WhatKey(pins, ".LO92SWX")) != 0) break; 

			DDR_OUT = LED_OUT | TIKB_2_Y2;
			PORT_OUT |= OUT_PULLUP & ~TIKB_2_Y2;
			SBZ(TIKB_2_Y2);
			_delay_ms(5);
			pins = ~PINS_INP;
			SBO(TIKB_2_Y2);
#if defined TI_SCAN_DBG
			print("2_Y2="); phex(pins); print("\n");
#endif
			if ((key = WhatKey(pins, ",KI83DEC")) != 0) break; 
		
			DDR_OUT = LED_OUT | TIKB_2_Y3;
			PORT_OUT |= OUT_PULLUP & ~TIKB_2_Y3;
			SBZ(TIKB_2_Y3);
			_delay_ms(5);
			pins = ~PINS_INP;
			SBO(TIKB_2_Y3);
#if defined TI_SCAN_DBG
			print("2_Y3="); phex(pins); print("\n");
#endif
			if ((key = WhatKey(pins, "MJU74FRV")) != 0) break; 
		}
		while (0);
		
		if (key == 0)
		{
			if (prev_usbkey != 0)
			{
				cli();
				idle_count = 0;
				Pin_D4_Led = 1;
				LED_OFF;
				sei();
			}
			arKbDelay = 0;
			prev_usbkey = 0;
			prev_usbmod = 0;
		}
		else
		{
#if defined TI_KEY_DBG
			if (caps)  print ("Caps ");
			if (shift) print ("Shft ");
			if (ctrl)  print ("Ctrl ");
			if (fctn)  print ("Fctn ");

			print("Key: 0x");
			phex(key);
			print(" [");
			pchar(key);
			print("]\n");
#endif
			cli();
			idle_count = 0;
			Pin_D4_Led = 0;
			LED_ON;
			sei();

			usbkey = 0;
			usbmod = 0;
			
			if (key == '\n')
			{
				usbkey = KEY_ENTER;
			}
			else if (fctn)
			{
				switch(key)
				{
				case 'A': usbkey = KEY_BACKSLASH; usbmod = KEY_SHIFT; break;
				case 'C': usbkey = KEY_TILDE; usbmod = 0; break;
				case 'D': usbkey = KEY_RIGHT; usbmod = 0; break;
				case 'E': usbkey = KEY_UP; usbmod = 0; break;
				case 'F': usbkey = KEY_LEFT_BRACE; usbmod = KEY_SHIFT; break;
				case 'G': usbkey = KEY_RIGHT_BRACE; usbmod = KEY_SHIFT; break;
				case 'I': usbkey = KEY_SLASH; usbmod = KEY_SHIFT; break;
				case 'O': usbkey = KEY_QUOTE; usbmod = 0; break;
				case 'P': usbkey = KEY_QUOTE; usbmod = KEY_SHIFT; break;
				case 'R': usbkey = KEY_LEFT_BRACE; usbmod = 0; break;
				case 'S': usbkey = KEY_LEFT; usbmod = 0; break;
				case 'T': usbkey = KEY_RIGHT_BRACE; usbmod = 0; break;
				case 'U': usbkey = KEY_MINUS; usbmod = KEY_SHIFT; break;
				case 'W': usbkey = KEY_TILDE; usbmod = KEY_SHIFT; break;
				case 'X': usbkey = KEY_DOWN; usbmod = 0; break;
				case 'Z': usbkey = KEY_BACKSLASH; usbmod = 0; break;
				}
				if (usbkey != 0) fctn = 0;
			}
			if ((usbkey == 0)  && (key >= ' ') && (key <= 'Z'))
			{
				if ((key == '/') && shift)
				{
					shift = 0;
					usbkey = KEY_MINUS;
					usbmod = 0;
				}
				else
				{
					usbkey = KeyConv[key-0x20].Usb_key;
					usbmod = KeyConv[key-0x20].Usb_modifier;
					if (caps && (key >= 'A') && (key <= 'Z'))
					{
						usbmod |= KEY_SHIFT;
					}
				}
			}
//When to use this??	if (caps)  usbmod |= KEY_CAPS_LOCK;
			if (shift) usbmod |= KEY_SHIFT;
			if (ctrl)  usbmod |= KEY_CTRL;
			if (fctn)  usbmod |= KEY_ALT;

			if (usbkey != 0)
			{
				if ((prev_usbkey != usbkey)
				||  (prev_usbmod != usbmod)
				||  (arKbDelay == ARKB_DELAY)
				)
				{
#if defined USBKEY_DBG
					print("UsbMod: 0x");
					phex(usbmod);
					print("  UsbKey: 0x");
					phex(usbkey);
					print("  Delay: 0x");
					print("\n");
#endif
					usb_keyboard_press(usbkey, usbmod); 
				}
				else if (bTsek)
				{
					arKbDelay++;
				}
			}
#if defined USBKEY_DBG
			else
			{
				print("No USB key!\n");
			}
#endif
			prev_usbkey = usbkey;
			prev_usbmod = usbmod;
		}
	} 
}

static uchar WhatKey(uchar Pins, char *pThisKeys)
{
	uchar mask = 0x01;
	
	while (*pThisKeys != '\0')
	{
		if (Pins & mask) return *pThisKeys;
		
		mask <<= 1;
		pThisKeys += 1;
	}
	return 0;
}

// This interrupt routine is run approx 61 times per second.
// A very simple inactivity timeout is implemented, where the
// LED is blinking.

ISR(TIMER0_OVF_vect)
{
	uTsek++;
	bTsek = 0;
	if (uTsek >= 6)
	{
		uTsek = 0;
		bTsek = 1;
	}
	
	idle_count++;
	if (idle_count > 61 * 1)
	{
		idle_count = 0;
		
		if (Pin_D4_Led)
		{
			Pin_D4_Led = 0;
			LED_ON;
//			print("Led ON\n");
		}
		else
		{
			Pin_D4_Led = 1;
			LED_OFF;
//			print("Led OFF\n");
		}
//		print("Timer Event :)\n");
//		usb_keyboard_press(KEY_SPACE, 0);
	}
}
 