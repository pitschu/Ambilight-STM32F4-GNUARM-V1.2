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
 *	05.05.2014	pitschu	v1.1 minor changes
 */


#ifndef __TVP5150_DCMI_H_
#define __TVP5150_DCMI_H_


//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_dcmi.h"
#include "stm32f4xx_dma.h"
#include "i2c1.h"
#include "hardware.h"


#define		SLOTS_X			64			// LED slots with RGB values from picture processor
#define		SLOTS_Y			40

#define		CROP_TOP_MIN	24
#define		CROP_HEIGHT_MAX	284
#define		CROP_LEFT_MIN	160
#define		CROP_WIDTH_MAX
#define  	TVP5150_I2C_ADR		0xB8	// slave addr of chip

// TVP5150AM1 register address and default values
#define R00_Video_input_source_selection_1    		0x00    // 00h  R/W
#define R01_Analog_channel_controls    				0x01    // 15h  R/W
#define R02_Operation_mode_controls   				0x02    // 00h  R/W
#define R03_Miscellaneous_controls    				0x03    // 01h  R/W
#define R04_Autoswitch_mask    						0x04    // DCh  R/W
#define R05_Reserved    							0x05    // 00h  R/W
#define R06_Color_killer_threshold_control    		0x06    // 10h  R/W
#define R07_Luminance_processing_control_1    		0x07    // 60h  R/W
#define R08_Luminance_processing_control_2    		0x08    // 00h  R/W
#define R09_Brightness_control    					0x09    // 80h  R/W
#define R0A_Color_saturation_control    			0x0A    // 80h  R/W
#define R0B_Hue_control    							0x0B    // 00h  R/W
#define R0C_Contrast_Control    					0x0C    // 80h  R/W
#define R0D_Outputs_and_data_rates_select    		0x0D    // 47h  R/W
#define R0E_Luminance_processing_control_3    		0x0D    // 47h  R/W
#define R0F_Configuration_shared_pins    			0x0F    // 08h  R/W
#define R11_Active_video_cropping_start_pixel_MSB   0x11    // 00h  R/W
#define R12_Active_video_cropping_start_pixel_LSB   0x12    // 00h  R/W
#define R13_Active_video_cropping_stop_pixel_MSB    0x13    // 00h  R/W
#define R14_Active_video_cropping_stop_pixel_LSB    0x14    // 00h  R/W
#define R15_Genlock_and_RTC    						0x15    // 01h  R/W
#define R16_Horizontal_sync_start    				0x16    // 80h  R/W
#define R18_Vertical_blanking_start   				0x18    // 00h  R/W
#define R19_Vertical_blanking_stop    				0x19    // 00h  R/W
#define R1A_Chrominance_control_1    				0x1A    // 0Ch  R/W
#define R1B_Chrominance_control_2    				0x1B    // 14h  R/W
#define R1C_Interrupt_reset_register_B    			0x1C    // 00h  R/W
#define R1D_Interrupt_enable_register_B    			0x1D    // 00h  R/W
#define R1E_Interrupt_configuration_register_B    	0x1E    // 00h  R/W
#define R21_Indirect_Register_Data    				0x21    // 00h  R/W
#define R23_Indirect_Register_Address    			0x23    // 00h  R/W
#define R24_Indirect_Register_ReadWrite_Strobe    	0x24    // 00h  R/W
#define R28_Video_standard    						0x28    // 00h  R/W
#define R2C_Cb_gain_factor    						0x2C    // R
#define R2D_Cr_gain_factor    						0x2D    // R
#define R2E_Macrovision_on_counter    				0x2E    // 0Fh  R/W
#define R2F_Macrovision_off_counter    				0x2F    // 01h  R/W
#define R30_656_revision_select    					0x30    // 00h  R/W
#define R33_RAM_Version_LSB   	 					0x33    // 00h  R
#define R7E_Patch_Write_Address    					0x7E    // 00h  R/W(2)
#define R7F_Patch_Code_Execute    					0x7F    // 00h  R/W(2)
#define R80_Device_ID_MSB    						0x80    // 51h  R
#define R81_Device_ID_LSB    						0x81    // 50h  R
#define R82_ROM_version    							0x82    // 04h  R
#define R83_RAM_version_MSB    						0x83    // 00h  R
#define R84_Vertical_line_count_MSB    				0x84    // R
#define R85_Vertical_line_count_LSB    				0x85    // R
#define R86_Interrupt_status_register_B    			0x86    // R
#define R87_Interrupt_active_register_B    			0x87    // R
#define R88_Status_register_1    					0x88    // R
#define R89_Status_register_2    					0x89    // R
#define R8A_Status_register_3    					0x8A    // R
#define R8B_Status_register_4    					0x8B    // R
#define R8C_Status_register_5    					0x8C    // R
#define R8E_Patch_Read_Address    					0x8E    // 00h  R/W(2)
#define R90_Closed_caption_data    					0x90    // R
#define R94_WSS_CGMS_A_data    						0x94    // R
#define R9A_VPS_Gemstar_2x_data    					0x9A    // R
#define RA7_VITC_data    							0xA7    // R
#define RB0_VBI_FIFO_read_data    					0xB0    // R
#define RB1_Teletext_filter_and_mask_1    			0xB1    // 00h  R/W
#define RB6_Teletext_filter_and_mask_2    			0xB6    // 00h  R/W
#define RBB_Teletext_filter_control    				0xBB    // 00h  R/W
#define RC0_Interrupt_status_register_A    			0xC0    // 00h  R/W
#define RC1_Interrupt_enable_register_A    			0xC1    // 00h  R/W
#define RC2_Interrupt_configuration_register_A    	0xC2    // 04h  R/W
#define RC3_VDP_configuration_RAM_data    			0xC3    // DCh  R/W
#define RC4_VDP_configuration_RAM_address_low_byte  0xC4    // 0Fh  R/W
#define RC5_VDP_configuration_RAM_address_high_byte 0xC5    // 00h  R/W
#define RC6_VDP_status    							0xC6    // R
#define RC7_FIFO_word_count    						0xC7    // R
#define RC8_FIFO_interrupt_threshold   				0xC8    // 80h  R/W
#define RC9_FIFO_reset								0xC9	// 00h  W
#define RCA_Line_number_interrupt    				0xCA    // 00h  R/W
#define RCB_Pixel_alignment_LSB    					0xCB    // 4Eh  R/W
#define RCC_Pixel_alignment_HSB    					0xCC    // 00h  R/W
#define RCD_FIFO_output_control    					0xCD    // 01h  R/W
#define RCF_Full_field_enable_    					0xCF    // 00h  R/W
#define RD0_Line_mode    							0xD0    // 00h  R/W
#define RFC_Full_field_mode    						0xFC    // 7Fh  R/W




