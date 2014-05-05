#ifndef _AvrXSerialIo_h_
#define _AvrXSerialIo_h_
#include "AvrXFifo.h"
/*
	AvrSerialIo.h

	Sample code for fully buffered interrupt driven serial I/O for the
	AVR processor.  Uses the AvrXFifo facility.

	Author: Larry Barello (larry@barello.net)

	Revision History:

*/

// pitschu: if defed USE_USB we use the USB VCP to send data to a host (PuTTY); if not defed we use USART3 on PD8/PD9 pins
#define USE_USB		1


// Buffer size can be any thing from 2 to 32000
#define FIFOLEN_TOHOST 		1000
#define FIFOLEN_FROMHOST 	64

// Forward declarations
#ifndef _AVRXSERIALIO_C_	// Don't comingle this macro
	AVRX_EXT_FIFO(fifoToHost);
	AVRX_EXT_FIFO(fifoFromHost);
#endif

// pitschu july 2013: ported from AVR to STM32;
extern void  USART3_Init (int baudRate);			// to host

int put_c2Host(char c);	// Non blocking output
int put_char2Host( char c);	// Blocking output
int write_str2Host (char* p);
int get_cHost(void);	// Non blocking, return status outside of char range
int get_charHost(void);	// Blocks waiting for something

#endif //_AvrSerialIo_h_
