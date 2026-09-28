#ifndef POWER_UART_DEBUG_H
#define POWER_UART_DEBUG_H

#include "stm32g4xx_hal.h"

#include <stdint.h>

#define UART_DEBUG_PRINTF_BUF 128U

/* 非阻塞串口调试：环形缓冲 + HAL_UART_Transmit_IT。 */
typedef struct
{
    UART_HandleTypeDef *huart;
    uint8_t *tx_buf;
    uint16_t tx_size;
    volatile uint16_t tx_head;
    volatile uint16_t tx_tail;
    volatile uint16_t tx_count;
    volatile uint16_t tx_chunk;
    volatile uint32_t tx_lost;
    volatile uint8_t tx_busy;
} UartDebug;

void UartDebug_Init(UartDebug *dbg, UART_HandleTypeDef *huart,
                    uint8_t *storage, uint16_t size);
uint16_t UartDebug_Write(UartDebug *dbg, const void *data, uint16_t len);
uint16_t UartDebug_WriteString(UartDebug *dbg, const char *str);
void UartDebug_Printf(UartDebug *dbg, const char *fmt, ...);
void UartDebug_TxCpltCallback(UartDebug *dbg);
void UartDebug_Process(UartDebug *dbg);
uint32_t UartDebug_GetLost(const UartDebug *dbg);

#endif
