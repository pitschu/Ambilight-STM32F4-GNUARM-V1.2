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
 *	05.05.2014	pitschu	v1.1 supports dynamic X/Y LED strip size
 */

#include "stm32f4xx.h"
#include <stdio.h>
#include "string.h"
#include "stdlib.h"

#include "hardware.h"
#include "main.h"
#include "AvrXSerialIo.h"
#include "ws2812.h"
#include "ambiLight.h"
#include "IRdecoder.h"


// rgbImage is a scaled imgae of the raw video image blocks. It can be sized from 1x1 to 64x40
rgbIcontroller_t	rgbImage [2*SLOTS_X + 2*SLOTS_Y];
short				rgbImageWid = SLOTS_X;
short				rgbImageHigh = SLOTS_Y;

short 				factorI = 32;			// Integration speed: 128 = max speed (online); 1 = very slow, over 128 frames

// the dynXYZ values build the dynamic border of the ´none black´ rgbSlots rectangle. They are permanently adapted to the
// picture content. Black or nearly black borders are cut off.
short				dynLeft 	= 0;
short				dynRight 	= SLOTS_X-1;
short				dynTop 		= 0;
short				dynBottom 	= SLOTS_Y-1;

dynRGBsum_t			dynColumns[SLOTS_X];
dynRGBsum_t			dynRows[SLOTS_Y];
unsigned short		dynFrames = 0;			// number of frames integrated in dynXY vars
unsigned short		dynFramesLimit = 100;	// pitschu v1.2: Added limit value

short				dynBlackLevel;
long				dynBlackLevelInt;
short				dynWhiteLevel;
long				dynWhiteLevelInt;

int 				frameWidth = 4;			// number of slots to aggregate for LED stripe

rgbValue_t 			tvprocRGBDelayFifo[DELAY_LINE_SIZE][LEDS_MAXTOTAL];			// for TV picture proc delays; up to 1 second
short  				tvprocDelayW;			// delay FIFO write pointer
short  				tvprocDelayTime;		// difference between write an read pointr

//----------------------------------------------------------------------------------------------------------




void ambiLightInit (void)
{
	int i, j;

	dynLeft 	= 0;
	dynRight 	= SLOTS_X-1;
	dynTop 		= 0;
	dynBottom 	= SLOTS_Y-1;
	memset ((char*)&dynColumns[0], sizeof(dynColumns), 0);
	memset ((char*)&dynRows[0], sizeof(dynRows), 0);
	dynBlackLevel = INT16_MAX;
	dynBlackLevelInt = 0;;
	dynWhiteLevel = 0;
	dynWhiteLevelInt = 0;
	dynFrames = 0;

	for (j = 0; j < DELAY_LINE_SIZE; j++)
	{
		for (i = 0; i < LEDS_MAXTOTAL; i++)
		{
			tvprocRGBDelayFifo[j][i].B = 0;
			tvprocRGBDelayFifo[j][i].G = 0;
			tvprocRGBDelayFifo[j][i].R = 0;
		}
	}

	tvprocDelayTime = 0;		// no delay
	tvprocDelayW = 0;

}


