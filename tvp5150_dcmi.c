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
 *	05.05.2014	pitschu	v1.1 supports AGC control
 *	24.07.2014	pitschu v1.2 fixed bugs in pixel to slots mapping (integer division problems)
*/

#include "hardware.h"
#include "AvrXSerialIo.h"
#include "ws2812.h"
#include "tvp5150_dcmi.h"

/*
 * Funktionsweise:
 * Der TVP5150 liefert mit 27Mhz die Pixeldaten im Format ITU656. Hierbei weerden die Chromadaten CB und CR mit
 * den Lumadaten Y für 2 Pixels in 4 bytes <Cb><y1><Cr><y2>. Die Daten werden per DCMI wie folgt verarbeitet:
 * Über die Crop Funktion des DCMI wird der gewünschte Pixelbereich eingestellt, die Crop Funktion des TVP5150
 * wird nicht genutzt, er liefert immer das volle Bild von 1728 bytes pro Zeile und 625 Zeilen pro Frame.
 *
 * Die Farb- und Helligkeitswerte der gelesenen Pixel werden zunächst in 64 x 40 Blöcken aufaddiert (YCbCrSlots).
 * Da diese Verarbeitung für eine Zeile länger als 64us dauert, wird eine Zeile in 2 Hälften gelesen, die Verarbeitung
 * dauert dann nur 40us für eine halbe Zeile.
 * Zunächst wird über die Crop Einstellung die linke Hälfte des Bildes zeilenweise gelesen. Wenn das Bild bestehend
 * aus beiden PAL-Frames fertig ist (VSync), wird umgeschaltet auf die rechte Hälfte. Während der VSync Austastlücke
 * werden die YCbCr Summenblöcke in RGB-Blöcke umgerechnet und die YCbCr Daten wieder auf 0 initialisiert.
 *
 * Das DMA verwendet double buffering, so dass während der Aufaddition der YCbCr Daten aus Zeile N bereits die
 * Daten der Zeile N+1 per DMA in den anderen Buffer geladen werden.
 *
 * Die gesamte Verarbeitungskette läuft wie folgt:
 *
 * 1. Daten werden per DMA geladen und nach HSync in <yCbCrSlots[48x28]> aufaddiert
 * 2. <yCbCr> Daten werden nach VSync in RGB <rgbSlots[48x28]> umgerechnet
 *
 * Bis hierher war alles während interupt Zeiten. Ab hier passiert es in Main()
 *
 */

#define		LINE_WIDTH		864		// full line width in pixels
#define		PAL_WIDTH		720		// visible part
#define		PAL_HEIGHT		288
#define		DMA_LINES		1

#undef 		HARD_SYNC
#define 	HARD_SYNC		// had no luck in using embedded codes; I use the hsync+vsync pins now


// some PAL timings: Front porch = 20 bytes, sync width = 128 bytes, back porch = 140 bytes
//		Vertical:	 front porch = 2.5 lines, sync width = 6 lines, back porch = 15 lines
// most values can be changed during runtime

unsigned long	captureWidth = 		696;				// in pixels; best on my old Topfield

unsigned long	cropLeft	= 		160;				// back porch bytes until visible area starts
unsigned long	cropTop		= 		16;
unsigned long	cropHeight	= 		274;

unsigned long	dmaWidth 	= 		PAL_WIDTH/4;		// in words (= DMA unit)
unsigned long	dmaBufLen	= 		((PAL_WIDTH/2) * DMA_LINES);

unsigned char	Brightness			= 60;		// image defaults; all values stored in flash param section
unsigned char	Color_saturation	= 100;
signed char		Hue_control			= 0;
unsigned char	Contrast			= 80;

volatile short	captureLeftRight = 	0;			// 0 = left; 1 = right; one frame for each side
volatile short 	captureReady = 		0;			// semaphore for main(); set when frame captured completly

unsigned char	videoSourceSelect = 0;			// 0 = auto; 1/2 = video channel
unsigned char	videoCurrentSource = 1;			// current video channel set in TVP5150

unsigned short	tvp5150AGC = 1;					// pitschu 140505: user selectable setting (usrinterface.c)


short 	TVP5150initRegisters(void);
void 	TVP5150initIOports(void);
void 	TVP5150initDCMI(void);
void 	TVP5150initDMA(void);

//------------------------------------------------------------------------------------------------------------

typedef struct {
	uint8_t		Cb;
	uint8_t		Y0;
	uint8_t		Cr;
	uint8_t		Y1;
} YCbCr_t;

