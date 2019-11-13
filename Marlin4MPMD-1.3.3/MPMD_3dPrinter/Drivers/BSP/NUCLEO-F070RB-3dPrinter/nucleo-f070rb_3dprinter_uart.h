/** 
  ******************************************************************************
  * @file    nucleo-f070rb_3dPrinter_uart.h
  * @author  IPC Rennes
  * @version V1.0.0
  * @date    January 29, 2015
  * @brief   Header for motor functions of 3D Printer BSP driver 
  *  (based on L6474)
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************  
  */ 
  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __NUCLEO_F070RB_3DPRINTER_UART_H
#define __NUCLEO_F070RB_3DPRINTER_UART_H

#ifdef __cplusplus
 extern "C" {
#endif

   /* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"
   
/* Exported macros ------------------------------------------------------------*/
//TODO: we should probably adjust these to a smaller size
#define UART_TX_BUFFER_SIZE (1024)
#define UART_RX_BUFFER_SIZE (64)
   
/* Definition for Usart resources *********************************************/
#if 0
#define BSP_UART_DEBUG                           (USART2)
#define __BSP_UART_DEBUG_CLK_ENABLE()              __USART2_CLK_ENABLE()
#define __BSP_UART_DEBUG_CLK_DISABLE()             __USART2_CLK_DISABLE()
#define __BSP_UART_DEBUG_RX_GPIO_CLK_ENABLE()      __GPIOA_CLK_ENABLE()
#define __BSP_UART_DEBUG_TX_GPIO_CLK_ENABLE()      __GPIOA_CLK_ENABLE()

#define __BSP_UART_DEBUG_FORCE_RESET()             __USART2_FORCE_RESET()
#define __BSP_UART_DEBUG_RELEASE_RESET()           __USART2_RELEASE_RESET()

/* Definition for BSP_UART_DEBUG Pins */
#define BSP_UART_DEBUG_TX_PIN               (GPIO_PIN_2)
#define BSP_UART_DEBUG_TX_PORT              (GPIOA)
#define BSP_UART_DEBUG_RX_PIN               (GPIO_PIN_3)
#define BSP_UART_DEBUG_RX_PORT              (GPIOA)

/* Definition for BSP_UART_DEBUG's NVIC */
#define BSP_UART_DEBUG_IRQn                      (USART2_IRQn)
#define BSP_UART_DEBUG_IRQHandler                USART2_IRQHandler

#define BSP_UART_DEBUG_TX_AF                     (GPIO_AF1_USART2)
#define BSP_UART_DEBUG_RX_AF                     (GPIO_AF1_USART2)
#else
#define BSP_UART_DEBUG                           (USART1)
#define __BSP_UART_DEBUG_CLK_ENABLE()              __USART1_CLK_ENABLE()
#define __BSP_UART_DEBUG_CLK_DISABLE()             __USART1_CLK_DISABLE()
#define __BSP_UART_DEBUG_RX_GPIO_CLK_ENABLE()      __GPIOA_CLK_ENABLE()
#define __BSP_UART_DEBUG_TX_GPIO_CLK_ENABLE()      __GPIOA_CLK_ENABLE()

#define __BSP_UART_DEBUG_FORCE_RESET()             __USART1_FORCE_RESET()
#define __BSP_UART_DEBUG_RELEASE_RESET()           __USART1_RELEASE_RESET()

/* Definition for BSP_UART_DEBUG Pins */
#define BSP_UART_DEBUG_TX_PIN               (GPIO_PIN_9)
#define BSP_UART_DEBUG_TX_PORT              (GPIOA)
#define BSP_UART_DEBUG_RX_PIN               (GPIO_PIN_10)
#define BSP_UART_DEBUG_RX_PORT              (GPIOA)

/* Definition for BSP_UART_DEBUG's NVIC */
#define BSP_UART_DEBUG_IRQn                      (USART1_IRQn)
#define BSP_UART_DEBUG_IRQHandler                USART1_IRQHandler
   
#define BSP_UART_DEBUG_TX_AF                     (GPIO_AF1_USART1)
#define BSP_UART_DEBUG_RX_AF                     (GPIO_AF1_USART1)
#endif
/* Exported types --- --------------------------------------------------------*/
typedef struct BspUartDataTag
{
  volatile uint8_t rxWriteChar;
  uint8_t *pRxBuffer;
  volatile uint8_t *pRxWriteBuffer;
  volatile uint8_t *pRxReadBuffer;
  uint8_t *pTxBuffer;
  volatile uint8_t *pTxWriteBuffer;
  volatile uint8_t *pTxReadBuffer;
  volatile uint8_t *pTxWrap;
  volatile uint8_t newTxRequestInThePipe;
  volatile uint8_t gCodeDataMode;
  volatile uint16_t nbTxBytesOnGoing;
  volatile ITStatus rxBusy;
  volatile ITStatus txBusy;
  void (*uartRxDataCallback)(uint8_t *,uint8_t);  
  void (*uartTxDoneCallback)(void);  
  UART_HandleTypeDef handle;
  uint32_t debugNbRxFrames; 
  uint32_t debugNbTxFrames;
  volatile uint32_t nbBridgedBytes;
}BspUartDataType;

/* Exported variables  --------------------------------------------------------*/
extern BspUartDataType gBspUartData;

/* Exported functions --------------------------------------------------------*/
uint8_t* BSP_UartIfGetFreeTxBuffer(void);
void BSP_UartHwInit(uint32_t newBaudRate);
void BSP_UartIfStart(void);
void BSP_UartIfQueueTxData(uint8_t *pBuf, uint32_t nbData);
void BSP_UartIfSendQueuedData(void);
void BSP_UartAttachRxDataHandler(void (*callback)(uint8_t *, uint8_t));
void BSP_UartAttachTxDoneCallback(void (*callback)(void));
uint32_t BSP_UartPrintf(const char* format,...);
uint32_t BSP_UartGetNbRxAvailableBytes(void);
uint32_t BSP_UartCopyNextRxBytes(uint8_t *buff, uint32_t maxlen);
int8_t BSP_UartGetNextRxBytes(void);
uint8_t BSP_UartIsTxOnGoing(void);
#if defined(MARLIN)
uint32_t BSP_UartCommandsFilter(char *pBufCmd, uint8_t nxRxBytes);
#endif
void BSP_UartLockingTx(uint8_t *pBuf, uint8_t nbData);
#ifdef __cplusplus
}
#endif

#endif /* __NUCLEO_F070RB_3DPRINTER_UART_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
