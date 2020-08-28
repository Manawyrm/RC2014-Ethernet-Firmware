#pragma once

#include <stdint.h>
#include <z80.h>
#include "ne2k_const.h"
#include "romfunctions.h"
#include "debug.h"

struct ne2k_struct {
    uint8_t mac[6];                 // MAC address
    uint16_t iobase;                // Configured I/O base
    uint8_t rx_page_start;          // Start of receive ring
    uint8_t rx_page_stop;           // End of receive ring
    uint8_t next_pkt;               // Next unread received packet
    uint16_t rx_ring_start;         // Start address of receive ring
    uint16_t rx_ring_end;           // End address of receive ring
};

struct recv_ring_desc {
    unsigned char rsr;              // Receiver status
    unsigned char next_pkt;         // Pointer to next packet
    unsigned short count;           // Bytes in packet (length + 4)
};

void ne2k_setup(uint16_t iobase);
int ne2k_transmit(uint8_t *packet, uint16_t length);
void ne2k_readmem(uint16_t src, void *dst, uint16_t len);
void ne2k_get_packet(uint16_t src, char *dst, uint16_t len);
void ne2k_receive();