static volatile YCbCr_t YCbCr_buf0 [((LINE_WIDTH/2) * DMA_LINES)+1] = {{0}};		// the two alternating DMA buffers
static volatile YCbCr_t YCbCr_buf1 [((LINE_WIDTH/2) * DMA_LINES)+1] = {{0}};


typedef struct {
	long		Y;
	long		Cb;
	long		Cr;
	long		cnt;			// number of pixels in this slot
} videoData_t;


static volatile videoData_t YCbCrSlots [(SLOTS_Y * SLOTS_X) + 1] = {{0}};		// ycbcr values of frame currently captured
static volatile videoData_t *arrP;				// working pointer used in HSYNC handler

static unsigned short  arrYslotCnt;			// counter within slice currently running

static int tvp5150_log_status(void);		// prints all TVP5150 registers


// rgbSlots[][] array is the main output of this module. It contains the raw RGB video data; it´s updated with each new frame
volatile rgbValue_t   rgbSlots [SLOTS_Y][SLOTS_X];					// RGB values are updated permanently


/***********************************************************************************/

short TVP5150init(void)
{
	TVP5150initIOports();
	TVP5150initDCMI();
	TVP5150initDMA();

	TVP5150initRegisters();

	delay_ms(100);
	tvp5150_log_status ();

	return(0);
}



// Start the capturing. Captureing continues until TVP5150stopCapture() is called

void TVP5150startCapture(void)
{
	DCMI_CROPInitTypeDef DCMI_Crop;
	int timeout = 1000;

	captureReady = 0;
	captureLeftRight = 0;		// start with left half
	arrP = &YCbCrSlots[0];

	DCMI_Crop.DCMI_CaptureCount 			= captureWidth - 1;  // !! we capture only half of <captureWidth> pixels !!
	DCMI_Crop.DCMI_HorizontalOffsetCount 	= cropLeft;		// get left half of picture
	DCMI_Crop.DCMI_VerticalLineCount 		= cropHeight - 1;
	DCMI_Crop.DCMI_VerticalStartLine 		= cropTop;
	DCMI_CROPConfig (&DCMI_Crop);
	DCMI_CROPCmd (ENABLE);

	dmaWidth = captureWidth / 4;
	dmaBufLen = dmaWidth;

	arrYslotCnt = 0;

	DMA_SetCurrDataCounter(DMA2_Stream1, dmaBufLen);
	DMA_MemoryTargetConfig(DMA2_Stream1, (uint32_t)&YCbCr_buf0[0], DMA_Memory_0);
	DMA_DoubleBufferModeConfig (DMA2_Stream1, (uint32_t)&YCbCr_buf1[0], DMA_Memory_0);
	DMA_DoubleBufferModeCmd (DMA2_Stream1, ENABLE);

	DMA_Cmd(DMA2_Stream1, ENABLE);
	while (timeout && DMA_GetCmdStatus(DMA2_Stream1) != ENABLE)
		timeout--;

	DCMI_Cmd(ENABLE);
	DCMI_CaptureCmd(ENABLE);
}






void TVP5150stopCapture(void)
{
	int timeout = 1000;

	DCMI_CaptureCmd(DISABLE);
	DCMI_Cmd(DISABLE);
	DMA_Cmd(DMA2_Stream1, DISABLE);
	while (timeout && DMA_GetCmdStatus(DMA2_Stream1) != DISABLE)
		timeout--;
}




void TVP5150setPictureParams (void)
{
	I2C_WriteByte(TVP5150_I2C_ADR, R09_Brightness_control, 			(uint8_t)Brightness);
	I2C_WriteByte(TVP5150_I2C_ADR, R0A_Color_saturation_control, 	(uint8_t)Color_saturation);
	I2C_WriteByte(TVP5150_I2C_ADR, R0B_Hue_control, 				(uint8_t)Hue_control);
	I2C_WriteByte(TVP5150_I2C_ADR, R0C_Contrast_Control, 			(uint8_t)Contrast);

	I2C_WriteByte(TVP5150_I2C_ADR, R01_Analog_channel_controls,		(tvp5150AGC ? 0x15 : 0x1e));		// pitschu 140505
}



