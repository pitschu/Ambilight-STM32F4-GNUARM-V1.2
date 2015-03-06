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


#ifndef AMBILIGHT_H_
#define AMBILIGHT_H_

#define DYN_WIN_Y			10			// max +/- in Y
#define DYN_WIN_X			10			// max +/- in X
#define BLACK_LEVEL_SHIFT	10


typedef struct {
	uint8_t		R;
	uint8_t		G;
	uint8_t		B;
	short		Ri, Gi, Bi;
} rgbIcontroller_t;

typedef struct {
	long			intContrast, intAvg;
	short			rgbContrast, rgbAvg;
	short			rgbConChange, rgbAvgChange;
} dynRGBsum_t;

extern void ambiLightInit (void);
extern void ambiLightClearImage (void);
extern void ambiLightSlots2Dyn (void);
extern void ambiLightPrintDynInfos (void);
extern void ambiLightDyn2Image (void);
extern void ambiLightImage2LedRGB (void);
extern int  ambiLightHandleIRcode ();

void computeI (rgbIcontroller_t *pid, uint8_t r, uint8_t g, uint8_t b);

extern short	rgbImageWid;
extern short	rgbImageHigh;
extern unsigned short		dynFramesLimit;	// pitschu v1.2: Added limit value

extern short 	factorI;			// 128 = 1.0
extern rgbIcontroller_t	rgbImage [];

extern rgbValue_t 	tvprocRGBDelayFifo[DELAY_LINE_SIZE][LEDS_MAXTOTAL];			// for TV picture proc delays; up to 1 second
extern short  	tvprocDelayW;				// delay FIFO write pointer
extern short  	tvprocDelayTime;			// difference between write an read pointr


#endif /* AMBILIGHT_H_ */
