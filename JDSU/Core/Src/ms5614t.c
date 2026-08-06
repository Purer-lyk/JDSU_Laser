/* AI/ms5614t.c */
#include "main.h"
#include "usbd_cdc_if.h"

#define DAC_DELAY 3
#define TRANSITION_STEPS 6

uint16_t frame = 0;
uint16_t adcData = 0;
uint32_t wave_time = 1;

uint8_t codeBuf[2] = {0};
uint8_t readBuf[2] = {0};
uint16_t IDACData[5] = {0};
uint16_t prevDAC[5] = {0};
uint16_t uADCOriginvalues[4] = {0};
int8_t unstableFlags[Number][4] = {0};

float tempData = 0;
uint16_t tempInt = 0;
uint16_t tempDec = 0;

uint16_t start_wave = 0;
uint16_t end_wave = 0;

HAL_StatusTypeDef dacRet = HAL_TIMEOUT;
HAL_StatusTypeDef dacrRet = HAL_TIMEOUT;

void delay_us(__IO uint32_t us){
		uint32_t i;
		SysTick_Config(SystemCoreClock/1000000);
		for(i=0;i<us;i++){
			while(!((SysTick->CTRL)&(1<<16)));
		}
		SysTick->CTRL &=(~SysTick_CTRL_ENABLE_Msk);
}

void delay_ms(__IO uint32_t ms){
	delay_us(ms*1000);
}

void delay_s(__IO uint32_t s){
	delay_us(s*1000000);
}

inline void short_delay(volatile uint32_t n)
{
    while (n--) __NOP();
}

/* SPI2 ?? 16bit */
static void SPI2_Send16(uint16_t data)
{
    HAL_SPI_Transmit(&hspi2, (uint8_t*)&data, 1, HAL_MAX_DELAY);
}

/* SPI3 ?? 16bit */
static void SPI3_Send16(uint16_t data)
{
    HAL_SPI_Transmit(&hspi3, (uint8_t*)&data, 1, HAL_MAX_DELAY);
}

/* ??:D15..D12 = A1 A0 PWR SPD, D11..D0 = code */
static uint16_t MakeFrame(MS5614T_Channel_t ch, uint16_t code, MS5614T_Speed_t spd, MS5614T_Power_t pwr)
{
    if (code > 4095u) code = 4095u;

    return (uint16_t)((((uint16_t)ch  & 0x03u) << 14) |
                      (((uint16_t)pwr & 0x01u) << 13) |
                      (((uint16_t)spd & 0x01u) << 12) |
                      (code & 0x0FFFu));
}

void MS5614T_SetCode(MS5614T_Channel_t ch, uint16_t code, MS5614T_Speed_t spd, MS5614T_Power_t pwr)
{
    frame = MakeFrame(ch, code, spd, pwr);

    if (pwr == MS5614T_POWERDOWN) DAC1_PD_LOW(); else DAC1_PD_HIGH();
    DAC1_LDAC_LOW();

    DAC1_FS_HIGH();
    short_delay(DAC_DELAY);

    DAC1_CS_LOW();
    short_delay(DAC_DELAY);

    DAC1_FS_LOW();
    short_delay(DAC_DELAY);

    SPI2_Send16(frame);

    short_delay(DAC_DELAY);
    DAC1_FS_HIGH();
    short_delay(DAC_DELAY);
    DAC1_CS_HIGH();
}

void MS5614T2_SetCode(MS5614T_Channel_t ch, uint16_t code, MS5614T_Speed_t spd, MS5614T_Power_t pwr)
{
		frame = MakeFrame(ch, code, spd, pwr);

    if (pwr == MS5614T_POWERDOWN) DAC2_PD_LOW(); else DAC2_PD_HIGH();
    DAC2_LDAC_LOW();

    DAC2_FS_HIGH();
    short_delay(DAC_DELAY);

    DAC2_CS_LOW();
    short_delay(DAC_DELAY);

    DAC2_FS_LOW();
    short_delay(DAC_DELAY);

    SPI3_Send16(frame);

    short_delay(DAC_DELAY);
    DAC2_FS_HIGH();
    short_delay(DAC_DELAY);
    DAC2_CS_HIGH();
}

