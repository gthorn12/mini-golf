
#ifndef OLED_H
#define OLED_H

#include <stdint.h>

void ssd1306_command(uint8_t command);

void ssd1306_data(uint8_t data);

void ssd1306_init(void);

void ssd1306_clear(void);

void ssd1306_set_cursor(uint8_t col, uint8_t page);

void ssd1306_print_char(char c);

void ssd1306_print(const char* str);

#endif // OLED_H