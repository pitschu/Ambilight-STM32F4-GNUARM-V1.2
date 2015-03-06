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


#ifndef MAIN_H_
#define MAIN_H_

#include "hardware.h"
#include "AvrXSerialIo.h"
#include "ws2812.h"
#include "tvp5150_dcmi.h"
#include "stm32_ub_usb_cdc.h"


/*
 * Ressource usage:

TVP5150 TV processor:
	  PA4  -> DCMI_HSYNC  = TVP5150 HSYNC (HREF)
	  PA6  -> DCMI_PCLK   = TVP5150 PCLK (PIXCLK)
	  PB6  -> DCMI_D5     = TVP5150 D5
	  PB7  -> DCMI_VSYNC  = TVP5150 VSYNC
	  PB8  -> I2C1-SCL    = TVP5150 I2C-SCL
	  PB9  -> I2C1-SDA    = TVP5150 I2C-SDA
	  PC6  -> DCMI_D0     = TVP5150 D0
	  PC7  -> DCMI_D1     = TVP5150 D1
	  PC8  -> DCMI_D2     = TVP5150 D2
	  PC9  -> DCMI_D3     = TVP5150 D3
	  PE4  -> DCMI_D4     = TVP5150 D4
	  PE5  -> DCMI_D6     = TVP5150 D6
	  PE6  -> DCMI_D7     = TVP5150 D7 (!! I2C select during reset: 0 = 0xB8, 1 = 0xBA)

	  PE12 -> gpio (in)	  = TVP5150 AVID
	  PE13 -> gpio (in)	  = TVP5150 VBLK
	  PE14 -> gpio (out)  = TVP5150 Reset
	  PE15 -> gpio (in)	  = TVP5150 FID

	  DMA : entweder DMA2_STREAM1_CHANNEL1
			oder     DMA2_STREAM7_CHANNEL1

RGB-LED:
	  uses TIM3 Channel 3
	  PB0  -> TM3_CH3	  = PWM output to WS2812 led strip

	  DMA : uses DMA1_Strem7_Channel5

IR-Decoder
	  PE9  -> TM1_CH1	  = Input capture compare


Timers used:
	TIM1					IR-Decoder (input capture mode)
	TIM3 + DMA1_S7_C5		PWM out to WS2812  (output compare mode with DMA for next oc value)
	TIM4					timer for delay_ms() and delay_us() funcs
--------------------------------------------------------------*/


// factor for rgbImage integrators
#define MAX_ICONTROL	128

extern short	videoOffCount;		// when counted down to 0, video is lost

typedef enum {
	MODE_MOODLIGHT = 0,
	MODE_AMBILIGHT = 1,
	MODE_STANDBY
} mainMode_e;

extern mainMode_e	mainMode;
extern mainMode_e	stdbyMode;

extern int 			frameWidth;			// number of slots to aggregate for LED stripe
extern int			masterBrightness;

extern void displayOverlayPercents (int percent, int duration);


#endif /* MAIN_H_ */
