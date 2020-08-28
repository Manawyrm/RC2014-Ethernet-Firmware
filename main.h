#pragma once

#include <stdio.h>
#include "romfunctions.h"
#include <z80.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "ne2k.h"
#include "debug.h"
#include "uip/uip.h"
#include "uip/uip_arp.h"
#include "uip/timer.h"

extern uint32_t ticks;

void main();
