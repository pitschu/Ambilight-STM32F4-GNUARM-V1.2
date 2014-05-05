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
 *	19.11.2013	pitschu 	first release
 *	05.05.2014	pitschu	v1.1 supports blue user button (mode switch/standby)
 */

#include "stm32f4xx.h"
#include <stdio.h>
#include "string.h"
#include "stdlib.h"

#include "hardware.h"
#include "main.h"
#include "AvrXSerialIo.h"
#include "ws2812.h"
#include "tvp5150_dcmi.h"
#include "IRdecoder.h"
#include "ambiLight.h"
#include "moodLight.h"
#include "stm32_ub_usb_cdc.h"
#include "flashparams.h"


extern void IRdecoderInit(void);
extern int  UserInterface(void);
extern volatile uint32_t system_time;


unsigned char status1 = 0;

short				videoOffCount = 5;		// when counted down to 0, video is lost

mainMode_e	mainMode = 		MODE_STANDBY;
mainMode_e	stdbyMode =		MODE_AMBILIGHT;	// change to stdbyMode when system is activated

int			masterBrightness = 100;

//----------------------------------------------------------------------------------------------------------



int main(void)
{
	unsigned long signalDetectTimer = system_time + 50;
	unsigned long flashUpdateTimer;			// check after 3 seconds

	SystemInit();
	SystemCoreClockUpdate();
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
	RNG_Cmd (ENABLE);
	while (RNG_GetFlagStatus(RNG_FLAG_DRDY) != SET)
		;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
	CRC_ResetDR();

	FLASH_Unlock();

	init_systick ();		// runs at 100Hz

	BoardHardwareInit ();
	DelayCountInit ();
	IRdecoderInit();

	// Init vom USB-OTG-Port als CDC-Device (Virtueller-ComPort)
	UB_USB_CDC_Init();

	USART3_Init (115200);
	delay_ms(3000);			// delay 3sec to give user a chance for starting PuTTY

	write_str2Host("Hi there. This is pitschu's AmbiLight V1.1\n\r");
	/* Initialize LEDs and User_Button on STM32F4-Discovery --------------------*/
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);

	STM_EVAL_LEDInit(LED_GRN);
	STM_EVAL_LEDInit(LED_ORN);
	STM_EVAL_LEDInit(LED_RED);
	STM_EVAL_LEDInit(LED_BLU);

	//-------------------------------------------------
	// http://www.mikrocontroller.net/topic/261021
