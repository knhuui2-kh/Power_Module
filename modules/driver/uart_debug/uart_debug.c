#include "uart_debug.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static uint32_t UartDebug_Lock(void)
{
#if defined(__arm__) || defined(__arm) || defined(__ARM_ARCH) || defined(__TARGET_ARCH_ARM)
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
#else
    return 0U;   /* 主机语法检查用；目标编译走上面的临界区。 */
#endif
}

static void UartDebug_Unlock(uint32_t primask)
{
#if defined(__arm__) || defined(__arm) || defined(__ARM_ARCH) || defined(__TARGET_ARCH_ARM)
    __set_PRIMASK(primask);
#else
    (void)primask;
#endif
}

static void UartDebug_Kick(UartDebug *dbg)
{
    uint16_t chunk;
    uint16_t linear;
    uint32_t lock;

    if (dbg == NULL)
    {
        return;
    }

    lock = UartDebug_Lock();

    if ((dbg->huart == NULL) || (dbg->tx_buf == NULL)
        || (dbg->tx_size == 0U) || (dbg->tx_busy != 0U)
        || (dbg->tx_count == 0U))
    {
        UartDebug_Unlock(lock);
        return;
    }

    chunk = dbg->tx_count;
    linear = dbg->tx_size - dbg->tx_tail;
    if (chunk > linear)
    {
        chunk = linear;
    }

    if (HAL_UART_Transmit_IT(dbg->huart,
                            &dbg->tx_buf[dbg->tx_tail],
                            chunk) == HAL_OK)
    {
        dbg->tx_busy = 1U;
        dbg->tx_chunk = chunk;
    }

    UartDebug_Unlock(lock);
}

void UartDebug_Init(UartDebug *dbg, UART_HandleTypeDef *huart,
                    uint8_t *storage, uint16_t size)
{
    if (dbg == NULL)
    {
        return;
    }

    dbg->huart = huart;
    dbg->tx_buf = storage;
    dbg->tx_size = (storage != NULL) ? size : 0U;
    dbg->tx_head = 0U;
    dbg->tx_tail = 0U;
    dbg->tx_count = 0U;
    dbg->tx_chunk = 0U;
    dbg->tx_lost = 0U;
    dbg->tx_busy = 0U;
}

uint16_t UartDebug_Write(UartDebug *dbg, const void *data, uint16_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    uint16_t free_space;
    uint16_t i;
    uint32_t lock;

    if ((dbg == NULL) || (src == NULL) || (dbg->tx_buf == NULL)
        || (dbg->tx_size == 0U) || (len == 0U))
    {
        return 0U;
    }

    lock = UartDebug_Lock();
    free_space = (dbg->tx_count < dbg->tx_size) ? (uint16_t)(dbg->tx_size - dbg->tx_count) : 0U;
    if (free_space == 0U)
    {
        dbg->tx_lost += len;
        UartDebug_Unlock(lock);
        return 0U;
    }

    if (len > free_space)
    {
        dbg->tx_lost += (uint32_t)(len - free_space);
        len = free_space;
    }

    for (i = 0U; i < len; i++)
    {
        dbg->tx_buf[dbg->tx_head] = src[i];
        dbg->tx_head++;
        if (dbg->tx_head >= dbg->tx_size)
        {
            dbg->tx_head = 0U;
        }
    }

    dbg->tx_count += len;
    UartDebug_Unlock(lock);

    UartDebug_Kick(dbg);
    return len;
}

uint16_t UartDebug_WriteString(UartDebug *dbg, const char *str)
{
    size_t len;

    if ((dbg == NULL) || (str == NULL))
    {
        return 0U;
    }

    len = strlen(str);
    if (len > 0xFFFFU)
    {
        len = 0xFFFFU;
    }

    return UartDebug_Write(dbg, str, (uint16_t)len);
}

void UartDebug_Printf(UartDebug *dbg, const char *fmt, ...)
{
    char buf[UART_DEBUG_PRINTF_BUF];
    va_list args;
    int len;

    if ((dbg == NULL) || (fmt == NULL))
    {
        return;
    }

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0)
    {
        return;
    }

    if (len >= (int)sizeof(buf))
    {
        len = (int)sizeof(buf) - 1;
    }

    (void)UartDebug_Write(dbg, buf, (uint16_t)len);
}

void UartDebug_TxCpltCallback(UartDebug *dbg)
{
    uint32_t lock;

    if (dbg == NULL)
    {
        return;
    }

    lock = UartDebug_Lock();

    if (dbg->tx_busy == 0U)
    {
        UartDebug_Unlock(lock);
        return;
    }

    dbg->tx_tail += dbg->tx_chunk;
    if (dbg->tx_tail >= dbg->tx_size)
    {
        dbg->tx_tail %= dbg->tx_size;
    }

    if (dbg->tx_count >= dbg->tx_chunk)
    {
        dbg->tx_count -= dbg->tx_chunk;
    }
    else
    {
        dbg->tx_count = 0U;
    }

    dbg->tx_busy = 0U;
    dbg->tx_chunk = 0U;
    UartDebug_Unlock(lock);

    UartDebug_Kick(dbg);
}

void UartDebug_Process(UartDebug *dbg)
{
    UartDebug_Kick(dbg);
}

uint32_t UartDebug_GetLost(const UartDebug *dbg)
{
    return (dbg != NULL) ? dbg->tx_lost : 0U;
}
