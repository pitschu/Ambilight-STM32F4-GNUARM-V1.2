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


#include "stm32f4xx.h"
#include <stdio.h>
#include "string.h"
#include "stdlib.h"
#include "math.h"

#include "hardware.h"
#include "main.h"
#include "AvrXSerialIo.h"
#include "ws2812.h"
#include "moodLight.h"
#include "IRdecoder.h"


moodLightMode_e 	moodLightMode = MLM_SINGLE_COLOR;
int					moodLightTargetFixedColor = 0;
int					moodLightMasterBrightness = 70;		// = 70%
volatile int		moodLightFaderTime = 200;			// global fading timer = 2 sec

moodLightFader_t    moodLightFader [LEDS_PHYS];				// one fader for each LED

const moodLightFixedColor_t	fixedColors[] = {
		{{255,   0,   0}, 0x58},
		{{255,  64,   0}, 0x54},
		{{255, 128,   0}, 0x50},
		{{255, 192,   0}, 0x1C},
		{{255, 255,   0}, 0x18},

		{{  0, 255,   0}, 0x59},
		{{  0, 255,  64}, 0x55},
		{{  0, 255, 128}, 0x51},
		{{  0, 255, 192}, 0x1D},
		{{  0, 255, 255}, 0x19},

		{{  0,   0, 255}, 0x45},
		{{ 64,   0, 255}, 0x49},
		{{128,   0, 255}, 0x4D},
		{{192,   0, 255}, 0x1E},
		{{255,   0, 255}, 0x1A},

		{{255, 255, 255}, 0x44},
		{{255, 192, 192}, 0x48},
		{{255, 192, 128}, 0x4C},
		{{192, 192, 255}, 0x1F},
		{{128, 192, 255}, 0x1B},
		{{  0,   0,   0}, 0xffff},
};
moodLightColor_t moodLightTargetColor = {222, 111, 44};

moodLightColor_t moodLightFade7colors[7] = {
		{255,   0,   0},
		{255, 255,   0},
		{  0, 255,   0},
		{  0,   0, 255},
		{  0, 255, 255},
		{255,   0, 255},
		{255, 255, 255},
};
int moodLightFade7colorIndex = 0;


moodLightColor_t moodLightDIYcolor[6] = {
		{255, 255,   0},
		{  0, 255, 255},
		{255,   0, 255},
		{  0,   0,   0},
		{  0,   0,   0},
		{  0,   0,   0},
};
int moodLightFadeDIYcolorIndex = 0;

int		moodLightSinusCount = 0;			// counts up to fader time
moodLightSinusSettings_t moodLightSinusDIY [6];
int moodLightSinusDIYindex = 0;

int		moodLightSinusPeriodB = 5;
int		moodLightSinusPeriodG = 10;
int		moodLightSinusPeriodR = 7;
int		moodLightSinusBaseB = 128;
int		moodLightSinusBaseG = 128;
int		moodLightSinusBaseR = 128;
int		moodLightSinusAmplitudeB = 127;
int		moodLightSinusAmplitudeG = 127;
int		moodLightSinusAmplitudeR = 127;

//------------------------------------------------------------------------------------------