void TVP5150selectVideoSource (unsigned char src)
{
	if (src == 1)
		I2C_WriteByte(TVP5150_I2C_ADR, R00_Video_input_source_selection_1, 		0x00);		// select input #1
	else
		I2C_WriteByte(TVP5150_I2C_ADR, R00_Video_input_source_selection_1, 		0x02);		// select input #2

	videoCurrentSource = src;
}


// returns 0 when no video is detectod on active channel;
// returns 1 when video signal is detected and locked on active channel
unsigned char TVP5150hasVideoSignal ()
{
	int s = tvp5150_read(R88_Status_register_1);

	if ((s & 0x0e) != 0x0e)			// check color, Vsync and Hsync lock bits
		return (0);

	return 1;
}



unsigned char TVP5150getStatus1 ()
{
	int s = tvp5150_read(R88_Status_register_1);

	return (s & 0xff);
}



short TVP5150initRegisters(void)
{
	TVP5150selectVideoSource(videoCurrentSource);

// done in TVP5150setPictureParams: 	I2C_WriteByte(TVP5150_I2C_ADR, R01_Analog_channel_controls,					0x15);		// AGC on
	I2C_WriteByte(TVP5150_I2C_ADR, R03_Miscellaneous_controls, 					0b10101111);// VBLK select and on, YCvCr mode, HSYNC,VSYNC,... enabled, Clock out enabled
	I2C_WriteByte(TVP5150_I2C_ADR, R0F_Configuration_shared_pins,				0b00000010);// VBLK instead of INTREQ

	TVP5150setPictureParams ();

//	I2C_WriteByte(TVP5150_I2C_ADR, R0D_Outputs_and_data_rates_select, 			0b00000111); // code range BT.601; 2s compl + offset;discrete sync output
	I2C_WriteByte(TVP5150_I2C_ADR, R0D_Outputs_and_data_rates_select, 			0b01000000); // extended code range (1..254); 2s compl + offset;discrete sync output

	I2C_WriteByte(TVP5150_I2C_ADR, R11_Active_video_cropping_start_pixel_MSB,	0x00);
	I2C_WriteByte(TVP5150_I2C_ADR, R12_Active_video_cropping_start_pixel_LSB,	0x00);
	I2C_WriteByte(TVP5150_I2C_ADR, R13_Active_video_cropping_stop_pixel_MSB,	0x00);
	I2C_WriteByte(TVP5150_I2C_ADR, R14_Active_video_cropping_stop_pixel_LSB,	0x00);

	I2C_WriteByte(TVP5150_I2C_ADR, R16_Horizontal_sync_start,					0x80);

	I2C_WriteByte(TVP5150_I2C_ADR, R28_Video_standard,							0x00);		// Video standard autoswitch  mode
	return(0);
}




