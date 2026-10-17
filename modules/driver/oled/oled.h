#ifndef POWER_OLED_H
#define POWER_OLED_H

#include "stm32g4xx_hal.h"

/*
 * SSD1306 / SSD1315 128x64 OLED，I2C 接口。
 * 坐标按页组织：page 0~7，一页 8 像素高；8x16 字库一个字符占两页，
 * 所以写字时 page 只能取 0~6，x 为像素列（0~127，步进 8）。
 *
 * 本模块不做显存缓存，每个字符直接按列写屏；改动显示内容时才产生 I2C 流量。
 */
#define OLED_WIDTH          128U
#define OLED_HEIGHT         64U
/* SSD1306/SSD1315 常见从机地址，HAL 使用左移后的 8 位形式 */
#define OLED_ADDR_DEFAULT   0x78U
#define OLED_I2C_TIMEOUT    100U

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t address;
} Oled;

void Oled_Init(Oled *oled, I2C_HandleTypeDef *hi2c, uint16_t address);
void Oled_Clear(Oled *oled);
void Oled_ShowChar(Oled *oled, uint8_t x, uint8_t page, char ch);
void Oled_ShowString(Oled *oled, uint8_t x, uint8_t page, const char *str);
void Oled_ShowUInt(Oled *oled, uint8_t x, uint8_t page, uint32_t num, uint8_t width);
void Oled_ShowFloat(Oled *oled, uint8_t x, uint8_t page, float num, uint8_t width, uint8_t decimals);

#endif
