/*****************************************************
 *
 *	Control program for the PitSchuLight TV-Backlight
 *	(c) Peter Schulten, M�lheim, Germany
 *	peter_(at)_pitschu.de
 *
 *	Die unver�nderte Wiedergabe und Verteilung dieses gesamten Sourcecodes
 *	in beliebiger Form ist gestattet, sofern obiger Hinweis erhalten bleibt.
 *
 * 	Ich stelle diesen Sourcecode kostenlos zur Verf�gung und biete daher weder
 *	Support an noch garantiere ich f�r seine Funktionsf�higkeit. Au�erdem
 *	�bernehme ich keine Haftung f�r die Folgen seiner Nutzung.

 *	Der Sourcecode darf nur zu privaten Zwecken verwendet und modifiziert werden.
 *	Dar�ber hinaus gehende Verwendung bedarf meiner Zustimmung.
 *
 *	History
 *	09.06.2013	pitschu		Start of work
 */

#include "stm32f4xx.h"
#include <stdio.h>
#include "string.h"
#include "main.h"
#include "ambiLight.h"



typedef enum {
	MS_NONE		= 0,
	MS_FARBTON,
	MS_CONTRAST,
	MS_BRIGHTNESS,
	MS_SATURATION,
	MS_TOP,
	MS_LEFT,
	MS_RIGHT,
	MS_HEIGHT,
	MS_ICONTROL,
	MS_IMAGE_WID,
	MS_IMAGE_HIG,
	MS_VIDEO_SRC,
	MS_FRAME_WID,
	MS_FRAME_DELAY
} mainStates_e;


mainStates_e mainState = MS_NONE;