void TVP5150initDCMI(void)
{
	DCMI_CROPInitTypeDef DCMI_Crop;
	DCMI_CodesInitTypeDef DCMI_Codes;
	NVIC_InitTypeDef        NVIC_InitStructure;
	DCMI_InitTypeDef DCMI_InitStruct;

	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_DCMI, ENABLE);

	DCMI_DeInit ();
	DCMI_InitStruct.DCMI_SynchroMode = 		DCMI_SynchroMode_Hardware;
	DCMI_InitStruct.DCMI_CaptureMode = 		DCMI_CaptureMode_SnapShot;	// will be reactivated in IRQ handler
	DCMI_InitStruct.DCMI_PCKPolarity = 		DCMI_PCKPolarity_Rising;
	DCMI_InitStruct.DCMI_VSPolarity = 		DCMI_VSPolarity_High;
	DCMI_InitStruct.DCMI_HSPolarity = 		DCMI_HSPolarity_High;
	DCMI_InitStruct.DCMI_CaptureRate = 		DCMI_CaptureRate_All_Frame;
	DCMI_InitStruct.DCMI_ExtendedDataMode = DCMI_ExtendedDataMode_8b;
	DCMI_Init(&DCMI_InitStruct);

	// see ITU BT.656 specs; currently not used in my implementation
	DCMI_Codes.DCMI_FrameStartCode = 		0xB6;
	DCMI_Codes.DCMI_LineStartCode = 		0x80;
	DCMI_Codes.DCMI_LineEndCode = 			0x9D;
	DCMI_Codes.DCMI_FrameEndCode = 			0xAB;
	DCMI_SetEmbeddedSynchroCodes (&DCMI_Codes);

	DCMI_Crop.DCMI_CaptureCount = 			captureWidth-1;
	DCMI_Crop.DCMI_HorizontalOffsetCount = 	cropLeft - 1;
	DCMI_Crop.DCMI_VerticalLineCount = 		cropHeight-1;
	DCMI_Crop.DCMI_VerticalStartLine = 		cropTop;
	DCMI_CROPConfig (&DCMI_Crop);
	DCMI_CROPCmd (ENABLE);

	/* Enable DCMI Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = 	DCMI_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	DCMI_ClearFlag(DCMI_FLAG_VSYNCRI | DCMI_FLAG_LINERI | DCMI_FLAG_FRAMERI | DCMI_FLAG_ERRRI | DCMI_FLAG_OVFRI);
	DCMI_ITConfig (DCMI_IT_LINE | DCMI_IT_VSYNC | DCMI_IT_FRAME | DCMI_IT_OVF, ENABLE);
}




void TVP5150initDMA(void)
{
	DMA_InitTypeDef  DMA_InitStructure;
	NVIC_InitTypeDef        NVIC_InitStructure;

	// Clock enable
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

	// DMA deinit
	DMA_Cmd(DMA2_Stream1, DISABLE);
	DMA_DeInit(DMA2_Stream1);

	// DMA init
	DMA_InitStructure.DMA_Channel 				= DMA_Channel_1;
	DMA_InitStructure.DMA_PeripheralBaseAddr 	= TVP5150_DCMI_REG_DR_ADDRESS;
	DMA_InitStructure.DMA_PeripheralInc 		= DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_PeripheralDataSize 	= DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_PeripheralBurst 		= DMA_PeripheralBurst_Single;
	DMA_InitStructure.DMA_DIR 					= DMA_DIR_PeripheralToMemory;

	DMA_InitStructure.DMA_Memory0BaseAddr 		= (uint32_t)&YCbCr_buf0[0];
	DMA_InitStructure.DMA_BufferSize 			= dmaBufLen;
	DMA_InitStructure.DMA_MemoryInc 			= DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_MemoryDataSize 		= DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_MemoryBurst 			= DMA_MemoryBurst_Single;

	DMA_InitStructure.DMA_Mode 					= DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority 				= DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode 				= DMA_FIFOMode_Enable;
	DMA_InitStructure.DMA_FIFOThreshold 		= DMA_FIFOThreshold_Full;

	DMA_Init(DMA2_Stream1, &DMA_InitStructure);

	// we use double buffering. While first buffer is fílled we can process second buffer
	DMA_DoubleBufferModeConfig (DMA2_Stream1, (uint32_t)&YCbCr_buf1[0], DMA_Memory_0);
	DMA_DoubleBufferModeCmd (DMA2_Stream1, ENABLE);

	/* Enable DMA interrupt: when line buffer is full, we are interupted */
	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	DMA_ClearITPendingBit(DMA2_Stream1, DMA_IT_TCIF1);
	DMA_ITConfig (DMA2_Stream1, DMA_IT_TC, ENABLE);		// enable transfer complete event
}


//-------------------------------------------------------------------------------------------------------------------


// IRQ handler called when line buffer is full (just before HSYNC)
void DMA2_Stream1_IRQHandler (void)
{
	if (DMA_GetITStatus(DMA2_Stream1, DMA_IT_TCIF1) == SET)
	{
		STM_EVAL_LEDOn(LED_RED);		// set check point for oszi
		DMA_ClearITPendingBit(DMA2_Stream1, DMA_IT_TCIF1);

		register videoData_t *p = (videoData_t*)arrP;
		register YCbCr_t	  *s = (YCbCr_t*)(DMA_GetCurrentMemoryTarget(DMA2_Stream1) == 0 ? &YCbCr_buf1[0] : &YCbCr_buf0[0]);
		register short a = 0;
		register short x;

		for (x = dmaWidth; x != 0; x--)				// add video data in blocks (YCbCrSlots)
		{
			p->Cb += (unsigned long)s->Cb - 128;		// Cb and Cr are 2s complement vlues
			p->Cr += (unsigned long)s->Cr - 128;
			p->Y  += (unsigned long)s->Y0;				// add both Y values
			p->Y  += (unsigned long)s->Y1;
			//			p->Y -= 32;									// Y has an offset of 16 (BTU.601)
			p->cnt++;
			s++;
			a += (SLOTS_X / 2);
			if (a > dmaWidth)
			{
				a -= dmaWidth;
				p++;			// gather pixels into next X slot
			}
		}

		arrYslotCnt += SLOTS_Y;
		if (arrYslotCnt > cropHeight)
		{
			arrYslotCnt -= cropHeight;
			arrP += SLOTS_X;			// points to next row
		}
		STM_EVAL_LEDOff(LED_RED);
	}
}