/*
 * Embedded sync codes (ITU-BT.656)
 * Not used in my hardware based implementation
 * Bit 7 is always 1
	F V H 	P3 P2 P1 P0		code
	0 0 0 	0  0  0  0		80
	0 0 1 	1  1  0  1		9D
	0 1 0 	1  0  1  1		AB
	0 1 1 	0  1  1  0		B6
	1 0 0 	0  1  1  1 		C7
	1 0 1 	1  0  1  0		DA
	1 1 0 	1  1  0  0		EC
	1 1 1 	0  0  0  1		F1
*/



short TVP5150init(void);
void TVP5150startCapture(void);
void TVP5150stopCapture(void);
void TVP5150setPictureParams (void);
int tvp5150_read (short addr);
void TVP5150selectVideoSource (unsigned char src);
unsigned char TVP5150hasVideoSignal ();
unsigned char TVP5150getStatus1 ();


extern volatile rgbValue_t  rgbSlots [SLOTS_Y][SLOTS_X];
extern volatile short 		captureReady;

extern unsigned char		videoSourceSelect;			// 0 = auto; 1/2 = video channel
extern unsigned char		videoCurrentSource;			// current video channel set in TVP5150
extern unsigned char		Brightness;
extern unsigned char		Color_saturation;
extern   signed char		Hue_control;
extern unsigned char		Contrast;

extern unsigned long	captureWidth;
extern unsigned long	cropLeft;
extern unsigned long	cropTop;
extern unsigned long	cropHeight;

extern unsigned short	tvp5150AGC;					// pitschu 140505: user selectable setting (usrinterface.c)

//--------------------------------------------------------------
#endif