int  ambiLightHandleIRcode ()
{
	int s;
	static uint32_t		lastPress = 0;


	switch (irCode.code)
	{
	case MODEKEY_BLACK:
		if (irCode.isNew == IR_RELEASED)			// no auto repeat codes allowed here
		{
			if (system_time - lastPress < 400)		// second press withon 4 seconds
			{
				s = videoSourceSelect + 1;

				if (s > 2)
					s = 0;

				if (s == 0)
				{
					unsigned char i = TVP5150hasVideoSignal();

					if (i == 0 || videoSourceSelect != s)		// no signal -> switch source
					{
						TVP5150selectVideoSource(videoCurrentSource == 1 ? 2 : 1);
					}
				}
				else
				{
					TVP5150selectVideoSource(s);
				}

				videoSourceSelect = s;
			}									// else: if first press then show info only

			lastPress = system_time;

			printf("\nCurrent video source is %d, mode = %d\n", (int)videoCurrentSource, (int)videoSourceSelect);
			videoOffCount = 5;				// start search
			if (videoSourceSelect > 0)
				displayOverlayPercents(videoSourceSelect == 1 ? 100 : 0, 300);
			else
			{
				displayOverlayPercents(0, 300);
				displayOverlayPercents(100, 0);
			}
		}
		break;

		// simulate UART characters to set params (see userinterface.c)
	case BRIGHTNESS_HI:
		AvrXPutFifo(fifoFromHost, '+');
		break;
	case BRIGHTNESS_LO:
		AvrXPutFifo(fifoFromHost, '-');
		break;
	case AUTO_KEY:
		AvrXPutFifo(fifoFromHost, 'd');		// set default
		break;
	case RED_KEY:
		AvrXPutFifo(fifoFromHost, 'F');		// set hue control
		break;
	case GREEN_KEY:
		AvrXPutFifo(fifoFromHost, 'S');		// set saturation
		break;
	case BLUE_KEY:
		AvrXPutFifo(fifoFromHost, 'C');		// set contrast
		break;
	case WHITE_KEY:
		AvrXPutFifo(fifoFromHost, 'B');		// set brightness
		break;
	case SLOW_KEY:
		AvrXPutFifo(fifoFromHost, 'I');		// set integration time
		AvrXPutFifo(fifoFromHost, '-');		// increase
		break;
	case QUICK_KEY:
		AvrXPutFifo(fifoFromHost, 'I');		// set integration time
		AvrXPutFifo(fifoFromHost, '+');		// decrease
		break;
	case RED_HI:
		AvrXPutFifo(fifoFromHost, 'L');		// set left border
		break;
	case RED_LO:
		AvrXPutFifo(fifoFromHost, 'W');		// set width (right border)
		break;
	case GREEN_HI:
		AvrXPutFifo(fifoFromHost, 'T');		// set top border
		break;
	case GREEN_LO:
		AvrXPutFifo(fifoFromHost, 'H');		// set image height (bottom border)
		break;
	case BLUE_HI:
		AvrXPutFifo(fifoFromHost, 'X');		// set image blocks X
		break;
	case BLUE_LO:
		AvrXPutFifo(fifoFromHost, 'Y');		// set image blocks Y
		break;
	case FLASH_KEY:
		AvrXPutFifo(fifoFromHost, 'E');		// set # of blocks to aggregate for LED
		break;
	case FADE7_KEY:
		AvrXPutFifo(fifoFromHost, 'M');		// set frame delay
		break;
	case JUMP3_KEY:							// pitschu 140505
		AvrXPutFifo(fifoFromHost, 'A');		// set AGC mode
		break;

	}
	return (0);
}




void ambiLightClearImage (void)
{
	memset((void*)&rgbImage[0], 0, sizeof (rgbImage));
}



void ambiLightPrintDynInfos (void)
{
	int x, y;

	printf("\nCurrent dynRows:");
	printf("\n AVG:");
	for (y = 0; y < SLOTS_Y; y++)
		printf ("%4d", dynRows[y].rgbAvg);
	printf("\n CON:");
	for (y = 0; y < SLOTS_Y; y++)
		printf ("%4d", dynRows[y].rgbContrast);
	printf("\n ACH:");
	for (y = 0; y < SLOTS_Y; y++)
		printf ("%4d", dynRows[y].rgbAvgChange);
	printf("\n CCH:");
	for (y = 0; y < SLOTS_Y; y++)
		printf ("%4d", dynRows[y].rgbConChange);
	printf ("\n dynTop = %04d, dynBot = %4d", dynTop, dynBottom);
	printf ("\n Black level = %4d\n", dynBlackLevel);
	printf("\n");

	printf("\nCurrent dynCols:");
	printf("\n AVG:");
	for (x = 0; x < SLOTS_X; x++)
		printf ("%4d", dynColumns[x].rgbAvg);
	printf("\n CON:");
	for (x = 0; x < SLOTS_X; x++)
		printf ("%4d", dynColumns[x].rgbContrast);
	printf("\n ACH:");
	for (x = 0; x < SLOTS_X; x++)
		printf ("%4d", dynColumns[x].rgbAvgChange);
	printf("\n CCH:");
	for (x = 0; x < SLOTS_X; x++)
		printf ("%4d", dynColumns[x].rgbConChange);
	printf ("\n dynLeft = %04d, dynRight = %4d", dynLeft, dynRight);
	printf ("\n Black level = %4d\n", dynBlackLevel);
	printf("\n");
}





/*
 * Transform raw RGB values to scaled image
 */
