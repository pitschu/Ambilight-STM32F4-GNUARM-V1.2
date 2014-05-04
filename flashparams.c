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

static uint32_t		lastCRC = 0;
static uint32_t		flashCRC = 0;
static uint8_t		*blockAddr = (uint8_t*)PARAM_FLASH_START;
static uint16_t		blockSize;			// # of bytes in parameter flash block (without CRC bytes)

#define	FLASH_SIGNATURE			((long)('P'<<24) |	(long)('.'<<16) | (long)('S'<<8) | (long)('.'<<0) )
#define FLASH_VERSION			134


const flashParam_t flashParams[] = {
		{(uint8_t*)&rgbImageWid			, sizeof (rgbImageWid)},
		{(uint8_t*)&rgbImageHigh		, sizeof (rgbImageHigh)},
		{(uint8_t*)&factorI				, sizeof (factorI)},
		{(uint8_t*)&frameWidth			, sizeof (frameWidth)},
		{(uint8_t*)&tvprocDelayTime		, sizeof (tvprocDelayTime)},
		{(uint8_t*)&Hue_control			, sizeof (Hue_control)},
		{(uint8_t*)&Brightness			, sizeof (Brightness)},
		{(uint8_t*)&Color_saturation	, sizeof (Color_saturation)},
		{(uint8_t*)&Contrast			, sizeof (Contrast)},
		{(uint8_t*)&cropLeft			, sizeof (cropLeft)},
		{(uint8_t*)&captureWidth		, sizeof (captureWidth)},
		{(uint8_t*)&cropTop				, sizeof (cropTop)},
		{(uint8_t*)&cropHeight			, sizeof (cropHeight)},
		{(uint8_t*)&ledsX				, sizeof (ledsX)},
		{(uint8_t*)&ledsY				, sizeof (ledsY)},
		{(uint8_t*)&moodLightMasterBrightness	, sizeof (moodLightMasterBrightness)},
		{(uint8_t*)&moodLightTargetFixedColor	, sizeof (moodLightTargetFixedColor)},
		{(uint8_t*)&moodLightFade7colors[0]	, sizeof (moodLightFade7colors)},
		{(uint8_t*)&moodLightDIYcolor[0], sizeof (moodLightDIYcolor)},
		{(uint8_t*)&moodLightSinusDIY[0], sizeof (moodLightSinusDIY)},

// Add what ever parameter you want to be saved to flash
		{(uint8_t*)0, 0},
};



int calcParameterBlockSize ()
/*
 * add all sizeof() values in the flashParams array; add 4 bytes of CRC and 1 "valid byte"
 */
{
	uint8_t *s;
	int i, j;

	i = 0;
	j = 0;
	while ((s = flashParams[i].paraP) != (uint8_t*)0)
	{
		j += flashParams[i].paraSize;
		i++;
	}

	j += sizeof(flashCRC) + 1;			// add 4 for CRC and 1 for "valid indicator" byte

	return (j);
}




uint8_t *findActualParameterFlashBlock ()
/*
 * <blockAddr> is set to the start addr (= "valid byte" of actual parameter block;
 * The return value points to the flash byte just behind the block
 */
{
	uint8_t *s;
	uint32_t 	actualCRC;
	uint8_t *d;
	int i, j;


	blockSize = calcParameterBlockSize();
// walk thru the stored blocks and check the "valid byte" and their CRC.
// If it matches with the stored CRC, we found the actual block.

	i = 0;
	s = (uint8_t*)PARAM_FLASH_START;

	while (s < (uint8_t*)(PARAM_FLASH_END - blockSize))
	{
		blockAddr = s;
		if (blockAddr [0] != 0xFF)		// "valid byte" was flashed -> block invalid/old
		{
			s += blockSize;				// goto next block
		}
		else		// candidate found; calc it´s CRC
		{
			CRC_ResetDR();
			s++;				// parameter area start´s after the "valid byte"
			i = 0;
			while ((flashParams[i].paraP) != (uint8_t*)0)
			{
				for (j = 0; j < flashParams[i].paraSize; j++)
				{
					CRC_CalcCRC((uint32_t)*s);		// calc CRC from flash bytes
					s++;
				}
				i++;
			}

			actualCRC = CRC_GetCRC();

			d = (uint8_t*)&flashCRC;

			for (j = 0; j < sizeof (flashCRC); j++)		// read CRC stored in flash
				*d++ = *s++;

			if (actualCRC == flashCRC)		// flash content seems to be consistent -> found it
			{
				break;
			}
		}
	}

	if (s >= (uint8_t*)(PARAM_FLASH_END - blockSize))		// no valid block found -> flash full; should be erased
	{
		printf("\nfindActualParameterFlashBlock: no valid block found; searched up to %08X", (unsigned int)s);
		return ((uint8_t*)0);
	}

	printf("\nfindActualParameterFlashBlock: found block at %08X, next at %08X", (unsigned int)blockAddr, (unsigned int)s);
	return (s);				// points to byte after found block

}




