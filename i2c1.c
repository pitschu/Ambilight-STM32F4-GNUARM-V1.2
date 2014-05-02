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

#include "i2c1.h"

#define   I2C_TIMEOUT     0x8000


void 	I2C_startI2C(void);
int 	I2C_timeout(void);





void I2C_InitHardware(void)
{
	static uint8_t initDone = 0;
	GPIO_InitTypeDef  GPIO_InitStructure;

	// do only once
	if(initDone !=0)
		return;

	// Clock enable
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	// I2C reset
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);

	// I2C alternate functions
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_I2C1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

	// set pin options
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;

	// SCL
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	// SDA
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// I2C start
	I2C_startI2C();

	initDone = 1;
}




int16_t I2C_ReadByte(uint8_t slave_adr, uint8_t adr)
{
	int16_t ret_wert=0;
	uint32_t timeout=I2C_TIMEOUT;

	I2C_GenerateSTART(I2C1, ENABLE);

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C_AcknowledgeConfig(I2C1, DISABLE);
	I2C_Send7bitAddress(I2C1, slave_adr, I2C_Direction_Transmitter);

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C1->SR2;

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	// send rigister addr
	I2C_SendData(I2C1, adr);

	timeout=I2C_TIMEOUT;
	while ((!I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) || (!I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF))) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	// restart
	I2C_GenerateSTART(I2C1, ENABLE);

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C_Send7bitAddress(I2C1, slave_adr, I2C_Direction_Receiver);

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C1->SR2;

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C_GenerateSTOP(I2C1, ENABLE);

	ret_wert=(int16_t)(I2C_ReceiveData(I2C1));
	I2C_AcknowledgeConfig(I2C1, ENABLE);

	return(ret_wert);
}




int16_t I2C_WriteByte(uint8_t slave_adr, uint8_t adr, uint8_t wert)
{
	int16_t ret_wert=0;
	uint32_t timeout=I2C_TIMEOUT;

	I2C_GenerateSTART(I2C1, ENABLE);

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_SB)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C_Send7bitAddress(I2C1, slave_adr, I2C_Direction_Transmitter);

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C1->SR2;

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	// register addr
	I2C_SendData(I2C1, adr);

	timeout=I2C_TIMEOUT;
	while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	// register data
	I2C_SendData(I2C1, wert);

	timeout=I2C_TIMEOUT;
	while ((!I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE)) || (!I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF))) {
		if(timeout!=0) timeout--; else return(I2C_timeout());
	}

	I2C_GenerateSTOP(I2C1, ENABLE);

	ret_wert=0;

	return(ret_wert);
}





void I2C_startI2C(void)
{
	I2C_InitTypeDef  I2C_InitStructure;

	I2C_InitStructure.I2C_Mode = 				I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = 			I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 		0x00;
	I2C_InitStructure.I2C_Ack = 				I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 			400000;

	I2C_Cmd(I2C1, ENABLE);
	I2C_Init(I2C1, &I2C_InitStructure);
}



int I2C_timeout()
{
	// Stop und Reset
	I2C_GenerateSTOP(I2C1, ENABLE);
	I2C_SoftwareResetCmd(I2C1, ENABLE);
	I2C_SoftwareResetCmd(I2C1, DISABLE);

	I2C_DeInit(I2C1);
	I2C_startI2C();

	return 0;
}