// IRQ handler called when VSYNC signal becomes active (in line 2.5 and line 315)
//		Vsync active ends in line 8.5 and line 321.5
void DCMI_IRQHandler(void)
{
	// check masked IE bits
	if (DCMI->MISR & DCMI_IT_VSYNC)
	{
		DCMI_CROPInitTypeDef DCMI_Crop;
		short x, y;
		long l;

		captureLeftRight = (captureLeftRight == 0 ? 1 : 0);		// toggle left/right side

		STM_EVAL_LEDOn(LED_ORN);

		// stop DMA, reset buffers to other half of picture and then restart
		DCMI_ClearFlag(DCMI_FLAG_VSYNCRI);
		DCMI_CaptureCmd(DISABLE);
		DCMI_Cmd(DISABLE);

		DMA_Cmd(DMA2_Stream1, DISABLE);
		while (DMA_GetCmdStatus(DMA2_Stream1) != DISABLE);


		// reset values because they may have been changed by user
		dmaWidth = captureWidth / 4;
		dmaBufLen = dmaWidth;

		arrP = (captureLeftRight == 0 ? &YCbCrSlots[0] : &YCbCrSlots[SLOTS_X/2]);
		arrYslotCnt = 0;

		DCMI_CROPCmd (DISABLE);
		DCMI_Crop.DCMI_CaptureCount 			= captureWidth-1;
		DCMI_Crop.DCMI_HorizontalOffsetCount 	= cropLeft + (captureLeftRight == 0 ? 0 : captureWidth);
		DCMI_Crop.DCMI_VerticalLineCount 		= cropHeight - 1;
		DCMI_Crop.DCMI_VerticalStartLine 		= cropTop;
		DCMI_CROPConfig (&DCMI_Crop);
		DCMI_CROPCmd (ENABLE);

		DMA_SetCurrDataCounter(DMA2_Stream1, dmaBufLen);
		DMA_MemoryTargetConfig(DMA2_Stream1, (uint32_t)&YCbCr_buf0[0], DMA_Memory_0);
		DMA_DoubleBufferModeConfig (DMA2_Stream1, (uint32_t)&YCbCr_buf1[0], DMA_Memory_0);
		DMA_DoubleBufferModeCmd (DMA2_Stream1, ENABLE);
		DMA_ClearITPendingBit(DMA2_Stream1, DMA_IT_TCIF1);
		DMA_Cmd(DMA2_Stream1, ENABLE);
		while (DMA_GetCmdStatus(DMA2_Stream1) != ENABLE);

		DCMI_Cmd(ENABLE);
		DCMI_CaptureCmd(ENABLE);			// start capture

		// now transform the YCbCr values into RGB-values

		{
			// when captureLeftRight = left then process the right half
			short offset = (captureLeftRight == 0 ? SLOTS_X/2 : 0);
			videoData_t *cp = (videoData_t *)(captureLeftRight == 0 ? &YCbCrSlots[SLOTS_X/2] : &YCbCrSlots[0]);


			for (y = 0; y < SLOTS_Y; y++)
			{
				for (x = offset; x < offset+SLOTS_X/2; x++)
				{
					if (cp->cnt > 0)
					{
						long Y = (cp->Y / cp->cnt) / 2;		// we have 2 Y values here
						long Cb =(cp->Cb / cp->cnt);		// build average values
						long Cr =(cp->Cr / cp->cnt);

						// we use integer arithmetics here to speed up; do it step by step, some compilers do strange things
						l = (Cr * 1403);				// red
						l /= 1000;
						l += Y;
						if (l < 0) l = 0;
						if (l > 254) l = 254;
						rgbSlots[y][x].R = l;

						l = (Cr * 714) + (Cb * 344);	// green
						l /= 1000;
						l = Y - l;
						if (l < 0) l = 0;
						if (l > 254) l = 254;
						rgbSlots[y][x].G = l;

						l = (Cb * 1773);				// blue
						l /= 1000;
						l += Y;
						if (l < 0) l = 0;
						if (l > 254) l = 254;
						rgbSlots[y][x].B = l;
					}
					cp->Cb = 0;
					cp->Cr = 0;
					cp->Y = 0;
					cp->cnt= 0;

					cp += 1;
				}
				cp += (SLOTS_X/2);	// skip to next line
			}
		}

		if (captureLeftRight == 0)
			captureReady = 1;			// semaphore for main loop to update LEDs

		STM_EVAL_LEDOff(LED_ORN);
	}

	if (DCMI->MISR & DCMI_IT_LINE)
	{
//		STM_EVAL_LEDToggle (LED_GRN);
		DCMI_ClearFlag(DCMI_FLAG_LINERI);
	}

	if (DCMI->MISR & DCMI_IT_FRAME)
	{
		//		STM_EVAL_LEDToggle (LED_RED);
		DCMI_ClearFlag(DCMI_FLAG_FRAMERI);
	}

	if (DCMI->MISR & DCMI_IT_OVF)
	{
		//		STM_EVAL_LEDToggle (LED_BLU);
		DCMI_ClearFlag(DCMI_FLAG_OVFRI);
		put_c2Host('O');
	}

	if (DCMI->MISR & DCMI_IT_ERR)
	{
		//		STM_EVAL_LEDToggle (LED_BLU);
		DCMI_ClearFlag(DCMI_FLAG_ERRRI);
		put_c2Host('E');
	}
}



