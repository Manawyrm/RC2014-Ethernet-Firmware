#pragma once

#include <stdint.h>

extern void** _romfunctions;

#define ROM_CF_INIT 0
#define ROM_CF_READ 1
#define ROM_PUTSTRING_UART 2
#define ROM_UART_AVAILABLE 3
#define ROM_UART_READ 4
#define ROM_PUTCHAR_UART 5
#define ROM_PF_OPEN 6
#define ROM_PF_READ 7
#define ROM_PF_OPENDIR 9
#define ROM_PF_READDIR 10
#define ROM_CF_WRITE 11

typedef void (*rom_putstring_uart_f) (const uint8_t* text);   

static inline void rom_putstring_uart(const uint8_t* text)
{
	((rom_putstring_uart_f)_romfunctions[ROM_PUTSTRING_UART])(text);
}

typedef uint8_t (*rom_uart_available_f) ();   

static inline uint8_t rom_uart_available()
{
	return ((rom_uart_available_f)_romfunctions[ROM_UART_AVAILABLE])();
}

typedef uint8_t (*rom_uart_read_f) ();   

static inline uint8_t rom_uart_read()
{
	return ((rom_uart_read_f)_romfunctions[ROM_UART_READ])();
}

typedef void (*rom_putchar_uart_f) (char text);   

static inline void rom_putchar_uart(char text)
{
	((rom_putchar_uart_f)_romfunctions[ROM_PUTCHAR_UART])(text);
}


typedef void (*rom_cf_init_f) ();   

static inline void rom_cf_init()
{
	((rom_cf_init_f)_romfunctions[ROM_CF_INIT])();
}

typedef void (*rom_cf_read_f) (uint32_t sector, uint8_t* data);   

static inline void rom_cf_read(uint32_t sector, uint8_t* data)
{
	((rom_cf_read_f)_romfunctions[ROM_CF_READ])(sector, data);
}

typedef void (*rom_cf_write_f) (uint32_t sector, uint8_t* data);   

static inline void rom_cf_write(uint32_t sector, uint8_t* data)
{
	((rom_cf_write_f)_romfunctions[ROM_CF_WRITE])(sector, data);
}