int UserInterface (void)
{
	int16_t c;

#ifdef USE_USB
	if(UB_USB_CDC_GetStatus()==USB_CDC_CONNECTED)
		c = -1;
	else
		c = UB_VCP_CharRx ();
#else
	c = get_cHost();
#endif

	if (c > 0)				// got char from user
	{
		switch (c)
		{
		case 'Q':
		case 'q':
			ambiLightPrintDynInfos();
			break;
		case 'a':
		case 'A':
			mainState = MS_FRAME_DELAY;
			printf("\nCurrent frame delay is %d frames\n", (int)tvprocDelayTime);
			break;
		case 'e':
		case 'E':
			mainState = MS_FRAME_WID;
			printf("\nCurrent frame width (slots) is %d\n", frameWidth);
			break;
		case 'f':
		case 'F':
			mainState = MS_FARBTON;
			printf("\nCurrent Hue control is %d\n", Hue_control);
			break;
		case 's':
		case 'S':
			mainState = MS_SATURATION;
			printf("\nCurrent Color_saturation is %d\n", Color_saturation);
			break;
		case 'b':
		case 'B':
			mainState = MS_BRIGHTNESS;
			printf("\nCurrent Brightness is %d\n", Brightness);
			break;
		case 'c':
		case 'C':
			mainState = MS_CONTRAST;
			printf("\nContrast Contrast is %d\n", Contrast);
			break;

		case 'l':
		case 'L':
			mainState = MS_LEFT;
			printf("\nCrop left is %d\n", (int)cropLeft/2);
			printf("Capture right is %d\n", (int)(cropLeft/2 + captureWidth));
			break;
		case 'w':
		case 'W':
			mainState = MS_RIGHT;
			printf("\nCrop left is %d\n", (int)cropLeft/2);
			printf("Capture right is %d\n", (int)(cropLeft/2 + captureWidth));
			break;
		case 't':
		case 'T':
			mainState = MS_TOP;
			printf("\nCrop top is %d\n", (int)cropTop);
			printf("Crop bottom is %d\n", (int)(cropTop+cropHeight-1));
			break;
		case 'h':
		case 'H':
			mainState = MS_HEIGHT;
			printf("\nCrop top is %d\n", (int)cropTop);
			printf("Crop bottom is %d\n", (int)(cropTop+cropHeight-1));
			break;

		case 'i':
		case 'I':
			mainState = MS_ICONTROL;
			printf("\nI-Control is %d\n", (int)factorI);
			break;

		case 'x':
		case 'X':
			mainState = MS_IMAGE_WID;
			printf("\nImage width in blocks is %d\n", (int)rgbImageWid);
			break;

		case 'y':
		case 'Y':
			mainState = MS_IMAGE_HIG;
			printf("\nImage height in blocks is %d\n", (int)rgbImageHigh);
			break;

		case 'v':
		case 'V':
			mainState = MS_VIDEO_SRC;
			printf("\nCurrent video source is %d, mode = %d\n", (int)videoCurrentSource, (int)videoSourceSelect);
			break;

		case '+':
		case '-':
		case 'd':
			switch (mainState)
			{
			case MS_FRAME_DELAY:
				if (c=='+' && tvprocDelayTime < DELAY_LINE_SIZE-1) tvprocDelayTime += 1;
				if (c=='-' && tvprocDelayTime > 0) tvprocDelayTime -= 1;
				if (c=='d')	tvprocDelayTime = 0;
				printf("Frame delay is %d frames\n", tvprocDelayTime);
				break;
			case MS_FRAME_WID:
				if (c=='+' && frameWidth < 11) frameWidth += 1;
				if (c=='-' && frameWidth > 1) frameWidth -= 1;
				if (c=='d')	frameWidth = 4;
				printf("Frame width (slots) is %d\n", frameWidth);
				break;
			case MS_FARBTON:
				if (c=='+' && Hue_control < 125) Hue_control += 1;
				if (c=='-' && Hue_control > -127) Hue_control -= 1;
				if (c=='d')	Hue_control = 0;
				printf("Hue control is %d\n", Hue_control);
				break;
			case MS_BRIGHTNESS:
				if (c=='+' && Brightness < 255) Brightness += 1;
				if (c=='-' && Brightness > 1) Brightness -= 1;
				if (c=='d')	Brightness = 60;
				printf("Brightness is %d\n", Brightness);
				break;
			case MS_SATURATION:
				if (c=='+' && Color_saturation < 255) Color_saturation += 1;
				if (c=='-' && Color_saturation > 1) Color_saturation -= 1;
				if (c=='d')	Color_saturation = 128;
				printf("Color_saturation is %d\n", Color_saturation);
				break;
			case MS_CONTRAST:
				if (c=='+' && Contrast < 197) Contrast += 1;
				if (c=='-' && Contrast > 1) Contrast -= 1;
				if (c=='d')	Contrast = 80;
				printf("Contrast is %d\n", Contrast);
				break;

			case MS_ICONTROL:
				if (c=='+' && factorI < MAX_ICONTROL) factorI += 1;
				if (c=='-' && factorI > 1) factorI -= 1;
				if (c=='d')	factorI = 20;
				printf("I-Control is %d\n", factorI);
				break;

			case MS_IMAGE_WID:
				if (c=='+' && rgbImageWid < SLOTS_X) rgbImageWid += 1;
				if (c=='-' && rgbImageWid > 1) rgbImageWid -= 1;
				if (c=='d')	rgbImageWid = SLOTS_X;
				printf("Image width in blocks is %d\n", rgbImageWid);
				ambiLightClearImage();
				break;

			case MS_IMAGE_HIG:
				if (c=='+' && rgbImageHigh < SLOTS_Y) rgbImageHigh += 1;
				if (c=='-' && rgbImageHigh > 1) rgbImageHigh -= 1;
				if (c=='d')	rgbImageHigh = SLOTS_Y;
				ambiLightClearImage();
				printf("Image height in blocks is %d\n", rgbImageHigh);
				break;

			case MS_LEFT:
				if (c=='+' && cropLeft/2 < 200 && (cropLeft/2 + captureWidth) < 840)
				{
					cropLeft += 8;
					captureWidth -= 4;
				}
				if (c=='-' && cropLeft/2 > 40 && (cropLeft/2 + captureWidth) < 840)
				{
					cropLeft -= 8;
					captureWidth += 4;
				}
				if (c=='d')
				{
					cropLeft = 168;
					captureWidth = 668;
				}
				printf("\nCrop left is %d\n", (int)cropLeft/2);
				printf("Capture right is %d\n", (int)(cropLeft/2 + captureWidth));
				memset((void*)&rgbSlots[0][0], 0, sizeof (rgbSlots));
				break;
			case MS_RIGHT:
				if (c=='+' && captureWidth < 800 && (cropLeft/2 + captureWidth) < 840)
				{
					captureWidth += 4;
				}
				if (c=='-' && captureWidth > 200)
				{
					captureWidth -= 4;
				}
				if (c=='d')
				{
					captureWidth = 668;
				}
				printf("\nCrop left is %d\n", (int)cropLeft/2);
				printf("Capture right is %d\n", (int)(cropLeft/2 + captureWidth));
				memset((void*)&rgbSlots[0][0], 0, sizeof (rgbSlots));
				break;
			case MS_TOP:
				if (c=='+' && cropTop < 150 && (cropTop + cropHeight) < 306)
				{
					cropTop += 1;
					cropHeight -= 1;
				}
				if (c=='-' && cropTop > 4)
				{
					cropTop -= 1;
					cropHeight += 1;
				}
				if (c=='d')
				{
					cropTop = 16;
					cropHeight = 288;
				}
				printf("\nCrop top is %d\n", (int)cropTop);
				printf("Crop bottom is %d\n", (int)(cropTop+cropHeight-1));
				memset((void*)&rgbSlots[0][0], 0, sizeof (rgbSlots));
				break;
			case MS_HEIGHT:
				if (c=='+' && (cropTop + cropHeight) < 306)
				{
					cropHeight += 1;
				}
				if (c=='-' && cropHeight > 40)
				{
					cropHeight -= 1;
				}
				if (c=='d')	cropHeight = 288;
				printf("\nCrop top is %d\n", (int)cropTop);
				printf("Crop bottom is %d\n", (int)(cropTop+cropHeight-1));
				memset((void*)&rgbSlots[0][0], 0, sizeof (rgbSlots));
				break;

			default:
				break;
			}
			TVP5150setPictureParams();
			break;

			case '0':
			case '1':
			case '2':
				if (mainState == MS_VIDEO_SRC)
				{
					int s = c - '0';

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

					printf("\nCurrent video source is %d, mode = %d\n", (int)videoCurrentSource, (int)videoSourceSelect);
					delay_ms(1000);
					printf("    video signal state %d\n", (int)TVP5150hasVideoSignal());
					videoOffCount = 5;				// start search
				}
				break;

			default:
				printf("Usage: use +/- keys to set val; d=default\n");
				printf("     F=Hue, S=Saturation, B=Brightness, C=Contrast\n");
				printf("     L=Left, W=Width, T=Top, H=Height\n");
				printf("     I=I-factor of integrator (128 = MAX)\n");
				printf("     E=# of slots aggregated for LED strip (1..10)\n");
				printf("     X=virtual image width in blocks\n");
				printf("     Y=virtual image height in blocks\n");
				printf("     V=select video source (1 or 2)\n");
				printf("     Q=show info about Dyn Matrix\n");
				break;
		}

		switch (mainState)
		{
		case MS_FRAME_DELAY:
			displayOverlayPercents(((int)tvprocDelayTime*100)/(DELAY_LINE_SIZE-1), 300);
			break;
		case MS_FRAME_WID:
			displayOverlayPercents(((int)frameWidth*100)/11, 300);
			break;
		case MS_FARBTON:
			displayOverlayPercents((((int)Hue_control*100)/127) + 50, 300);
			break;
		case MS_BRIGHTNESS:
			displayOverlayPercents((((int)Brightness*100)/255), 300);
			break;
		case MS_SATURATION:
			printf("Color_saturation is %d\n", Color_saturation);
			displayOverlayPercents((((int)Color_saturation*100)/255), 300);
			break;
		case MS_CONTRAST:
			displayOverlayPercents((((int)Contrast*100)/198), 300);
			break;

		case MS_ICONTROL:
			displayOverlayPercents((((int)factorI*100)/MAX_ICONTROL), 300);
			break;

		case MS_IMAGE_WID:
			displayOverlayPercents((((int)rgbImageWid*100)/SLOTS_X), 300);
			break;

		case MS_IMAGE_HIG:
			displayOverlayPercents((((int)rgbImageHigh*100)/SLOTS_Y), 300);
			break;

		case MS_LEFT:
		case MS_RIGHT:
			displayOverlayPercents(((((int)(cropLeft/2)-40)*100)/840), 300);
			displayOverlayPercents((((int)(captureWidth)*100)/840), 0);
			break;
		case MS_TOP:
			displayOverlayPercents((((int)(cropTop)*100)/150), 300);
			break;
		case MS_HEIGHT:
			displayOverlayPercents((((int)(cropHeight)*100)/300), 300);
			break;

		default:
			break;
		}


	}		// if (c > 0)

	return (0);
}