// IRQ called when vertical blanking starts
// currently not used
void EXTI15_10_IRQHandler (void)
{
	if(EXTI_GetITStatus(EXTI_Line13) != RESET)		// V-Blank
	{
//		STM_EVAL_LEDToggle (LED_BLU);
		EXTI_ClearITPendingBit(EXTI_Line13);
	}
}




//-------------------------------------------------------------------------------------------------------------------

void TVP5150initIOports(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	EXTI_InitTypeDef   EXTI_InitStructure;
	NVIC_InitTypeDef   NVIC_InitStructure;

	//-----------------------------------------
	// Clock Enable Port-A, Port-B, Port-C und Port-E
	//-----------------------------------------
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);

	// init D7 port as output and set it low. Pin is sampled during reset to get I2C adress bit 0
	GPIO_InitStructure.GPIO_Pin = 	GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/*
	  PE11 -> gpio (out)  = TVP5150 PDN
	  PE12 -> gpio (in)	  = TVP5150 AVID
	  PE13 -> gpio (in)	  = TVP5150 VBLK		( is IRQ source)
	  PE14 -> gpio (out)  = TVP5150 Reset
	  PE15 -> gpio (in)	  = TVP5150 FID
	 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOE, GPIO_Pin_11);		// clear PDN power down
	GPIO_ResetBits(GPIOE, GPIO_Pin_14);		// activate RESET
	delay_ms (50);
	GPIO_SetBits(GPIOE, GPIO_Pin_11);			// release PDN
	delay_ms (50);
	GPIO_ResetBits(GPIOE, GPIO_Pin_6); 			// set D7 to 0
	delay_ms(50);
	GPIO_SetBits(GPIOE, GPIO_Pin_14);
	delay_ms (50);


	//-----------------------------------------
	// init port A
	//-----------------------------------------
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource4, GPIO_AF_DCMI); // PA4=DCMI_HSYNC -> HSNYC
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_DCMI); // PA6=DCMI_PCLK  -> PCLK

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//-----------------------------------------
	// init port B
	//-----------------------------------------
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_DCMI); // PB6=DCMI_D5    -> D5
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_DCMI); // PB7=DCMI_VSYNC -> VSYNC

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	//-----------------------------------------
	// init port C
	//-----------------------------------------
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_DCMI); // PC6=DCMI_D0    -> D0
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_DCMI); // PC7=DCMI_D1    -> D1
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_DCMI); // PC8=DCMI_D2    -> D2
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_DCMI); // PC9=DCMI_D3    -> D3

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	//-----------------------------------------
	// init port E
	//-----------------------------------------
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource4, GPIO_AF_DCMI); // PE4=DCMI_D4    -> D4
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource5, GPIO_AF_DCMI); // PE5=DCMI_D6    -> D6
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource6, GPIO_AF_DCMI); // PE6=DCMI_D7    -> D7

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* Connect EXTI Line0 to PE13 pin (V blanking) */
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource13);

	/* Configure EXTI Line13 */
	EXTI_InitStructure.EXTI_Line = EXTI_Line13;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	/* Enable and set EXTI Line13 Interrupt to the lowest priority */
	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}


//---------------------------------------------------------------------------------------------------

