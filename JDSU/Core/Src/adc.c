#include "main.h"

uint16_t adcSPI = 0;
uint32_t adcCount = 0;
uint16_t adcStable = 0;
uint16_t adcUnstable = 0;
uint16_t adcQueue[WINDOW_SIZE] = {0};
uint16_t adcUnstableList[Number] = {0};

void Reset_ADC_Queue(void){
		for(uint8_t i=0;i<10;i++){
				adcQueue[i] = 0;
		}
		adcCount = 0;
		adcStable = 0;
}

static void ADC_Select_Chs(){
		uint16_t Chs = (1<<0) | (1<<1) | (1<<2) | (1<<3) | (1<<6) | (1<<7);
		ADC_SPI_Cmd(Chs);
}

void ADC_LOOP_SPI_Init(void){
		volatile uint8_t tmp;
		__HAL_SPI_CLEAR_OVRFLAG(&hspi1);
		(void)tmp;
		
		ADC_SPI_Cmd(0x8000);
		ADC_Select_Chs();
		ADC_Loop_Start();
}

void ADC_MANUAL_SPI_Init(void){
		volatile uint8_t tmp;
		__HAL_SPI_CLEAR_OVRFLAG(&hspi1);
		(void)tmp;
	
		ADC_SPI_Cmd(0x8000);
}

uint16_t ADC_SPI_Cmd(uint16_t cmdF){
		ADC_CS_HIGH();
		short_delay(1);
		ADC_CS_LOW();
	
		HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)&cmdF, (uint8_t*)&adcSPI, 1, HAL_MAX_DELAY);
	
		return adcSPI;
}

uint16_t ADC_Write_Read(uint8_t ch){
		uint16_t frame = (0x1 << 12) | (ch << 7);
		ADC_SPI_Cmd(frame);
		short_delay(1);
//		ADC_SPI_Cmd(frame);
//		short_delay(7);
		return ADC_SPI_Cmd(frame);
}

uint16_t ADC_Write_Read_Stable(uint8_t ch, uint8_t* unstable, uint8_t multi){
		uint16_t frame = (0x1 << 12) | (ch << 7);
		Reset_ADC_Queue();
	
		uint32_t multiStCount = multi*STABLECOUNT;
		uint32_t multiQueSize = multi*QUEUE_SIZE;
	
		while(adcStable<multiStCount){
				adcQueue[adcCount%WINDOW_SIZE] = ADC_SPI_Cmd(frame);
				
				if(adcCount==(multiQueSize-1)){
//						uint16_t adcSum = 0;
//						for(uint16_t i=1;i<QUEUE_SIZE;i++){
//								adcSum+=adcQueue[i];
//						}
						*unstable = 1;
//						return adcQueue[adcCount%QUEUE_SIZE];
						return adcQueue[adcCount%WINDOW_SIZE];
				}
			
//				if(adcCount>2 && abs((int)(adcQueue[(adcCount-1)%QUEUE_SIZE])-(int)(adcQueue[(adcCount-2)%QUEUE_SIZE]))<205) adcStable++;
//				else adcStable=0;
			
				if(adcCount>=WINDOW_SIZE && adcCount%WINDOW_SIZE==0){
//						uint16_t cur = (adcCount-WINDOW_SIZE+1)%QUEUE_SIZE;
						uint16_t cur = 0;
						uint16_t amax = adcQueue[cur];
						uint16_t amin = adcQueue[cur];
						for(uint16_t j=1;j<WINDOW_SIZE;j++){
//								cur = (cur+1)%QUEUE_SIZE;
								cur = j;
								if(adcQueue[cur]>amax) amax = adcQueue[cur];
								if(adcQueue[cur]<amin) amin = adcQueue[cur];
						}
						if(amax-amin<STABLERANGE) adcStable++;
						else adcStable=0;
				}
				adcCount++;
		}
		return adcQueue[(adcCount-1)%WINDOW_SIZE];
}

void ADC_Loop_Start(void){
		ADC_SPI_Cmd(ADC_RESET_FRAME);
		ADC_SPI_Cmd(ADC_ENTER_FRAME);
}

uint16_t ADC_Write_Loop(void){
		return ADC_SPI_Cmd(ADC_LOOP_FRAME);
}
