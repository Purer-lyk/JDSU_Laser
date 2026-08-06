#include "main.h"
#include "usbd_cdc_if.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

//UART1 RX
uint8_t getData = 0;
uint8_t lastGet = 0;
uint8_t ReceEndFlag = 0;
__IO uint8_t uhRxCounter = 0;
uint8_t aRxBuffer[USART_RX_SIZE] = {0};
uint8_t uartFrame[USART_RX_SIZE] = {0};

//UART6 RX
uint8_t atReceEndFlag = 0;
uint8_t atRxBuffer[AT_RX_SIZE] = {0};
uint8_t atFrame[AT_RX_SIZE] = {0};

//UART1 TX
uint8_t txHead = 0;
uint8_t txTail = 0;
uint16_t txCount = 0;
uint8_t dma_transfer_complete = 1;
uint8_t txQueue[TX_QUEUE_SIZE][USART_TX_SIZE] = {0};
uint16_t txLen[USART_TX_SIZE] = {0};

//UART6 TX
uint8_t atTxHead = 0;
uint8_t atTxTail = 0;
uint8_t at_transfer_complete = 1;
uint8_t atTxQueue[TX_QUEUE_SIZE][USART_TX_SIZE] = {0};
uint16_t atTxLen[USART_TX_SIZE] = {0};

void USART_Queue_Send(uint8_t *data, uint16_t len)
{
    memcpy(txQueue[txHead], data, len);
    txLen[txHead] = len;
    txHead = (txHead + 1) % TX_QUEUE_SIZE;

    if(dma_transfer_complete)
    {
        USART_DMA_Send();
    }
}

void USART_DMA_Send(void)
{
	if(txTail == txHead) return;

	dma_transfer_complete = 0;

	HAL_UART_Transmit_DMA(&huart1,
												txQueue[txTail],
												txLen[txTail]);

	txTail = (txTail + 1) % TX_QUEUE_SIZE;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		if(workState == MANUAL_STATE)
		{
//				dma_transfer_complete = 1;
				USART_DMA_Send();
		}
		else
		{
				USART_DMA_Send();
//				uint8_t flag = 0x21;
//				HAL_UART_Transmit_DMA(&huart1, &flag, 1);
		}
		dma_transfer_complete = 1;
	}
	else if(huart->Instance == USART6)
	{
		at_transfer_complete = 1;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART1)
	{
//		HAL_UART_DMAStop(&huart1);
		
		memcpy(aRxBuffer, uartFrame, USART_RX_SIZE*sizeof(uint8_t));
		
		ReceEndFlag = 1;
//		USART_Queue_Send(aRxBuffer, USART_RX_SIZE);

		HAL_UART_Receive_DMA(&huart1, uartFrame, USART_RX_SIZE);
	}
}

void USB_Queue_Send(uint8_t *data, uint16_t len){
	memcpy(txQueue[txHead], data, len);
	txLen[txHead] = len;
	txHead = (txHead + 1) % TX_QUEUE_SIZE;

	if(dma_transfer_complete)
	{
			USB_SendNext();
	}
}

void USB_SendNext(void){
	if(txTail == txHead){
		dma_transfer_complete = 1;
		return;
	}

	dma_transfer_complete = 0;

	CDC_Transmit_FS(txQueue[txTail], txLen[txTail]);

	txTail = (txTail + 1) % TX_QUEUE_SIZE;
}

void USB_IRQHandler_Process(void)
{
    USBD_CDC_HandleTypeDef *hcdc =
        (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

    if (hcdc != NULL &&
        hcdc->TxState == 0 &&
        USBD_BUSY)
    {
        USB_SendNext();
    }
}

void HAL_UARTEx_RxEventCallback(
        UART_HandleTypeDef *huart,
        uint16_t Size)
{
    if(huart->Instance == USART6)
    {
				memcpy(atRxBuffer, atFrame, Size*sizeof(uint8_t));
			
        atReceEndFlag = 1;
			
//				HAL_UART_Transmit_DMA(&huart1, atRxBuffer, Size);
				
				memset(atFrame, 0, AT_RX_SIZE);

        HAL_UARTEx_ReceiveToIdle_DMA(
                &huart6,
                atFrame,
                AT_RX_SIZE
        );
				__HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);
    }
}