int tvp5150_read (short addr)
{
	return (I2C_ReadByte(TVP5150_I2C_ADR, addr));
}



static void dump_reg_range(char *s, u8 init, const u8 end, int max_line)
{
	int i = 0;

	while (init != (u8)(end + 1)) {
		if ((i % max_line) == 0) {
			if (i > 0)
				printf("\n");
			printf("tvp5150: %s reg 0x%02x = ", s, init);
		}
		printf("%02x ", tvp5150_read(init));

		init++;
		i++;
	}
	printf("\n");
}



static int tvp5150_log_status(void)
{
	printf("tvp5150: Video input source selection #1 = 0x%02x\n",
			tvp5150_read(R00_Video_input_source_selection_1));
	printf("tvp5150: Analog channel controls = 0x%02x\n",
			tvp5150_read(R01_Analog_channel_controls));
	printf("tvp5150: Operation mode controls = 0x%02x\n",
			tvp5150_read(R02_Operation_mode_controls));
	printf("tvp5150: Miscellaneous controls = 0x%02x\n",
			tvp5150_read(R03_Miscellaneous_controls));
	printf("tvp5150: Autoswitch mask= 0x%02x\n",
			tvp5150_read(R04_Autoswitch_mask));
	printf("tvp5150: Color killer threshold control = 0x%02x\n",
			tvp5150_read(R06_Color_killer_threshold_control));
	printf("tvp5150: Luminance processing controls #1 #2 and #3 = %02x %02x %02x\n",
			tvp5150_read(R07_Luminance_processing_control_1),
			tvp5150_read(R08_Luminance_processing_control_2),
			tvp5150_read(R0E_Luminance_processing_control_3));
	printf("tvp5150: Brightness control = 0x%02x\n",
			tvp5150_read(R09_Brightness_control));
	printf("tvp5150: Color saturation control = 0x%02x\n",
			tvp5150_read(R0A_Color_saturation_control));
	printf("tvp5150: Hue control = 0x%02x\n",
			tvp5150_read(R0B_Hue_control));
	printf("tvp5150: Contrast control = 0x%02x\n",
			tvp5150_read(R0C_Contrast_Control));
	printf("tvp5150: Outputs and data rates select = 0x%02x\n",
			tvp5150_read(R0D_Outputs_and_data_rates_select));
	printf("tvp5150: Configuration shared pins = 0x%02x\n",
			tvp5150_read(R0F_Configuration_shared_pins));
	printf("tvp5150: Active video cropping start = 0x%02x%02x\n",
			tvp5150_read(R11_Active_video_cropping_start_pixel_MSB),
			tvp5150_read(R12_Active_video_cropping_start_pixel_LSB));
	printf("tvp5150: Active video cropping stop  = 0x%02x%02x\n",
			tvp5150_read(R13_Active_video_cropping_stop_pixel_MSB),
			tvp5150_read(R14_Active_video_cropping_stop_pixel_LSB));
	printf("tvp5150: Genlock/RTC = 0x%02x\n",
			tvp5150_read(R15_Genlock_and_RTC));
	printf("tvp5150: Horizontal sync start = 0x%02x\n",
			tvp5150_read(R16_Horizontal_sync_start));
	printf("tvp5150: Vertical blanking start = 0x%02x\n",
			tvp5150_read(R18_Vertical_blanking_start));
	printf("tvp5150: Vertical blanking stop = 0x%02x\n",
			tvp5150_read(R19_Vertical_blanking_stop));
	printf("tvp5150: Chrominance processing control #1 and #2 = %02x %02x\n",
			tvp5150_read(R1A_Chrominance_control_1),
			tvp5150_read(R1B_Chrominance_control_2));
	printf("tvp5150: Interrupt reset register B = 0x%02x\n",
			tvp5150_read(R1C_Interrupt_reset_register_B));
	printf("tvp5150: Interrupt enable register B = 0x%02x\n",
			tvp5150_read(R1D_Interrupt_enable_register_B));
	printf("tvp5150: Interrupt configuration register B = 0x%02x\n",
			tvp5150_read(R1E_Interrupt_configuration_register_B));
	printf("tvp5150: Video standard = 0x%02x\n",
			tvp5150_read(R28_Video_standard));
	printf("tvp5150: Chroma gain factor: Cb=0x%02x Cr=0x%02x\n",
			tvp5150_read(R2C_Cb_gain_factor),
			tvp5150_read(R2D_Cr_gain_factor));
	printf("tvp5150: Macrovision on counter = 0x%02x\n",
			tvp5150_read(R2E_Macrovision_on_counter));
	printf("tvp5150: Macrovision off counter = 0x%02x\n",
			tvp5150_read(R2F_Macrovision_off_counter));
	printf("tvp5150: ITU-R BT.656.%d timing(TVP5150AM1 only)\n",
			(tvp5150_read(R30_656_revision_select) & 1) ? 3 : 4);
	printf("tvp5150: Device ID = %02x%02x\n",
			tvp5150_read(R80_Device_ID_MSB),
			tvp5150_read(R81_Device_ID_LSB));
	printf("tvp5150: ROM version = (hex) %02x.%02x\n",
			tvp5150_read(R82_ROM_version),
			tvp5150_read(R83_RAM_version_MSB));
	printf("tvp5150: Vertical line count = 0x%02x%02x\n",
			tvp5150_read(R84_Vertical_line_count_MSB),
			tvp5150_read(R85_Vertical_line_count_LSB));
	printf("tvp5150: Interrupt status register B = 0x%02x\n",
			tvp5150_read(R86_Interrupt_status_register_B));
	printf("tvp5150: Interrupt active register B = 0x%02x\n",
			tvp5150_read(R87_Interrupt_active_register_B));
	printf("tvp5150: Status regs #1 to #5 = %02x %02x %02x %02x %02x\n",
			tvp5150_read(R88_Status_register_1),
			tvp5150_read(R89_Status_register_2),
			tvp5150_read(R8A_Status_register_3),
			tvp5150_read(R8B_Status_register_4),
			tvp5150_read(R8C_Status_register_5));

	dump_reg_range("Teletext filter 1",   RB1_Teletext_filter_and_mask_1,
			RB1_Teletext_filter_and_mask_1 + 4, 8);
	dump_reg_range("Teletext filter 2",   RB6_Teletext_filter_and_mask_2,
			RB6_Teletext_filter_and_mask_2 + 4, 8);

	printf("tvp5150: Teletext filter enable = 0x%02x\n",
			tvp5150_read(RBB_Teletext_filter_control));
	printf("tvp5150: Interrupt status register A = 0x%02x\n",
			tvp5150_read(RC0_Interrupt_status_register_A));
	printf("tvp5150: Interrupt enable register A = 0x%02x\n",
			tvp5150_read(RC1_Interrupt_enable_register_A));
	printf("tvp5150: Interrupt configuration = 0x%02x\n",
			tvp5150_read(RC2_Interrupt_configuration_register_A));
	printf("tvp5150: VDP status register = 0x%02x\n",
			tvp5150_read(RC6_VDP_status));
	printf("tvp5150: FIFO word count = 0x%02x\n",
			tvp5150_read(RC7_FIFO_word_count));
	printf("tvp5150: FIFO interrupt threshold = 0x%02x\n",
			tvp5150_read(RC8_FIFO_interrupt_threshold));
	printf("tvp5150: FIFO reset = 0x%02x\n",
			tvp5150_read(RC9_FIFO_reset));
	printf("tvp5150: Line number interrupt = 0x%02x\n",
			tvp5150_read(RCA_Line_number_interrupt));
	printf("tvp5150: Pixel alignment register = 0x%02x%02x\n",
			tvp5150_read(RCC_Pixel_alignment_HSB),
			tvp5150_read(RCB_Pixel_alignment_LSB));
	printf("tvp5150: FIFO output control = 0x%02x\n",
			tvp5150_read(RCD_FIFO_output_control));
	printf("tvp5150: Full field enable = 0x%02x\n",
			tvp5150_read(RCF_Full_field_enable_));
	printf("tvp5150: Full field mode register = 0x%02x\n",
			tvp5150_read(RFC_Full_field_mode));

	dump_reg_range("CC   data",   R90_Closed_caption_data,
			R90_Closed_caption_data + 3, 8);

	dump_reg_range("WSS  data",   R94_WSS_CGMS_A_data,
			R94_WSS_CGMS_A_data + 5, 8);

	dump_reg_range("VPS  data",   R9A_VPS_Gemstar_2x_data,
			R9A_VPS_Gemstar_2x_data + 12, 8);

	dump_reg_range("VITC data",   RA7_VITC_data,
			RA7_VITC_data + 8, 10);

	dump_reg_range("Line mode",   RD0_Line_mode,
			RD0_Line_mode + 43, 8);
	return 0;
}
