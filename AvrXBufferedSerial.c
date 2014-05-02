/*
	AvrXBufferedSerial.c

	Sample code for fully buffered interrupt driven serial I/O for the
	AVR processor.  Uses the AvrXFifo facility.

	Author: Larry Barello (larry@barello.net)

	Revision History:
	09-13-2005	- Initial version

	pitschu - July 2013:
		- ported from AVR to STM32F4
		- extended buffer length from max 255 to max 32767
 */

//------------------------------------------------------------------------------
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include "hardware.h"
#define _AVRXSERIALIO_C_
#include "AvrXSerialIo.h"


AVRX_DECL_FIFO(fifoToHost, FIFOLEN_TOHOST);
AVRX_DECL_FIFO(fifoFromHost, FIFOLEN_FROMHOST);


int put_c2Host(char c)	// Non blocking output
{
	int retc;
	retc = AvrXPutFifo(fifoToHost, c);

	if (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == SET)
		USART_SendData(USART3, AvrXPullFifo(fifoToHost));

	return retc;
}


int put_char2Host( char c)	// Blocking output
{
	AvrXWaitPutFifo(fifoToHost, c);
	if (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == SET)
		USART_SendData(USART3, AvrXPullFifo(fifoToHost));

	return 0;
}

int write_str2Host (char* p)
{
	while (*p)
		put_char2Host (*p++);

	return 0;
}

int get_cHost(void)	// Non blocking, return status outside of char range
{
	int retc = AvrXPullFifo(fifoFromHost);
	return retc;
}


int get_charHost(void)	// Blocks waiting for something
{
	return AvrXWaitPullFifo(fifoFromHost);
}





void USART3_IRQHandler(void)		// handle chars to/from sensor unit
{
	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		uint8_t c = USART_ReceiveData(USART3) & 0xFF;

		USART_ClearITPendingBit(USART3, USART_IT_RXNE);          /* Clear the USART Receive interrupt                   */

		if (AvrXPutFifo(fifoFromHost, c) >= 0)		// ignore errors
			put_c2Host((int)c);			// echo back to host
	}

	if (USART_GetITStatus(USART3, USART_IT_TC) != RESET)
	{
		USART_ClearITPendingBit(USART3, USART_IT_TC);           /* Clear the USART transmit interrupt                  */

		int c = AvrXPullFifo(fifoToHost);					// Return -1 if empty

		if (c >= 0)
			USART_SendData(USART3, c & 0xff);
	}
	if (USART3->SR & 0x0F)			// if any error flag is 1 then read DR to clear them
	{
		volatile short s = USART3->DR;
	}
}





void  USART3_Init (int baudRate)				// to host
{
	GPIO_InitTypeDef        GPIO_InitStructure;
	USART_InitTypeDef       USART_InitStructure;
	NVIC_InitTypeDef        NVIC_InitStructure;

	AVRX_INIT_FIFO (fifoToHost);
	AVRX_INIT_FIFO (fifoFromHost);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);  // PD8 -> TX
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);  // PD9 -> RX

	/* Configure USARTx_Tx as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* Configure USARTx_Rx as input with pull up */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate            = baudRate;
	USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits            = USART_StopBits_1;
	USART_InitStructure.USART_Parity              = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART3, &USART_InitStructure);
	USART_Cmd(USART3, ENABLE);

	/* Enable the USARTy Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART3, USART_IT_TC, ENABLE);
}


