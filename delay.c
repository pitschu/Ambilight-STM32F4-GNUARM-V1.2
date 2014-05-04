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
#include "moodLight.h"
#include "ws2812.h"
#include "hardware.h"

volatile uint32_t 			system_time = 0;
volatile static uint8_t  	_delay_sem = 0xff;		// FF = init before first use

volatile uint8_t	butState;		// current status of button (1=pressed)
volatile uint16_t	butCount;		// ticks since last change of button
volatile uint16_t	butONcount;		// how long was button pressed last (in ticks)
volatile uint16_t	butOFFcount;	// how long was button released



// systick runs at 100Hz and simply increases the global <system_time>

void init_systick(void)
{
	RCC_ClocksTypeDef RCC_Clocks;

	// 100 Hz
	RCC_GetClocksFreq(&RCC_Clocks);

	if(0 != SysTick_Config(SystemCoreClock / 100)) {
		while(1);
	}
}


void SysTick_Handler(void)
{
	system_time++;

	if (ws2812ovrlayCounter > 0)
		ws2812ovrlayCounter--;

	uint8_t actButton = STM_EVAL_PBGetState (BUTTON_USER);

	if (butState != actButton)
	{
		if (butCount > 10)			// has really changed -> save old state
		{
			if (actButton == Bit_RESET)				// button was down and is now up
				butONcount = butCount;
			else
				butOFFcount = butCount;

		}
		butState = actButton;
		butCount = 0;
	}
	else
	{
		if (butCount < 0xffff)
			butCount++;
	}

}




// TIM4 is used for delay_ms() or delay_us() functions.

void DelayCountInit(void)
{
// time TIM4 runs at 1 Mhz

	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 4;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4 , ENABLE);
	TIM_DeInit(TIM4);
	TIM_TimeBaseStructure.TIM_Period=10000;
	TIM_TimeBaseStructure.TIM_Prescaler= (uint16_t) ((SystemCoreClock / 2) / 1000000) - 1;	// -> 1 Mhz
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
	TIM_ClearFlag(TIM4, TIM_FLAG_Update);
	TIM_ITConfig(TIM4,TIM_IT_Update, DISABLE);		// enabled when delay funcs are used
	TIM_Cmd(TIM4, ENABLE);
}


void TIM4_IRQHandler(void)
{
	if ( TIM_GetITStatus(TIM4 , TIM_IT_Update) != RESET )
		_delay_sem = 1;

	TIM_ClearITPendingBit(TIM4 , TIM_FLAG_Update);
	TIM_ITConfig(TIM4,TIM_IT_Update,DISABLE);
	TIM_Cmd(TIM4, DISABLE);
}



// active wait for <time_us> micro seconds

void delay_us(uint32_t time_us)
{
	_delay_sem = 0;
	TIM_SetCounter(TIM4, 0);
	TIM_SetAutoreload(TIM4, (time_us > 5 ? time_us-1 : 5));		// if 0 then default to 5usec
	TIM_ClearITPendingBit(TIM4 , TIM_FLAG_Update);
	TIM_ITConfig(TIM4,TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM4, ENABLE);

	while (_delay_sem == 0)				// wait for timeout IRQ
		;
}



// active wait for <time_ms> milli seconds

void delay_ms(uint16_t time_ms)
{
	while (time_ms>0)
	{
		delay_us(1000);
		time_ms--;
	}
}
