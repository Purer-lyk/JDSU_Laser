#include "main.h"

uint16_t temperature = 0;
uint32_t pwm_buffer[ARR_1] = {0};

void DQ_IN(void){
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = TEMP_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(TEMP_PORT, &GPIO_InitStruct);
}

void DQ_OUT(void){
		GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = TEMP_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		HAL_GPIO_Init(TEMP_PORT, &GPIO_InitStruct);
}

uint8_t M1820Z_Reset(void){
		uint8_t response;

    DQ_OUT();
    HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_RESET);
    delay_us(500); // 480us+

    HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_SET);
		delay_us(500);
	
//    DQ_IN();

//    response = HAL_GPIO_ReadPin(TEMP_PORT, TEMP_PIN);

//    delay_us(600);

    return response;
}

//1100 1100
void M1820Z_WriteBit(uint8_t bit){
		DQ_OUT();

		HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_RESET);
		delay_us(1);
		
		if(bit)
				HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_SET);

		delay_us(70);

		HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_SET);
}

uint8_t M1820Z_ReadBit(void){
		uint8_t bit = 0;

//		DQ_OUT();
		HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_RESET);
		delay_us(3);
		HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_SET);

//		DQ_IN();
		delay_us(11);

		if(HAL_GPIO_ReadPin(TEMP_PORT, TEMP_PIN))
				bit = 1;

		delay_us(50);

		return bit;
}

void M1820Z_WriteByte(uint8_t data){
		//Byte from low to high
		for(uint8_t i=0;i<8;i++)
		{
				M1820Z_WriteBit(data & 0x01);
				data >>= 1;
		}
}

uint8_t M1820Z_ReadByte(void){
		uint8_t data = 0;

		for(int i=0;i<8;i++)
		{
				data >>= 1;
				if(M1820Z_ReadBit())
						data |= 0x80;
		}
		return data;
}

float M1820Z_GetTmp(void){
		uint8_t temp_l, temp_h;
    int16_t temp = 0;

    M1820Z_Reset();
    M1820Z_WriteByte(0xCC);
    M1820Z_WriteByte(0x44);

    delay_ms(5);
//		HAL_GPIO_WritePin(TEMP_PORT, TEMP_PIN, GPIO_PIN_SET);
//		while(M1820Z_ReadBit()==0){delay_us(2);}

    M1820Z_Reset();
    M1820Z_WriteByte(0xCC);
    M1820Z_WriteByte(0xBE);

    temp_l = M1820Z_ReadByte();
    temp_h = M1820Z_ReadByte();
	
    temp = (int16_t)((temp_h << 8) | temp_l);
		
		float tmpResult;
//		if(direct) tmpResult = 40.f-(float)temp/256.f;
//		else tmpResult = 40.f+(float)temp/256.f;
		tmpResult = 40.f+(float)temp/256.f;

    return tmpResult;
}

void Set_Soft_PWM_Duty(uint8_t duty)
{
		uint16_t ratio = ARR_1/100;
    if(duty > 100) duty = 100;
    
    for(int i = 0; i < ARR_1; i++)
    {
        if(i < duty*ratio)
        {
            pwm_buffer[i] = (1 << 2);
        }
        else
        {
            pwm_buffer[i] = (1 << 18);
        }
    }
}