void ambiLightSlots2Dyn (void)
{
	int i, j, k;
	int x, y;
	short blackLevl = INT16_MAX;


	/*---------------------------------- update the dynCols and dynRows							*/

	if (dynFrames < dynFramesLimit)
		dynFrames += 1;

	// compute row values for top and bottom borders

	for (y = 0; y < SLOTS_Y; y++)
	{
		long minRGB, maxRGB, sumRGB;

		minRGB = 0xffff;
		maxRGB = 0;
		sumRGB = 0;

		for (x = 0; x < SLOTS_X; x++)
		{
			int s = rgbSlots[y][x].R + rgbSlots[y][x].G + rgbSlots[y][x].B;
			if ( s < minRGB)
				minRGB = s;
			if (s > maxRGB)
				maxRGB = s;
			sumRGB += s;
		}
		dynRows[y].intContrast  += (maxRGB - minRGB);
		dynRows[y].intAvg += (sumRGB / SLOTS_X);

		if (dynFrames >= dynFramesLimit)
		{
			// compute moving average for min/max/avg
			dynRows[y].rgbAvg = dynRows[y].intAvg / dynFramesLimit;
			dynRows[y].intAvg -= dynRows[y].rgbAvg;
			dynRows[y].rgbContrast = dynRows[y].intContrast / dynFramesLimit;
			dynRows[y].intContrast -= dynRows[y].rgbContrast;
		}

		if (dynRows[y].rgbAvg < blackLevl)
			blackLevl = dynRows[y].rgbAvg;

		if (y > 0)
		{
			dynRows[y].rgbAvgChange = dynRows[y].rgbAvg - dynRows[y-1].rgbAvg;
			dynRows[y].rgbConChange = dynRows[y].rgbContrast - dynRows[y-1].rgbContrast;
		}
	}

	dynBlackLevelInt += blackLevl;
	if (dynFrames >= dynFramesLimit)
	{
		dynBlackLevel = dynBlackLevelInt / dynFramesLimit;
		dynBlackLevelInt -= dynBlackLevel;
	}

	k = 0;
	j = dynTop;
	for (y = 0; y < DYN_WIN_Y; y++)
	{
		if (dynRows[y].rgbAvg >= dynBlackLevel + BLACK_LEVEL_SHIFT)
		{
			j = y;
			break;
		}
		i = abs(dynRows[y].rgbAvgChange);
		if (i > k)
		{
			k = i;
			j = y;
		}
	}
	dynTop = j;

	k = 0;
	j = dynBottom;
	for (y = SLOTS_Y-1; y > SLOTS_Y-DYN_WIN_Y; y--)
	{
		if (dynRows[y].rgbAvg >= dynBlackLevel+BLACK_LEVEL_SHIFT)
		{
			j = y;
			break;
		}
		i = abs(dynRows[y].rgbAvgChange);
		if (i > k)
		{
			k = i;
			j = y-1;
		}
	}
	dynBottom = j;

	// now compute the rows values for the left and right borders

	for (x = 0; x < SLOTS_X; x++)
	{
		long minRGB, maxRGB, sumRGB;

		minRGB = 0xffff;
		maxRGB = 0;
		sumRGB = 0;

		for (y = 0; y < SLOTS_Y; y++)
		{
			int s = rgbSlots[y][x].R + rgbSlots[y][x].G + rgbSlots[y][x].B;
			if ( s < minRGB)
				minRGB = s;
			if (s > maxRGB)
				maxRGB = s;
			sumRGB += s;
		}

		dynColumns[x].intContrast  += (maxRGB - minRGB);
		dynColumns[x].intAvg += (sumRGB / SLOTS_X);

		if (dynFrames >= dynFramesLimit)
		{
			// compute moving average for min/max/avg
			dynColumns[x].rgbAvg = dynColumns[x].intAvg / dynFramesLimit;
			dynColumns[x].intAvg -= dynColumns[x].rgbAvg;
			dynColumns[x].rgbContrast = dynColumns[x].intContrast / dynFramesLimit;
			dynColumns[x].intContrast -= dynColumns[x].rgbContrast;
		}

		if (x > 0)
		{
			dynColumns[x].rgbAvgChange = dynColumns[x].rgbAvg - dynColumns[x-1].rgbAvg;
			dynColumns[x].rgbConChange = dynColumns[x].rgbContrast - dynColumns[x-1].rgbContrast;
		}
	}

	k = 0;
	j = dynLeft;
	for (x = 0; x < DYN_WIN_X; x++)
	{
		if (dynColumns[x].rgbAvg >= dynBlackLevel+BLACK_LEVEL_SHIFT)
		{
			j = x;
			break;
		}
		i = abs(dynColumns[x].rgbAvgChange);
		if (i > k)
		{
			k = i;
			j = x;
		}
	}
	dynLeft = j;

	k = 0;
	j = dynRight;
	for (x = SLOTS_X-1; x > SLOTS_X-DYN_WIN_X; x--)
	{
		if (dynColumns[x].rgbAvg >= dynBlackLevel + BLACK_LEVEL_SHIFT)
		{
			j = x;
			break;
		}
		i = abs(dynColumns[x].rgbAvgChange);
		if (i > k)
		{
			k = i;
			j = x-1;
		}
	}
	dynRight = j;

}