void moodLightMainAction (int action)
{
	static uint32_t  lastSystemTime = 0;
	uint32_t  l = system_time;
	int i;
	int s;
	int r, g, b;

	if (lastSystemTime == l)
		return;

	masterBrightness = moodLightMasterBrightness;
	lastSystemTime = l;

	STM_EVAL_LEDOn(LED_BLU);


	if (moodLightMode == MLM_JUMP3)
	{
		if (++moodLightSinusCount > moodLightFaderTime)
			moodLightSinusCount = 0;

		for (i = 0; i < LEDS_PHYS; i++)
		{
			float f;
#define TRANSFORM_PERIOD(p)	(p <= 20 ? (float)p/5 : (float)(p-12)/2)

			f = sinf(((float)moodLightSinusCount / moodLightFaderTime) * M_TWOPI +
					M_TWOPI * (TRANSFORM_PERIOD(moodLightSinusPeriodB) * (float)((i + LEDS_PHYS/2) % LEDS_PHYS)) / (float)LEDS_PHYS) * ((float)moodLightSinusAmplitudeB) +
					(float)moodLightSinusBaseB;
			s = (int)f;
			if (s < 0) s = 0;
			if (s > 255) s = 255;
			ws2812ledRGB[i].B = (uint8_t)s;

			f = sinf(((float)moodLightSinusCount / moodLightFaderTime) * M_TWOPI +
					M_TWOPI * (TRANSFORM_PERIOD(moodLightSinusPeriodG) * (float)((i + LEDS_PHYS/2) % LEDS_PHYS)) / (float)LEDS_PHYS) * ((float)moodLightSinusAmplitudeG) +
					(float)moodLightSinusBaseG;
			s = (int)f;
			if (s < 0) s = 0;
			if (s > 255) s = 255;
			ws2812ledRGB[i].G = (uint8_t)s;

			f = sinf(((float)moodLightSinusCount / moodLightFaderTime) * M_TWOPI +
					M_TWOPI * (TRANSFORM_PERIOD(moodLightSinusPeriodR) * (float)(LEDS_PHYS - i)) / (float)LEDS_PHYS) * ((float)moodLightSinusAmplitudeR) +
					(float)moodLightSinusBaseR;
			s = (int)f;
			if (s < 0) s = 0;
			if (s > 255) s = 255;
			ws2812ledRGB[i].R = (uint8_t)s;
		}
	}


	if (moodLightMode == MLM_FADE_7)
	{
		s = 0;

		if (l - moodLightFader[0].time >= moodLightFader[0].duration)
		{
			for (i = 0; i < LEDS_PHYS; i++)
			{
				moodLightFader[i].time = l;
				moodLightFader[i].fc.B = ws2812ledRGB[i].B;
				moodLightFader[i].fc.G = ws2812ledRGB[i].G;
				moodLightFader[i].fc.R = ws2812ledRGB[i].R;
				moodLightFader[i].tc.B = moodLightFade7colors[moodLightFade7colorIndex].B;
				moodLightFader[i].tc.G = moodLightFade7colors[moodLightFade7colorIndex].G;
				moodLightFader[i].tc.R = moodLightFade7colors[moodLightFade7colorIndex].R;
			}

			if (++moodLightFade7colorIndex > 6)
				moodLightFade7colorIndex = 0;
		}
	}

	if (moodLightMode == MLM_FADE_DIY)
	{
		s = 0;

		if (l - moodLightFader[0].time >= moodLightFader[0].duration)
		{
			int k = moodLightFadeDIYcolorIndex;

			while (++k != moodLightFadeDIYcolorIndex)
			{
				if (k > 5)
					k = 0;

				if (moodLightDIYcolor[k].B + moodLightDIYcolor[k].G + moodLightDIYcolor[k].R  > 0)	// skip BLACK colors
				{
					moodLightFadeDIYcolorIndex = k;
					k = -1;
					break;
				}
			}

			if (k >= 0)			// didn´t find non BLACK color -> build one
			{
				moodLightDIYcolor[0].B = 0x80;
				moodLightDIYcolor[0].G = 0xC0;
				moodLightDIYcolor[0].R = 0xF0;
				moodLightFadeDIYcolorIndex = 0;
			}

			for (i = 0; i < LEDS_PHYS; i++)
			{
				moodLightFader[i].time = l;
				moodLightFader[i].fc.B = ws2812ledRGB[i].B;
				moodLightFader[i].fc.G = ws2812ledRGB[i].G;
				moodLightFader[i].fc.R = ws2812ledRGB[i].R;
				moodLightFader[i].tc.B = moodLightDIYcolor[moodLightFadeDIYcolorIndex].B;
				moodLightFader[i].tc.G = moodLightDIYcolor[moodLightFadeDIYcolorIndex].G;
				moodLightFader[i].tc.R = moodLightDIYcolor[moodLightFadeDIYcolorIndex].R;
			}
		}
	}


	// now fade all LEDs using the individual timer values

	STM_EVAL_LEDOff(LED_BLU);

	if (moodLightMode != MLM_JUMP3)		// only for fader modes
	{
		for (i=0; i<LEDS_PHYS; i++)
		{
			if (l - moodLightFader[i].time < moodLightFader[i].duration)		// limit not reached yet -> next color step
			{
				s = l - moodLightFader[i].time;
				if (s < 0) s = 0;
				if (s > moodLightFader[i].duration)
					s = moodLightFader[i].duration;
				b = (int)moodLightFader[i].fc.B + (((int)moodLightFader[i].tc.B - (int)moodLightFader[i].fc.B) * s) / moodLightFader[i].duration;
				g = (int)moodLightFader[i].fc.G + (((int)moodLightFader[i].tc.G - (int)moodLightFader[i].fc.G) * s) / moodLightFader[i].duration;
				r = (int)moodLightFader[i].fc.R + (((int)moodLightFader[i].tc.R - (int)moodLightFader[i].fc.R) * s) / moodLightFader[i].duration;
			}
			else
			{
				b = moodLightTargetColor.B;
				g = moodLightTargetColor.G;
				r = moodLightTargetColor.R;
			}
			ws2812ledRGB[i].B = b;
			ws2812ledRGB[i].G = g;
			ws2812ledRGB[i].R = r;
		}
	}
	STM_EVAL_LEDOn(LED_BLU);

	while (ledBusy)				// wait until last DMA is ready
		;
	WS2812update();
	STM_EVAL_LEDOff(LED_BLU);

}