HAL_StatusTypeDef  PI11210_SetCode(PI11210_Channeld_t channel, uint16_t code)
{
		codeBuf[0] = (code>>8) & 0xFF;
		codeBuf[1] = code & 0xFF;
		dacRet = HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(IDAC_7BIT_ADDR<<1), (uint16_t)(channel<<1), I2C_MEMADD_SIZE_8BIT, codeBuf, 2, 1000);
//		delay_ms(10);
		return dacRet;
}

void write_ms5614t_table(void){
		if(!dma_transfer_complete) return;

		int i;
	  int j;
		uint8_t Head = 0xFF;
	
//		memset(txBuffer, 0, PACK_SIZE*sizeof(uint8_t));
		txBuffer[0] = 0xEE;
		txBuffer[1] = 0xEE;
		txCount = 4;
	
		for (i = 0; i < Number;) 
		{
				if(workState != TABLE_STATE) break;
				if(ReceEndFlag==1 && aRxBuffer[0] == Head && aRxBuffer[1] == Head) modify_table_loop();
//				checkTemp(workState);
			  // 提前把一个波长的通道数据取出来
			  for(j = 0; j < 5; j++)
			  {
					  IDACData[j] = Wave_DAC[i][j];
				}
				i++;
			
				// 波长数据0x00就跳过
				if ((IDACData[0] == 0xFFFF) && (IDACData[1] == 0xFFFF) && (IDACData[2] == 0xFFFF)){
//						uint8_t p1 = Find_Peaks(adc1, peaks1, i-1);
//						uint8_t p2 = Find_Peaks(adc2, peaks2, i-1);
//						uint8_t p3 = Find_Peaks(adc3, peaks3, i-1);
//						uint8_t p4 = Find_Peaks(adc4, peaks4, i-1);
						uint8_t p1=0,p2=0,p3=0,p4=0;
				
						txBuffer[2] = ((i-1) >> 8) & 0xFF;
						txBuffer[3] = (i-1) & 0xFF;
					
						FillPeaks(p1,p2,p3,p4);
					
						sampleTemperature();
						sendTxBuffer(i-1, p1, p2, p3, p4);
						break;
				}

				if(dacTarget == 0)
				{
						MS5614T2_SetCode(MS5614T_DAC_A,IDACData[0],MS5614T_SPEED_FAST,MS5614T_NORMAL);
						MS5614T2_SetCode(MS5614T_DAC_C,IDACData[1],MS5614T_SPEED_FAST,MS5614T_NORMAL);
						MS5614T2_SetCode(MS5614T_DAC_B,IDACData[2],MS5614T_SPEED_FAST,MS5614T_NORMAL);
						MS5614T_SetCode(MS5614T_DAC_A,IDACData[3],MS5614T_SPEED_FAST,MS5614T_NORMAL);
						MS5614T_SetCode(MS5614T_DAC_C,IDACData[4],MS5614T_SPEED_FAST,MS5614T_NORMAL);
				}
				else if(dacTarget == 1)
				{
						PI11210_SetCode(IDAC5, IDACData[0]); // GAIN
						PI11210_SetCode(IDAC6, IDACData[1]); // SOA
						PI11210_SetCode(IDAC1, IDACData[2]); // PHASE
						PI11210_SetCode(IDAC4, IDACData[3]); // WAVEA
						PI11210_SetCode(IDAC7, IDACData[4]); // WAVEB
				}
				
//				sampleVoltage();
				
				sampleVoltageStable(i);// todo:increase sample time via unstableFlag
				
//				M1820Z_GetTmp();
				
//				adc1[i] = uADCOriginvalues[0];
//				adc2[i] = uADCOriginvalues[1];
//				adc3[i] = uADCOriginvalues[2];
//				adc4[i] = uADCOriginvalues[3];
				
				//delay_us(1);
		}
}