void ambiLightDyn2Image (void)
{
	int i;
	int j;
	long  rVal, gVal, bVal;
	short curImage;
	short cntVal;
	short dv;

	/*
	 * right edge
	 */
	curImage = 0;			// = bottom right
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	int d = (2<<frameWidth) - 1;

	for (i = SLOTS_Y-1; i >= 0; i--)
	{
		if (i <= dynBottom && i >= dynTop)	// else do not change R/G/B vals because blocks should be black (avoids black flickering of LEDs)
		{
			for (j = 0; j < frameWidth; j++)
			{
				long n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[i][dynRight-j].R;
				rVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[i][dynRight-j].G;
				gVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[i][dynRight-j].B;
				bVal += (n / d);
			}
		}
		cntVal++;

		dv += rgbImageHigh;
		if (dv >= SLOTS_Y)
		{
			while (dv >= SLOTS_Y)
			{
				dv -= SLOTS_Y;
				computeI (&rgbImage [curImage], rVal / 100 / cntVal, gVal / 100 / cntVal, bVal / 100 / cntVal);
				curImage++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}

	// left edge
	curImage = rgbImageHigh + rgbImageWid;			// top left starts at <right edge leds> + <top edge leds>
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	for (i = 0; i < SLOTS_Y; i++)
	{
		if (i <= dynBottom && i >= dynTop)	// else do not change R/G/B vals because blocks should be black (avoids black flickering of LEDs)
		{
			for (j = 0; j < frameWidth; j++)
			{
				long n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[i][dynLeft+j].R;
				rVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[i][dynLeft+j].G;
				gVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[i][dynLeft+j].B;
				bVal += (n / d);
			}
		}

		cntVal++;

		dv += rgbImageHigh;
		if (dv >= SLOTS_Y)
		{
			while (dv >= SLOTS_Y)
			{
				dv -= SLOTS_Y;
				computeI (&rgbImage [curImage], rVal / 100 / cntVal, gVal / 100 / cntVal, bVal / 100 / cntVal);
				curImage++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}

	// top edge
	curImage = rgbImageHigh;			// top right starts at <right edge leds>
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	for (i = SLOTS_X-1; i >= 0; i--)
	{
		if (i <= dynRight && i >= dynLeft)	// else do not change R/G/B vals because blocks should be black (avoids black flickering of LEDs)
		{
			for (j = 0; j < frameWidth; j++)
			{
				long n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[dynTop+j][i].R;
				rVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[dynTop+j][i].G;
				gVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[dynTop+j][i].B;
				bVal += (n / d);
			}
		}
		cntVal++;

		dv += rgbImageWid;
		if (dv >= SLOTS_X)
		{
			while (dv >= SLOTS_X)
			{
				dv -= SLOTS_X;
				computeI (&rgbImage [curImage], rVal / 100 / cntVal, gVal / 100 / cntVal, bVal / 100 / cntVal);
				curImage++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}

	// bottom edge
	curImage = 2*rgbImageHigh + rgbImageWid;			// bottom left starts at <2*Y leds> + <top leds>
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	for (i = 0; i < SLOTS_X; i++)
	{
		if (i <= dynRight && i >= dynLeft)	// else do not change R/G/B vals because blocks should be black (avoids black flickering of LEDs)
		{
			for (j = 0; j < frameWidth; j++)
			{
				long n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[dynBottom-j][i].R;
				rVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[dynBottom-j][i].G;
				gVal += (n / d);

				n = ((2<<(frameWidth-1-j)) * 100) * (long)rgbSlots[dynBottom-j][i].B;
				bVal += (n / d);
			}
		}
		cntVal++;

		dv += rgbImageWid;
		if (dv >= SLOTS_X)
		{
			while (dv >= SLOTS_X)
			{
				dv -= SLOTS_X;
				computeI (&rgbImage [curImage], rVal / 100 / cntVal, gVal / 100 / cntVal, bVal / 100 / cntVal);
				curImage++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}
}


/*
 * Scale the virtual X*Y block image to the physical LED frame an push it into the delay FIFO
 */
void ambiLightImage2LedRGB (void)
{
	int i;
	long  rVal, gVal, bVal;
	short ledIdx;
	short cntVal;
	short dv;

	masterBrightness = 100;		// always 100% in ambi mode. Brightness can be set in TVP5150

	// distribute the scaled virtual image to the led buffer

	ledIdx = 0;			// = bottom right
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	for (i = 0; i < rgbImageHigh; i++)	// expand the image blocks to the physical LED strip
	{
		rVal += rgbImage[i].R;
		gVal += rgbImage[i].G;
		bVal += rgbImage[i].B;
		cntVal++;

		dv += ledsY;
		if (dv >= rgbImageHigh)
		{
			while (dv >= rgbImageHigh)
			{
				dv -= rgbImageHigh;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].R = rVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].G = gVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].B = bVal / cntVal;
				ledIdx++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}

	ledIdx = ledsY;			// = top right
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	for (i = rgbImageHigh; i < rgbImageWid + rgbImageHigh; i++)
	{
		rVal += rgbImage[i].R;
		gVal += rgbImage[i].G;
		bVal += rgbImage[i].B;
		cntVal++;

		dv += ledsX;
		if (dv >= rgbImageWid)
		{
			while (dv >= rgbImageWid)
			{
				dv -= rgbImageWid;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].R = rVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].G = gVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].B = bVal / cntVal;
				ledIdx++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}

	ledIdx = ledsY + ledsX;			// = top left
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	for (i = rgbImageWid + rgbImageHigh; i < rgbImageHigh + rgbImageWid + rgbImageHigh; i++)
	{
		rVal += rgbImage[i].R;
		gVal += rgbImage[i].G;
		bVal += rgbImage[i].B;
		cntVal++;

		dv += ledsY;
		if (dv >= rgbImageHigh)
		{
			while (dv >= rgbImageHigh)
			{
				dv -= rgbImageHigh;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].R = rVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].G = gVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].B = bVal / cntVal;
				ledIdx++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}

	ledIdx = ledsY + ledsX + ledsY;			// = bottom left
	cntVal = 0;
	rVal = gVal = bVal = 0;
	dv = 0;

	for (i = rgbImageHigh + rgbImageWid + rgbImageHigh; i < rgbImageHigh + rgbImageWid + rgbImageHigh + rgbImageWid; i++)
	{
		rVal += rgbImage[i].R;
		gVal += rgbImage[i].G;
		bVal += rgbImage[i].B;
		cntVal++;

		dv += ledsX;
		if (dv >= rgbImageWid)
		{
			while (dv >= rgbImageWid)
			{
				dv -= rgbImageWid;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].R = rVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].G = gVal / cntVal;
				tvprocRGBDelayFifo[tvprocDelayW][ledIdx].B = bVal / cntVal;
				ledIdx++;
			}
			rVal = gVal = bVal = 0;
			cntVal = 0;
		}
	}

	i = tvprocDelayW - tvprocDelayTime;	// calculate read index within delay line
	if (i < 0)
		i += DELAY_LINE_SIZE;

	for (ledIdx = 0; ledIdx < ledsPhysical; ledIdx++)		// copy delay entry to physical LED
	{
		ws2812ledRGB[ledIdx].R = tvprocRGBDelayFifo[i][ledIdx].R;
		ws2812ledRGB[ledIdx].G = tvprocRGBDelayFifo[i][ledIdx].G;
		ws2812ledRGB[ledIdx].B = tvprocRGBDelayFifo[i][ledIdx].B;
	}

	if (++tvprocDelayW >= DELAY_LINE_SIZE)		// prepare for next frame
		tvprocDelayW = 0;
}




void computeI (rgbIcontroller_t *pid, uint8_t r, uint8_t g, uint8_t b)
{
	long err;
	short i;

	err = (r - pid->R) * factorI;
	pid->Ri += err;
	i = pid->Ri / MAX_ICONTROL;
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	pid->R = i;

	err = (g - pid->G) * factorI;
	pid->Gi += err;
	i = pid->Gi / MAX_ICONTROL;
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	pid->G = i;

	err = (b - pid->B) * factorI;
	pid->Bi += err;
	i = pid->Bi / MAX_ICONTROL;
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	pid->B = i;
}

