#pragma once

#include "romfunctions.h"
#include <stdarg.h>
#include <stdio.h>

void myprintf( const char * format, ... );
void print_memory(const void *addr, uint16_t size);