#include "math.h"
	CORE_CycleCounEn();
	// TEST only: check for FPU is present and working
	vu32 it = CORE_GetCycleCount();
	float f = 1.01f * RNG_GetRandomNumber();;
	float f2 = f * 2.29f;
	vu32 it2 = CORE_GetCycleCount() - it;
	printf ("FPU test: Mult: %d\n", (int)it2);

	it = CORE_GetCycleCount ();
	f2 = sinf (f);
	it2 = CORE_GetCycleCount() - it;
	printf ("FPU test: Sinus:%g, cycles used %d\n", (double)f2, (int)it2);
	CORE_CycleCounDis();
	//-------------------------------------------------

	STM_EVAL_LEDOn(LED_BLU);
	WS2812init();				// init IO for RGB leds
	WS2812test();
	STM_EVAL_LEDOff(LED_BLU);

	I2C_InitHardware();				// init I2C bus to TVP5150
	TVP5150init();				// init DCMI interface and start TVP5150

	ambiLightInit();

	initFlashParamBlock();			// check for valid flash parameter block

	checkForParamChanges ();		// calc param CRC for later checks
	flashUpdateTimer = UINT32_MAX;	// no need to update now

	delay_ms(100);
	TVP5150startCapture();

	while (1)			// main loop
	{
		int i;

		while (mainMode != MODE_AMBILIGHT || captureReady == 0)		// time to handle input
		{
			if (system_time > signalDetectTimer)		// check every 500ms
			{
				signalDetectTimer = system_time + 50;

				unsigned char s = TVP5150getStatus1 ();
				if (s != status1)
				{
					printf("\nVideo status changed: %02X at source %d, mode = %d\n",
							(int)s, (int)videoCurrentSource, (int)videoSourceSelect);
					status1 = s;
				}

				if (TVP5150hasVideoSignal() == 0)
				{
					if (videoOffCount > 0)
					{
						if (videoOffCount == 1)
						{
							printf("\nNo video signal at source %d, mode = %d\n", (int)videoCurrentSource, (int)videoSourceSelect);
							for (i = 0; i < LEDS_MAXTOTAL; i++)		// blank all leds
							{
								ws2812ledRGB[i].R = 0;
								ws2812ledRGB[i].G = 0;
								ws2812ledRGB[i].B = 0;
							}

							if (videoSourceSelect == 0)			// no video connected -> switch source when auto select mode
							{
								short newSource = (videoCurrentSource == 1 ? 2 : 1);

								printf("\nAuto switch source to %d\n", (int)newSource);
								TVP5150selectVideoSource(newSource);
								// set green channel scan indicator
								ws2812ledRGB[newSource == 1 ? 0 : ledsY+ledsX+ledsY-1].G = 0x20;

								videoOffCount = 5;				// start search
							}
						}
						videoOffCount--;
					}
				}
				else
					videoOffCount = 5;

				if (checkForParamChanges() != 0)		// some parameter was changed -> delay flash write; maybe other changes follow
					flashUpdateTimer = system_time + 800;
			}

#ifdef USE_USB
			{
				int16_t c;
				if(UB_USB_CDC_GetStatus() != USB_CDC_CONNECTED)
					c = -1;
				else
					c = UB_VCP_CharRx ();

				if (c > 0)
					AvrXPutFifo(fifoFromHost, (uint8_t)c);
			}
#endif
			UserInterface();				// handle user input from UART/USB

			{
				extern volatile uint16_t	butONcount;		// simulate IR-codes when blue on-board button is pressed

				if (butONcount > 150)		// long press -> standby
				{
					irCode.isNew = IR_AUTORPT;
					irCode.code = ONOFF_KEY;
					irCode.repcntPressed = 20;
					butONcount = 0;
				}
				else if (butONcount > 20)		// short press -> toggle mode
				{
					irCode.isNew = IR_RELEASED;
					irCode.code = ONOFF_KEY;
					irCode.repcntPressed = 1;
					butONcount = 0;
				}

			}

			if (irCode.isNew == IR_AUTORPT || irCode.isNew == IR_RELEASED)
			{
				printf ("IR data: %04X, rep count: %d\n", (int)irCode.code, (int)irCode.repcntPressed);

				if (irCode.code == ONOFF_KEY)
				{
					if (irCode.repcntPressed > 12)			// long press -> goto stand by
					{
						if (mainMode != MODE_STANDBY)
						{
							stdbyMode = mainMode;
							mainMode = MODE_STANDBY;
							for (i = 0; i < LEDS_MAXTOTAL; i++)		// blank all leds
							{
								ws2812ledRGB[i].R = 0;
								ws2812ledRGB[i].G = 0;
								ws2812ledRGB[i].B = 0;
							}
							displayOverlayPercents (100, 200);
						}
					}
					else if (irCode.isNew == IR_RELEASED)
					{
						if (mainMode == MODE_STANDBY)
							mainMode = stdbyMode;
						else
							mainMode = (mainMode == MODE_MOODLIGHT ? MODE_AMBILIGHT : MODE_MOODLIGHT);

						for (i = 0; i < LEDS_MAXTOTAL; i++)		// blank all leds
						{
							ws2812ledRGB[i].R = 0;
							ws2812ledRGB[i].G = 0;
							ws2812ledRGB[i].B = 0;
						}
						displayOverlayPercents (100, 100);
					}

					printf ("Mood/Ambi mode switched to %02X\n", (int)mainMode);
				}
				else
				{
					switch (mainMode)
					{
					case MODE_MOODLIGHT:
						moodLightHandleIRcode();
						break;
					case MODE_AMBILIGHT:
						ambiLightHandleIRcode ();
						break;
					case MODE_STANDBY:
						if (irCode.isNew == IR_RELEASED && irCode.repcntPressed < 1)
						{
							mainMode = stdbyMode;				// restore mode
							printf ("Mood/Ambi mode switched to %02X\n", (int)mainMode);
						}
						break;
					}
				}
				irCode.isNew = IR_CHECKED;
			}


			if (mainMode == MODE_MOODLIGHT)
			{
				moodLightMainAction (0);
			}


			if (mainMode == MODE_STANDBY)
			{
				static uint32_t  lastSystemTime = 0;
				uint32_t  l = system_time;

				if (lastSystemTime != l)
				{
					lastSystemTime = l;
					while (ledBusy)				// wait until last DMA is ready
						;
					WS2812update();
				}
			}

			if (system_time > flashUpdateTimer)		// check parameter update every 3 secs
			{
				updateAllParamsToFlash(0);			// check for changes and write params to flash
				flashUpdateTimer = UINT32_MAX;
			}

		}									// while captureReady == 0


		/*
		 * A new frame has been captured. Transform raw RGB values to scaled image
		 */
		STM_EVAL_LEDOn(LED_BLU);
		ambiLightSlots2Dyn();			// update dyn matrix and find the non-black area
		STM_EVAL_LEDToggle (LED_BLU);
		ambiLightDyn2Image();			// transform the non-black area to the virtual X * Y image
		STM_EVAL_LEDToggle (LED_BLU);

		captureReady = 0;

		if (mainMode == MODE_AMBILIGHT)
		{
			if (videoOffCount >= 5)			// we have a good video signal
				ambiLightImage2LedRGB();	// expand the virtual image to the physical LED image

			STM_EVAL_LEDToggle (LED_BLU);

			while (ledBusy)				// wait until last DMA is ready
				;
			WS2812update();
		}
		STM_EVAL_LEDOff    (LED_BLU);
	}

}


