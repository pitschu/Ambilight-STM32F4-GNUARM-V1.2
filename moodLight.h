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
 */


#ifndef AMBILIGHT_STM32F4_GNUARM_V1_2_OLD_MOODLIGHT_H_
#define AMBILIGHT_STM32F4_GNUARM_V1_2_OLD_MOODLIGHT_H_

typedef enum {
	MLM_SINGLE_COLOR		= 0,
	MLM_FADE_7,
	MLM_FADE_DIY,
	MLM_SINUS,					// 1 sin() function for each color (R, G and B) with different frequencies
} moodLightMode_e;

typedef struct {
	unsigned char R, G, B;
} moodLightColor_t;

typedef struct {
	moodLightColor_t  fc;			// from color
	moodLightColor_t  tc;			// to color
	short			  duration;			// number of sys ticks to reach tc
	uint32_t		  time;		// sys tick counter
} moodLightFader_t;


typedef struct {
	moodLightColor_t 	color;
	unsigned short 		irCode;		// code from NEC IR-remote
} moodLightFixedColor_t;


typedef struct {
	unsigned char periodB, periodG, periodR;
	unsigned char baseB, baseG, baseR;
	unsigned char amplitudeB, amplitudeG, amplitudeR;
} moodLightSinusSettings_t;



extern volatile int			moodLightTargetTimer;
extern int					moodLightMasterBrightness;
extern volatile int			moodLightFaderTime;
extern moodLightMode_e 		moodLightMode;
extern int					moodLightTargetFixedColor;
extern int					moodLightMasterBrightness;		// = 70%
extern volatile int			moodLightFaderTime;			// global fading timer = 2 sec

extern moodLightColor_t 	moodLightFade7colors[7];
extern moodLightColor_t 	moodLightDIYcolor[6];
extern moodLightSinusSettings_t moodLightSinusDIY [6];


extern void moodLightMainAction (int action);

extern int moodLightHandleIRcode ();


#endif /* AMBILIGHT_STM32F4_GNUARM_V1_2_OLD_MOODLIGHT_H_ */