void write_ms5614t_manual(void){
		uint8_t Head = 0xFF;
		
//		USART_Queue_Send(&ReceEndFlag, 1);
		checkTemp(workState);
	
		if(ReceEndFlag==0) return;
		
		ReceEndFlag = 0;
		
		if (aRxBuffer[0] == Head && aRxBuffer[1] == Head)
		{
				if(aRxBuffer[2] == 0x00){
						scanWave();
				}
				else if(aRxBuffer[2] == 0x01){
						modify_table_loop();
				}
		}
		if ((aRxBuffer[0] != Head) && (aRxBuffer[0] != 0x00))
		{
				ClearRxBuff();
		}
}

void write_ms5614t_extra(void){
		uint8_t Head = 0xFF;
	
		checkTemp(workState);
		checkRT();
	
		if(ReceEndFlag==0) return;
		
		ReceEndFlag = 0;
		
		if (aRxBuffer[0] == Head && aRxBuffer[1] == Head)
		{
				if(aRxBuffer[2] == 0x00){
						singleValue();
				}
				else if(aRxBuffer[2] == 0x01){
						modify_table_loop();
				}
		}
		if ((aRxBuffer[0] != Head) && (aRxBuffer[0] != 0x00))
		{
				ClearRxBuff();
		}
}

void modify_table_loop(void){
		// 0x01 change wave_time
		if(aRxBuffer[3] == 0x01){
				wave_time = (aRxBuffer[7]<<24)+(aRxBuffer[8]<<16)+(aRxBuffer[9]<<8)+aRxBuffer[10];
//				uint8_t waveArray[2] = {(wave_time>>8)&0xFF, wave_time&0xFF};
//				USART_Queue_Send(waveArray, 2);
		}
		// 0x02 switch workState
		else if(aRxBuffer[3] == 0x02){
				workState = aRxBuffer[8];
				switch(workState)
				{
					case TABLE_STATE:
					{
						LED_MANUAL_LOW();
						LED_TABLE_HIGH();
						break;
					}
					case MANUAL_STATE:
					{
						LED_TABLE_LOW();
						LED_MANUAL_HIGH();
						memset(unstableFlags, 0, sizeof(unstableFlags));
						break;
					}
					case EXTRA_STATE:
					{
						LED_TABLE_HIGH();
						LED_MANUAL_HIGH();
						memset(unstableFlags, 0, sizeof(unstableFlags));
						break;
					}
				}
		}
		else if(aRxBuffer[3] == 0x03){
				dacTarget = aRxBuffer[8];
		}
		else if(aRxBuffer[3] == 0x04){
				getFilterDiff();
		}
		else if(aRxBuffer[3] == 0x05){
				getWaveRange();
		}
		ClearRxBuff();
		ReceEndFlag = 0;
}

