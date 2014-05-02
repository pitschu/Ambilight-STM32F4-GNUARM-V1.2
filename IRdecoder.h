/*****************************************************
 *
 *	Control program for the PitSchuLight TV-Backlight
 *	(c) Peter Schulten, Mülheim, Germany
 *	peter_(at)_pitschu.de
 *
 *	Die unveränderte Wiedergabe und Verteilung dieses gesamten Sourcecodes
 *	in beliebiger Form ist gestattet, sofern obiger Hinweis erhalten bleibt.
 *
 * 	Ich stelle diesen Sourcecode kostenlos zur Verfügung und biete daher weder
 *	Support an noch garantiere ich für seine Funktionsfähigkeit. Außerdem
 *	übernehme ich keine Haftung für die Folgen seiner Nutzung.

 *	Der Sourcecode darf nur zu privaten Zwecken verwendet und modifiziert werden.
 *	Darüber hinaus gehende Verwendung bedarf meiner Zustimmung.
 *
 *	History
 *	09.06.2013	pitschu		Start of work
 */


#ifndef IRDECODER_H_
#define IRDECODER_H_

// NEC / Japanese...
// 13.500ms     - start
//  2.250ms     - "1"
//  1.125ms     - "0"
// 90.000ms     - key repeat

#define T1_PER_MILISEC		(250000L / 1000)		// timer runs at 250kHz

#define NECP_MIN       (uint16_t)(T1_PER_MILISEC * 0.875)
#define NECP_ZERO      (uint16_t)(T1_PER_MILISEC * 1.750)
#define NECP_ONE       (uint16_t)(T1_PER_MILISEC * 3.000)
#define NECP_REPEAT    (uint16_t)(T1_PER_MILISEC * 12.00)
#define NECP_MAX       (uint16_t)(T1_PER_MILISEC * 15.00)

// NEC ir remote control codes

#define	NO_CODE				0xffff		// in <addr> part of irCode_t

#define	BRIGHTNESS_HI		0x5C
#define	BRIGHTNESS_LO		0x5D
#define	MODEKEY_BLACK		0x41
#define ONOFF_KEY			0x40
#define	RED_HI				0x14
#define	RED_LO				0x10
#define	GREEN_HI			0x15
#define	GREEN_LO			0x11
#define	BLUE_HI				0x16
#define	BLUE_LO				0x12
#define QUICK_KEY			0x17
#define SLOW_KEY			0x13
#define FADE3_KEY			0x06
#define FADE7_KEY			0x07
#define JUMP3_KEY			0x04
#define JUMP7_KEY			0x05
#define	AUTO_KEY			0x0F
#define FLASH_KEY			0x0B
#define	DIY_1_KEY			0x0C
#define	DIY_2_KEY			0x0D
#define	DIY_3_KEY			0x0E
#define	DIY_4_KEY			0x08
#define	DIY_5_KEY			0x09
#define	DIY_6_KEY			0x0A
//following keys are some of the fixed color keys; they are used for ambilight parameters too
#define RED_KEY				0x58		// Hue control
#define GREEN_KEY			0x59		// Saturation
#define BLUE_KEY			0x45		// Contrast
#define WHITE_KEY			0x44		// Brightness

// codes for isNew field in irCode struct

typedef enum {
	IR_NOTHING	= 0,	// code has been seen by caller
	IR_PRESSED	= 1,	// initial key press
	IR_AUTORPT,			// starts after 1 second
	IR_RELEASED,		// key was released
	IR_CHECKED			// code was checked and used
} irCodes_e;

#define	AUTO_RPT_INITIAL	8
#define	AUTO_RPT			(AUTO_RPT_INITIAL - 1)


typedef struct {
	volatile uint16_t	code;
	volatile uint16_t	repcntPressed;
	volatile uint16_t	ticksAutorpt;
	volatile irCodes_e	isNew;
} irCode_t;

extern volatile irCode_t	irCode;

extern void IRdecoderInit(void);


#endif /* IRDECODER_H_ */