int initFlashParamBlock (void)
/*
 * check the signature and version bytes at end of flash. If they do not match then erase flash and store RAM values
 */
{
	uint32_t *s = (uint32_t*)FLASH_SIGNATURE_P;
	uint32_t *v = (uint32_t*)FLASH_VERSION_P;

	if (*s != FLASH_SIGNATURE || *v != FLASH_VERSION)
	{
		printf ("\ninitFlashParamBlock: wrong signature/ version -> call updateAllParamsToFlash with forced erase\n");
		updateAllParamsToFlash (1);		// force erase and save ram vars to flash
	}
	else
	{
		if (readAllParamsFromFlash() != 0)	// get saved values from flash; if error init flash
		{
			printf ("\ninitFlashParamBlock: no flash values found; erasing flash and write RAM vars\n");
			updateAllParamsToFlash (1);		// force erase and save ram vars to flash
		}
	}

	return (0);
}




int checkForParamChanges (void)
/*
 *  Check for changed RAM variables by comparing last CRC with actual CRC
 */
{
	uint32_t actualCRC;
	uint8_t *s;
	int i, j;

	CRC_ResetDR();
	i = 0;
	while ((s = flashParams[i].paraP) != (uint8_t*)0)
	{
		for (j = 0; j < flashParams[i].paraSize; j++)
		{
			CRC_CalcCRC((uint32_t)*s);
			s++;
		}
		i++;
	}
	actualCRC = CRC_GetCRC();

	if (lastCRC != actualCRC)
	{
		lastCRC = actualCRC;
		return (1);
	}

	return (0);
}




int readAllParamsFromFlash (void)
/*
 * search actual block in flash and load all params to RAM varaibles
 */
{
	uint8_t *s;
	uint32_t 	actualCRC;
	uint8_t *d;
	int i, j;

	s = findActualParameterFlashBlock();
	if (s == (uint8_t*)0)					// no valid block found -> cannot load from flash
	{
		printf("\nreadAllParamsFromFlash: No block found -> return err\n");
		return (-1);
	}

	printf("\nreadAllParamsFromFlash at %08X\n", (unsigned int)blockAddr);
	CRC_ResetDR();
	i = 0;
	s = blockAddr + 1;			// first byte is "valid byte"; skip it

	while ((flashParams[i].paraP) != (uint8_t*)0)
	{
		for (j = 0; j < flashParams[i].paraSize; j++)
		{
			CRC_CalcCRC((uint32_t)*s);		// calc CRC from flash bytes
			s++;
		}
		i++;
	}

	actualCRC = CRC_GetCRC();
	d = (uint8_t*)&flashCRC;

	for (j = 0; j < sizeof (flashCRC); j++)		// read CRC stored in flash
		*d++ = *s++;

	if (actualCRC == flashCRC)		// flash content seems to be consistent -> load it
	{
		i = 0;
		s = (uint8_t*)(blockAddr + 1);
		while (( d = flashParams[i].paraP) != (uint8_t*)0)
		{
			for (j = 0; j < flashParams[i].paraSize; j++)
				*d++ = *s++;
			i++;
		}
	}
	return (0);
}




int updateAllParamsToFlash (int forceErase)
/*
 * Writes all RAM variables into a new flash block. If no free block is available or the <forceErase> is != 0
 * then erase the flash sector first and save RAM variables in a new block.
 */
{
	uint32_t *sig = (uint32_t*)FLASH_SIGNATURE_P;
	uint32_t *ver = (uint32_t*)FLASH_VERSION_P;
	uint32_t newCRC;
	uint8_t *d;
	uint8_t *s;
	int i, j;

	printf("\nupdateAllParamsToFlash with forceErase=%d\n", forceErase);

	if (forceErase || (s = findActualParameterFlashBlock()) == (uint8_t*)0)		// no valid block found or force erase
	{
		STM_EVAL_LEDOn(LED_BLU);
		STM_EVAL_LEDOn(LED_GRN);
		STM_EVAL_LEDOn(LED_RED);
		STM_EVAL_LEDOn(LED_ORN);
		printf ("\nErase flash blocks 10+11 (128k at 0x080C0000)\n");
		FLASH_EraseSector(FLASH_Sector_10, VoltageRange_3);
		FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);
		STM_EVAL_LEDOff(LED_BLU);
		STM_EVAL_LEDOff(LED_GRN);
		STM_EVAL_LEDOff(LED_RED);
		STM_EVAL_LEDOff(LED_ORN);
		FLASH_ProgramWord((uint32_t)sig, (uint32_t)FLASH_SIGNATURE);
		FLASH_ProgramWord((uint32_t)ver, (uint32_t)FLASH_VERSION);

		s = (uint8_t*)0;
	}

	checkForParamChanges ();		// calculate CRC
	newCRC = lastCRC;

	if (s != (uint8_t*)0)			// if valid block found then invalidate that block
	{
		FLASH_ProgramByte((uint32_t)blockAddr, '#');	// set "invalid byte" of old block
		blockAddr = s;									// save new block just behind old one
	}
	else
		blockAddr = (uint8_t*)PARAM_FLASH_START;	// save at start of flash sector

	printf("\nupdateAllParamsToFlash to %08X\n", (unsigned int)blockAddr);
	i = 0;
	d = (uint8_t*)(blockAddr + 1);					// skip "invalid byte"
	while ((s = flashParams[i].paraP) != (uint8_t*)0)
	{
		for (j = 0; j < flashParams[i].paraSize; j++)
		{
			FLASH_ProgramByte((uint32_t)d, *s);
			s++;
			d++;
		}
		i++;
	}

	// write CRC to flash
	i++;		// index of flashCRC
	s = (uint8_t*)&newCRC;
	for (j = 0; j < sizeof(newCRC); j++)
	{
		FLASH_ProgramByte((uint32_t)d, *s);
		s++;
		d++;
	}

	return (0);
}