void getFilterDiff(void){
		uint16_t getDiffIdx = 4;
	
		uint8_t dlow = 0, dhigh = 0;
	
		uint8_t diffs1 = aRxBuffer[getDiffIdx++];
		for(uint8_t i=0;i<diffs1;i++){
				dhigh = aRxBuffer[getDiffIdx++];
				dlow = aRxBuffer[getDiffIdx++];
				if(unstableFlags[(dhigh<<8)+dlow][0]<60)unstableFlags[(dhigh<<8)+dlow][0] += 1;
		}
		
		uint8_t diffs2 = aRxBuffer[getDiffIdx++];
		for(uint8_t i=0;i<diffs2;i++){
				dhigh = aRxBuffer[getDiffIdx++];
				dlow = aRxBuffer[getDiffIdx++];
				if(unstableFlags[(dhigh<<8)+dlow][1]<60)unstableFlags[(dhigh<<8)+dlow][1] += 1;
		}
		
		uint8_t diffs3 = aRxBuffer[getDiffIdx++];
		for(uint8_t i=0;i<diffs3;i++){
				dhigh = aRxBuffer[getDiffIdx++];
				dlow = aRxBuffer[getDiffIdx++];
				if(unstableFlags[(dhigh<<8)+dlow][2]<60)unstableFlags[(dhigh<<8)+dlow][2] += 1;
		}
		
		uint8_t diffs4 = aRxBuffer[getDiffIdx++];
		for(uint8_t i=0;i<diffs4;i++){
				dhigh = aRxBuffer[getDiffIdx++];
				dlow = aRxBuffer[getDiffIdx++];
				if(unstableFlags[(dhigh<<8)+dlow][3]<60)unstableFlags[(dhigh<<8)+dlow][3] += 1;
		}
}

void getWaveRange(void){
		uint16_t getResIdx = 4;
		
		int l_high = aRxBuffer[getResIdx++];
		int l_low = aRxBuffer[getResIdx++];
	
		int r_high = aRxBuffer[getResIdx++];
		int r_low = aRxBuffer[getResIdx++];
	
		start_wave = (l_high<<8)+l_low;
		end_wave = (r_high<<8)+r_low;
}

void sampleVoltage(void){
		delay_us(wave_time);
		for(uint8_t adc_idx=0;adc_idx<4;adc_idx++){
				adcData = ADC_Write_Read(adc_idx) & 0x0FFF;
				uADCOriginvalues[adc_idx] = adcData;
				txBuffer[txCount++] = (adcData >> 8) & 0xFF;
				txBuffer[txCount++] = adcData & 0xFF;
				txBuffer[txCount++] = 0;
		}
}

void sampleVoltageStable(uint16_t i){
//		delay_us(wave_time);
		uint8_t adc_idx=0;
		//uint8_t unstable_flag = 0;
		for(;adc_idx<4;adc_idx++){
//				unstable_flag = 0;
			
//				if(unstableFlags[i][adc_idx] >= 0) adcData = ADC_Write_Read_Stable(adc_idx, &unstable_flag, unstableFlags[i][adc_idx]+1) & 0x0FFF;
//				else adcData = ADC_Write_Read(adc_idx) & 0x0FFF;
			
				delay_us(wave_time*unstableFlags[i][adc_idx]*50);
				adcData = ADC_Write_Read(adc_idx) & 0x0FFF;
			
//				uADCOriginvalues[adc_idx] = adcData;
			
				txBuffer[txCount++] = (adcData >> 8) & 0xFF;
				txBuffer[txCount++] = adcData & 0xFF;
				//txBuffer[txCount++] = unstable_flag;
			
//				if(unstable_flag){
//					if(unstableFlags[i][adc_idx]<127)unstableFlags[i][adc_idx] += 1;
//				}
//				else{
//					if(unstableFlags[i][adc_idx]>-1)unstableFlags[i][adc_idx] -= 1;
//				}
		}
}

void sampleTemperature(void){
//		tempData = 150.1524;
		tempData = M1820Z_GetTmp();
		tempInt = (int)tempData;
		tempDec = (int)((tempData-tempInt)*10000);
		txBuffer[txCount++] = (tempInt>>8) & 0xFF;
		txBuffer[txCount++] = tempInt & 0xFF;
		txBuffer[txCount++] = (tempDec>>8) & 0xFF;
		txBuffer[txCount++] = tempDec & 0xFF;
}

void sendTxBuffer(int dac_size, int p1, int p2, int p3, int p4){
		int floating_size = 4+8*dac_size+3+4+4*(p1+p2+p3+p4)+4;
	
		txBuffer[txCount++] = 0xFF;
		txBuffer[txCount++] = 0xEF;
		
		if(txCount!=floating_size) return;
		
		dma_transfer_complete = 0;
		CDC_Transmit_FS(txBuffer, floating_size);
}