void displayOverlayPercents (int percent, int duration)
{
	int i;

	if (duration > 0)			// else only set additional overlay leds
	{
		for (i = 0; i < ledsPhysical; i++)
		{
			ws2812ledOVR[i].B = 0;
			ws2812ledOVR[i].G = 0;
			ws2812ledOVR[i].R = 0;
			ws2812ledHasOVR[i] = 0;
		}
		ws2812ovrlayCounter = duration;
	}

	i = ledsY + ledsX - 1 - (((ledsX-3) * percent)/100);				// led index on top side (0 = left, 100 = right)

	ws2812ledHasOVR[i-2] = 1;
	ws2812ledOVR[i-2].B = 0;
	ws2812ledOVR[i-2].G = 0;
	ws2812ledOVR[i-2].R = 0;

	ws2812ledHasOVR[i-1] = 1;
	ws2812ledOVR[i-1].B = 0;
	ws2812ledOVR[i-1].G = 0xFF;
	ws2812ledOVR[i-1].R = 0;

	ws2812ledHasOVR[i] = 1;
	ws2812ledOVR[i].B = 0;
	ws2812ledOVR[i].G = 0xFF;
	ws2812ledOVR[i].R = 0;

	ws2812ledHasOVR[i+1] = 1;
	ws2812ledOVR[i+1].B = 0;
	ws2812ledOVR[i+1].G = 0;
	ws2812ledOVR[i+1].R = 0;

}





#ifdef  USE_FULL_ASSERT

/**
 * @brief  Reports the name of the source file and the source line number
 *   where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	printf("Wrong parameters value: file %s on line %d\r\n", file, (int)line);
}
#endif
