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



#ifndef AMBILIGHT_STM32F4_GNUARM_V1_2_OLD_FLASHPARAMS_H_
#define AMBILIGHT_STM32F4_GNUARM_V1_2_OLD_FLASHPARAMS_H_

#define PARAM_FLASH_START		0x080C0000		// 256K before ROM end (last 2 sectors)
#define PARAM_FLASH_END			0x080FFFF0		// last flash byte usable for parameters
#define	FLASH_SIGNATURE_P		0x080FFFFC		// 4 bytes (should be 'P.S.')
#define FLASH_VERSION_P			0x080FFFF8		// 4 bytes version ID 0x0001 ...

typedef struct {
	uint8_t *paraP;
	short	 paraSize;
} flashParam_t;


extern int checkForParamChanges (void);
extern int initFlashParamBlock (void);
extern int readAllParamsFromFlash (void);
extern int updateAllParamsToFlash (int forceErase);

#endif /* AMBILIGHT_STM32F4_GNUARM_V1_2_OLD_FLASHPARAMS_H_ */