void ClearTxBuff(){
		for(uint8_t i=2;i<USART_TX_SIZE;i++){
				aTxBuffer[i] = 0;
		}
}

void ClearRxBuff(void){
		for (uint16_t i = 0; i < USART_RX_SIZE; i++)
	  {
				aRxBuffer[i] = 0;
		}
}

void checkTemp(uint8_t mode){
		tempData = M1820Z_GetTmp();
		tempInt = (int)tempData;
		tempDec = (int)((tempData-tempInt)*10000);
	
		aTxBuffer[2] = mode;// the mode of this tx
		aTxBuffer[3] = 0x01;// 0x01 refer temperature return
		aTxBuffer[4] = (tempInt>>8) & 0xFF;
		aTxBuffer[5] = tempInt & 0xFF;
		aTxBuffer[6] = (tempDec>>8) & 0xFF;
		aTxBuffer[7] = tempDec & 0xFF; 
	
		USB_Queue_Send(aTxBuffer, USART_TX_SIZE);
		ClearTxBuff();
}

void scanWave(void){
		if(aRxBuffer[3] == 0x00){
				scanWave_U();
		}
		else if(aRxBuffer[3] == 0x01){
				scanWave_I();
		}
}

void scanWave_U(void){
		uint16_t WriteData = 0;
		for(uint8_t i = 0; i < 5; i++)
		{
				WriteData = ((aRxBuffer[4 + 2 * i] << 8) + aRxBuffer[5 + 2 * i]);
				switch(i){
					case 0:MS5614T2_SetCode(MS5614T_DAC_A, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 1:MS5614T2_SetCode(MS5614T_DAC_C, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 2:MS5614T2_SetCode(MS5614T_DAC_B, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 3:MS5614T_SetCode(MS5614T_DAC_A, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 4:MS5614T_SetCode(MS5614T_DAC_C, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
				}			
		}
		
		uint8_t txIdx = 2;
		uint8_t sa = 1;
		aTxBuffer[txIdx++] = MANUAL_STATE;// the mode of this tx
		aTxBuffer[txIdx++] = 0x00;// 0x00 refer scan wave return
		aTxBuffer[txIdx++] = 0x21;// return flag
		
		adcData = ADC_Write_Read_Stable(6, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		aTxBuffer[txIdx++] = sa;
		
		sa = 1;
		adcData = ADC_Write_Read_Stable(7, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		aTxBuffer[txIdx++] = sa;
		
		USB_Queue_Send(aTxBuffer, USART_TX_SIZE);
		ClearRxBuff();
		ClearTxBuff();
}

void scanWave_I(void){
		uint16_t WriteData = 0;
		for(uint8_t i = 0; i < 5; i++)
		{
				WriteData = ((aRxBuffer[4 + 2 * i] << 8) + aRxBuffer[5 + 2 * i]);
				switch(i){
					case 0:PI11210_SetCode(IDAC5, WriteData);break;//GAIN
					case 1:PI11210_SetCode(IDAC6, WriteData);break;//SOA
					case 2:PI11210_SetCode(IDAC1, WriteData);break;//PHASE
					case 3:PI11210_SetCode(IDAC4, WriteData);break;//WAVEA
					case 4:PI11210_SetCode(IDAC7, WriteData);break;//WAVEB
				}			
		}
		
		uint8_t txIdx = 2;
		uint8_t sa = 1;
		aTxBuffer[txIdx++] = MANUAL_STATE;// the mode of this tx
		aTxBuffer[txIdx++] = 0x00;// 0x00 refer scan wave return
		aTxBuffer[txIdx++] = 0x21;// return flag
		
		adcData = ADC_Write_Read_Stable(6, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		aTxBuffer[txIdx++] = sa;
		
		sa = 1;
		adcData = ADC_Write_Read_Stable(7, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		aTxBuffer[txIdx++] = sa;
		
		USB_Queue_Send(aTxBuffer, USART_TX_SIZE);
		ClearRxBuff();
		ClearTxBuff();
}

void singleValue(void){
		if(aRxBuffer[3] == 0x00){
				singleValue_U();
		}
		else if(aRxBuffer[3] == 0x01){
				singleValue_I();
		}
}

void singleValue_U(void){
		uint16_t WriteData = 0;
		uint8_t txIdx = 2;
		uint8_t highRecv = 0, lowRecv = 0;
		aTxBuffer[txIdx++] = EXTRA_STATE;
		aTxBuffer[txIdx++] = 0x00;
	
		for(uint8_t i = 0; i < 5; i++)
		{
				highRecv = aRxBuffer[4+2*i];
				lowRecv = aRxBuffer[5+2*i];
				WriteData = (highRecv << 8) + lowRecv;
				aTxBuffer[txIdx++] = highRecv;
				aTxBuffer[txIdx++] = lowRecv;
				switch(i){
					case 0:MS5614T2_SetCode(MS5614T_DAC_A, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 1:MS5614T2_SetCode(MS5614T_DAC_C, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 2:MS5614T2_SetCode(MS5614T_DAC_B, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 3:MS5614T_SetCode(MS5614T_DAC_A, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
					case 4:MS5614T_SetCode(MS5614T_DAC_C, WriteData, MS5614T_SPEED_FAST, MS5614T_NORMAL);break;
				}		
		}
		USB_Queue_Send(aTxBuffer, USART_TX_SIZE);
		ClearRxBuff();
		ClearTxBuff();
}

void singleValue_I(void){
	  uint16_t WriteData = 0;
		uint8_t txIdx = 2;
		uint8_t highRecv = 0, lowRecv = 0;
		aTxBuffer[txIdx++] = EXTRA_STATE;
		aTxBuffer[txIdx++] = 0x00;
	
		for(uint8_t i = 0; i < 5; i++)
		{
				highRecv = aRxBuffer[4+2*i];
				lowRecv = aRxBuffer[5+2*i];
				WriteData = (highRecv << 8) + lowRecv;
				aTxBuffer[txIdx++] = highRecv;
				aTxBuffer[txIdx++] = lowRecv;
				switch(i){
					case 0:PI11210_SetCode(IDAC5, WriteData);break;//GAIN
					case 1:PI11210_SetCode(IDAC6, WriteData);break;//SOA
					case 2:PI11210_SetCode(IDAC1, WriteData);break;//PHASE
					case 3:PI11210_SetCode(IDAC4, WriteData);break;//WAVEA
					case 4:PI11210_SetCode(IDAC7, WriteData);break;//WAVEB
				}			
		}
		USB_Queue_Send(aTxBuffer, USART_TX_SIZE);
		
		ClearRxBuff();
		ClearTxBuff();
}

void checkRT(void){
		uint8_t txIdx = 2;
		uint8_t sa = 0;
		aTxBuffer[txIdx++] = EXTRA_STATE;
		aTxBuffer[txIdx++] = 0x02;
	
		adcData = ADC_Write_Read_Stable(6, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		
		adcData = ADC_Write_Read_Stable(7, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
	
		adcData = ADC_Write_Read_Stable(0, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		
		adcData = ADC_Write_Read_Stable(1, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
	
		adcData = ADC_Write_Read_Stable(2, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		
		adcData = ADC_Write_Read_Stable(3, &sa, 1) & 0x0FFF;
		aTxBuffer[txIdx++] = (adcData >> 8) & 0xFF;
		aTxBuffer[txIdx++] = (adcData) & 0xFF;
		
		USB_Queue_Send(aTxBuffer, USART_TX_SIZE);
		ClearTxBuff();
}