void moodLightUpdateTimers (void)
{
	int i;

	if (moodLightMode == MLM_FADE_7 || moodLightMode == MLM_FADE_DIY)
	{
		for (i = 0; i < LEDS_PHYS; i++)
		{
			moodLightFader[i].duration = moodLightFaderTime;
		}
	}
}



void moodLightUpdateFixedColor (void)
{
	uint32_t  l = system_time;
	int j;

	for (j = 0; j < LEDS_PHYS; j++)
	{
		moodLightFader[j].fc.B = ws2812ledRGB[j].B;
		moodLightFader[j].fc.G = ws2812ledRGB[j].G;
		moodLightFader[j].fc.R = ws2812ledRGB[j].R;
		moodLightFader[j].tc.B = moodLightTargetColor.B;
		moodLightFader[j].tc.G = moodLightTargetColor.G;
		moodLightFader[j].tc.R = moodLightTargetColor.R;
		moodLightFader[j].duration = 5;
		moodLightFader[j].time = l;
	}

}


typedef enum {
	SIN_NOTHING = 0,
	SIN_BASEBRITHNESS,
	SIN_AMPLITUDE,
	SIN_PERIOD
} sinusParameterMode_e;


int moodLightHandleIRcode ()
{
	int i;
	short handled = 1;
	static sinusParameterMode_e irSinusMode = SIN_NOTHING;

	if (irCode.isNew != IR_RELEASED && irCode.isNew != IR_AUTORPT)	//
		return (0);

	switch (irCode.code)
	{
	case JUMP3_KEY:
		if (moodLightMode != MLM_JUMP3)
		{
			moodLightMode = MLM_JUMP3;
		}
		break;

	case FADE7_KEY:
		if (moodLightMode != MLM_FADE_7)
		{
			moodLightMode = MLM_FADE_7;
			//			moodLightFade7colorIndex = 0;

			for (i = 0; i < LEDS_PHYS; i++)
			{
				moodLightFader[i].duration = moodLightFaderTime;
				moodLightFader[i].time = 0;			// force color update
			}
		}
		break;

	case FADE3_KEY:
		if (moodLightMode != MLM_FADE_DIY)
		{
			moodLightMode = MLM_FADE_DIY;
			//			moodLightFadeDIYcolorIndex = 0;

			for (i = 0; i < LEDS_PHYS; i++)
			{
				moodLightFader[i].duration = moodLightFaderTime;
				moodLightFader[i].time = 0;			// force color update
			}
		}
		break;

	case RED_HI:
	case RED_LO:
	case BLUE_HI:
	case BLUE_LO:
	case GREEN_HI:
	case GREEN_LO:
	case AUTO_KEY:
		if (moodLightMode == MLM_JUMP3)
		{
			switch (irSinusMode)
			{
			case SIN_NOTHING:
				break;
			case SIN_BASEBRITHNESS:
				switch (irCode.code)
				{
				case AUTO_KEY:
					moodLightSinusBaseR = 128;
					moodLightSinusBaseG = 128;
					moodLightSinusBaseB = 128;
					break;
				case RED_HI:
					if (moodLightSinusBaseR < 240) moodLightSinusBaseR += 16; else moodLightSinusBaseR = 255;	displayOverlayPercents(((int)moodLightSinusBaseR*100)/255, 300); break;
				case RED_LO:
					if (moodLightSinusBaseR >15) moodLightSinusBaseR -= 16; else moodLightSinusBaseR = 0;	displayOverlayPercents(((int)moodLightSinusBaseR*100)/255, 300); break;
				case BLUE_HI:
					if (moodLightSinusBaseB < 240) moodLightSinusBaseB += 16; else moodLightSinusBaseB = 255;	displayOverlayPercents(((int)moodLightSinusBaseB*100)/255, 300); break;
				case BLUE_LO:
					if (moodLightSinusBaseB >15) moodLightSinusBaseB -= 16; else moodLightSinusBaseB = 0;	displayOverlayPercents(((int)moodLightSinusBaseB*100)/255, 300); break;
				case GREEN_HI:
					if (moodLightSinusBaseG < 240) moodLightSinusBaseG += 16; else moodLightSinusBaseG = 255;	displayOverlayPercents(((int)moodLightSinusBaseG*100)/255, 300); break;
				case GREEN_LO:
					if (moodLightSinusBaseG >15) moodLightSinusBaseG -= 16; else moodLightSinusBaseG = 0;	displayOverlayPercents(((int)moodLightSinusBaseG*100)/255, 300); break;
				}
				break;

				case SIN_PERIOD:
					switch (irCode.code)
					{
					case AUTO_KEY:
						moodLightSinusPeriodR = 5;
						moodLightSinusPeriodG = 10;
						moodLightSinusPeriodB = 7;
						break;
					case RED_HI:
						if (moodLightSinusPeriodR < 100) moodLightSinusPeriodR += 1; else moodLightSinusPeriodR = 100;	displayOverlayPercents(((int)moodLightSinusPeriodR), 300); break;
					case RED_LO:
						if (moodLightSinusPeriodR >1) moodLightSinusPeriodR -= 1; else moodLightSinusPeriodR = 1;		displayOverlayPercents(((int)moodLightSinusPeriodR), 300); break;
					case BLUE_HI:
						if (moodLightSinusPeriodB < 100) moodLightSinusPeriodB += 1; else moodLightSinusPeriodB = 100;	displayOverlayPercents(((int)moodLightSinusPeriodB), 300); break;
					case BLUE_LO:
						if (moodLightSinusPeriodB >1) moodLightSinusPeriodB -= 1; else moodLightSinusPeriodB = 1;		displayOverlayPercents(((int)moodLightSinusPeriodB), 300); break;
					case GREEN_HI:
						if (moodLightSinusPeriodG < 100) moodLightSinusPeriodG += 1; else moodLightSinusPeriodG = 100;	displayOverlayPercents(((int)moodLightSinusPeriodG), 300); break;
					case GREEN_LO:
						if (moodLightSinusPeriodG >1) moodLightSinusPeriodG -= 1; else moodLightSinusPeriodG = 1;		displayOverlayPercents(((int)moodLightSinusPeriodG), 300); break;
					}

					break;

					case SIN_AMPLITUDE:
						switch (irCode.code)
						{
						case AUTO_KEY:
							moodLightSinusAmplitudeR = 127;
							moodLightSinusAmplitudeG = 127;
							moodLightSinusAmplitudeB = 127;
							break;
						case RED_HI:
							if (moodLightSinusAmplitudeR < 120) moodLightSinusAmplitudeR += 8; else moodLightSinusAmplitudeR = 127;	displayOverlayPercents(((int)moodLightSinusAmplitudeR*100)/127, 300); break;
						case RED_LO:
							if (moodLightSinusAmplitudeR >8) moodLightSinusAmplitudeR -= 8; else moodLightSinusAmplitudeR = 0;		displayOverlayPercents(((int)moodLightSinusAmplitudeR*100)/127, 300); break;
						case BLUE_HI:
							if (moodLightSinusAmplitudeB < 120) moodLightSinusAmplitudeB += 8; else moodLightSinusAmplitudeB = 127;	displayOverlayPercents(((int)moodLightSinusAmplitudeB*100)/127, 300); break;
						case BLUE_LO:
							if (moodLightSinusAmplitudeB >8) moodLightSinusAmplitudeB -= 8; else moodLightSinusAmplitudeB = 0;		displayOverlayPercents(((int)moodLightSinusAmplitudeB*100)/127, 300); break;
						case GREEN_HI:
							if (moodLightSinusAmplitudeG < 120) moodLightSinusAmplitudeG += 8; else moodLightSinusAmplitudeG = 127;	displayOverlayPercents(((int)moodLightSinusAmplitudeG*100)/127, 300); break;
						case GREEN_LO:
							if (moodLightSinusAmplitudeG >8) moodLightSinusAmplitudeG -= 8; else moodLightSinusAmplitudeG = 0;		displayOverlayPercents(((int)moodLightSinusAmplitudeG*100)/127, 300); break;
						}
						break;

			}
		}
		else
		{
			moodLightMode = MLM_SINGLE_COLOR;
			moodLightTargetColor.B =  ws2812ledRGB[LEDS_Y + LEDS_X/2].B;
			moodLightTargetColor.G =  ws2812ledRGB[LEDS_Y + LEDS_X/2].G;
			moodLightTargetColor.R =  ws2812ledRGB[LEDS_Y + LEDS_X/2].R;
			switch (irCode.code)
			{
			case RED_HI:
				if (moodLightTargetColor.R < 240) moodLightTargetColor.R += 16; else moodLightTargetColor.R = 255;
				displayOverlayPercents(((int)moodLightTargetColor.R*100)/255, 300); break;
			case RED_LO:
				if (moodLightTargetColor.R >15) moodLightTargetColor.R -= 16; else moodLightTargetColor.R = 0;
				displayOverlayPercents(((int)moodLightTargetColor.R*100)/255, 300); break;
			case BLUE_HI:
				if (moodLightTargetColor.B < 240) moodLightTargetColor.B += 16; else moodLightTargetColor.B = 255;
				displayOverlayPercents(((int)moodLightTargetColor.B*100)/255, 300); break;
			case BLUE_LO:
				if (moodLightTargetColor.B >15) moodLightTargetColor.B -= 16; else moodLightTargetColor.B = 0;
				displayOverlayPercents(((int)moodLightTargetColor.B*100)/255, 300); break;
			case GREEN_HI:
				if (moodLightTargetColor.G < 240) moodLightTargetColor.G += 16; else moodLightTargetColor.G = 255;
				displayOverlayPercents(((int)moodLightTargetColor.G*100)/255, 300); break;
			case GREEN_LO:
				if (moodLightTargetColor.G >15) moodLightTargetColor.G -= 16; else moodLightTargetColor.G = 0;
				displayOverlayPercents(((int)moodLightTargetColor.G*100)/255, 300); break;
			}
			moodLightUpdateFixedColor();
		}
		break;

	case BRIGHTNESS_HI:
		if (moodLightMasterBrightness < 97)
		{
			if (moodLightMasterBrightness > 20)
				moodLightMasterBrightness += 4;
			else
				moodLightMasterBrightness += 1;
		}
		else
			moodLightMasterBrightness = 100;
		displayOverlayPercents(moodLightMasterBrightness, 300);
		break;
	case BRIGHTNESS_LO:
		if (moodLightMasterBrightness > 1)
		{
			if (moodLightMasterBrightness >= 20)
				moodLightMasterBrightness -= 4;
			else
				moodLightMasterBrightness -= 1;
		}
		else
			moodLightMasterBrightness = 1;
		displayOverlayPercents(moodLightMasterBrightness, 300);
		break;
	case QUICK_KEY:
		if (moodLightFaderTime >= (6*100))
			moodLightFaderTime -= 100;
		else if (moodLightFaderTime >= 50)
			moodLightFaderTime -= 25;
		moodLightUpdateTimers();
		displayOverlayPercents(moodLightFaderTime/30, 300);
		break;

	case SLOW_KEY:
		if (moodLightFaderTime < 25)
			moodLightFaderTime  = 25;
		else if (moodLightFaderTime < (5 * 100))
			moodLightFaderTime += 25;
		else if (moodLightFaderTime < (30 * 100))
			moodLightFaderTime += 100;
		moodLightUpdateTimers();
		displayOverlayPercents(moodLightFaderTime/30, 300);
		break;

	case DIY_1_KEY:
	case DIY_2_KEY:
	case DIY_3_KEY:
	case DIY_4_KEY:
	case DIY_5_KEY:
	case DIY_6_KEY:
	{
		int diyNum = 0;
		switch (irCode.code)
		{
		case DIY_1_KEY: diyNum = 0; break;
		case DIY_2_KEY: diyNum = 1; break;
		case DIY_3_KEY: diyNum = 2; break;
		case DIY_4_KEY: diyNum = 3; break;
		case DIY_5_KEY: diyNum = 4; break;
		case DIY_6_KEY: diyNum = 5; break;
		}

		if (irCode.isNew == IR_AUTORPT && irCode.repcntPressed > AUTO_RPT_INITIAL)	// long press -> activate DIY color
			displayOverlayPercents(50, 50);
		if (irCode.isNew == IR_AUTORPT && irCode.repcntPressed > AUTO_RPT_INITIAL+15)
			displayOverlayPercents(60, 0);

		if (irCode.isNew == IR_RELEASED && irCode.repcntPressed < AUTO_RPT_INITIAL)	// short press -> activate DIY color
		{
			if (moodLightMode == MLM_JUMP3)
			{
				moodLightSinusPeriodB = moodLightSinusDIY[diyNum].periodB;
				moodLightSinusPeriodG = moodLightSinusDIY[diyNum].periodG;
				moodLightSinusPeriodR = moodLightSinusDIY[diyNum].periodR;
				moodLightSinusBaseB = moodLightSinusDIY[diyNum].baseB;
				moodLightSinusBaseG = moodLightSinusDIY[diyNum].baseG;
				moodLightSinusBaseR = moodLightSinusDIY[diyNum].baseR;
				moodLightSinusAmplitudeB = moodLightSinusDIY[diyNum].amplitudeB;
				moodLightSinusAmplitudeG = moodLightSinusDIY[diyNum].amplitudeG;
				moodLightSinusAmplitudeR = moodLightSinusDIY[diyNum].amplitudeR;
			}
			else
			{
				moodLightMode = MLM_SINGLE_COLOR;
				moodLightTargetColor.B =  moodLightDIYcolor[diyNum].B;
				moodLightTargetColor.G =  moodLightDIYcolor[diyNum].G;
				moodLightTargetColor.R =  moodLightDIYcolor[diyNum].R;
				moodLightUpdateFixedColor();
			}
		}
		else if (irCode.isNew == IR_RELEASED && irCode.repcntPressed > AUTO_RPT_INITIAL + 15)  // very long -> save BLACK
		{
			if (moodLightMode != MLM_JUMP3)
			{
				moodLightDIYcolor[diyNum].B =  0;
				moodLightDIYcolor[diyNum].G =  0;
				moodLightDIYcolor[diyNum].R =  0;
			}
		}
		else if (irCode.isNew == IR_RELEASED && irCode.repcntPressed > AUTO_RPT_INITIAL)	// save current color of central LED to DIY memory
		{
			if (moodLightMode == MLM_JUMP3)
			{
				moodLightSinusDIY[diyNum].periodB = moodLightSinusPeriodB;
				moodLightSinusDIY[diyNum].periodG = moodLightSinusPeriodG;
				moodLightSinusDIY[diyNum].periodR = moodLightSinusPeriodR;

				moodLightSinusDIY[diyNum].baseB = moodLightSinusBaseB;
				moodLightSinusDIY[diyNum].baseG = moodLightSinusBaseG;
				moodLightSinusDIY[diyNum].baseR = moodLightSinusBaseR;

				moodLightSinusDIY[diyNum].amplitudeB = moodLightSinusAmplitudeB;
				moodLightSinusDIY[diyNum].amplitudeG = moodLightSinusAmplitudeG;
				moodLightSinusDIY[diyNum].amplitudeR = moodLightSinusAmplitudeR;
			}
			else
			{
				moodLightDIYcolor[diyNum].B =  ws2812ledRGB[LEDS_Y + LEDS_X/2].B;
				moodLightDIYcolor[diyNum].G =  ws2812ledRGB[LEDS_Y + LEDS_X/2].G;
				moodLightDIYcolor[diyNum].R =  ws2812ledRGB[LEDS_Y + LEDS_X/2].R;
			}
			displayOverlayPercents(50, 20);
		}
	}
	break;

	case GREEN_KEY:
	case BLUE_KEY:
	case WHITE_KEY:
		if (moodLightMode == MLM_JUMP3)
		{
			switch (irCode.code)
			{
			case GREEN_KEY:
				irSinusMode = SIN_PERIOD; break;
			case BLUE_KEY:
				irSinusMode = SIN_AMPLITUDE; break;
			case WHITE_KEY:
				irSinusMode = SIN_BASEBRITHNESS; break;
			}
		}
		else			// handle again as standard color
			handled = 0;
		break;


	default:
		handled = 0;
		break;
	}

	if (handled)
		return (0);

	i = 0;
	while (fixedColors[i].irCode != 0xffff)
	{
		if (irCode.code == fixedColors[i].irCode)		// is a fixed color code
		{
			moodLightMode = MLM_SINGLE_COLOR;
			moodLightTargetColor.B =  fixedColors[i].color.B;
			moodLightTargetColor.G =  fixedColors[i].color.G;
			moodLightTargetColor.R =  fixedColors[i].color.R;
			moodLightUpdateFixedColor();
			return (0);
		}
		i++;
	}


	return (0);
}

