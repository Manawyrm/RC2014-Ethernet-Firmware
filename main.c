#include "main.h"
#include <stdio.h>
#include "romfunctions.h"
#include <z80.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

void print_memory(const void *addr, uint16_t size)
{
    uint16_t printed = 0;
    uint16_t i;
    const unsigned char* pc = addr;
    for (i=0; i<size; ++i)
    {
        int  g;
        g = (*(pc+i) >> 4) & 0xf;
        g += g >= 10 ? 'a'-10 : '0';
        rom_putchar_uart(g);
        printed++;

        g = *(pc+i) & 0xf;
        g += g >= 10 ? 'a'-10 : '0';
        rom_putchar_uart(g);
        printed++;
        if (printed % 32 == 0) rom_putchar_uart('\n');
        else if (printed % 4 == 0) rom_putchar_uart(' ');
    }
}

const uint8_t etherpack[] = { 
	0x00, 0x2b, 0x67, 0x26, 0x31, 0x13, 0x00, 0x80, 0x41, 0x42, 0x23, 0x42, 0x08, 0x00,
	0x45, 0x00, 0x00, 0x54, 0xdd, 0x28, 0x40, 0x00, 0x40, 0x01, 0x3a, 0xa2, 0x0a, 0x04,
	0x0d, 0xd6, 0x0a, 0x04, 0x01, 0x01, 0x08, 0x00, 0xc7, 0x60, 0x00, 0x04, 0x00, 0x02,
	0x97, 0xb5, 0x46, 0x5f, 0x00, 0x00, 0x00, 0x00, 0x8b, 0xb1, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
	0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };  

uint8_t prom[32];
uint8_t data = 0x00;
int i;

struct ne2k_struct {
  uint8_t mac[6];                 // MAC address
  uint16_t iobase;                // Configured I/O base
  uint8_t rx_page_start;          // Start of receive ring
  uint8_t rx_page_stop;           // End of receive ring
  uint8_t next_pkt;               // Next unread received packet*/
};

struct ne2k_struct ne2k;

void ne2k_setup(uint16_t iobase)
{
	ne2k.iobase = iobase;
	ne2k.mac[0] = 0x00;
	ne2k.mac[1] = 0x80;
	ne2k.mac[2] = 0x41;
	ne2k.mac[3] = 0x42;
	ne2k.mac[4] = 0x23;
	ne2k.mac[5] = 0x42;

	ne2k.rx_page_start = (16 * 1024) / NE_PAGE_SIZE;
	ne2k.rx_page_stop = ne2k.rx_page_start + ((16 * 1024) / NE_PAGE_SIZE) - NE_TXBUF_SIZE * NE_TX_BUFERS;
	ne2k.next_pkt = ne2k.rx_page_start + 1;

	rom_putstring_uart("Resetting card...\n");

	z80_outp(ne2k.iobase + 0x1F, z80_inp(ne2k.iobase + 0x1F));  // write the value of RESET into the RESET register
	while ((z80_inp(ne2k.iobase + 0x07) & 0x80) == 0);      // wait for the RESET to complete
	z80_outp(ne2k.iobase + 0x07, 0xFF);                     // mask interrupts

	rom_putstring_uart("Card reset successfully.\n");

	// Set page 0 registers, abort remote DMA, stop NIC
	z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD2 | NE_CR_STP);

	// Set FIFO threshold to 8, no auto-init remote DMA, byte order=80x86, byte-wide DMA transfers
	z80_outp(ne2k.iobase + NE_P0_DCR, NE_DCR_FT1 | NE_DCR_LS);

	// Set page 3 registers (RTL8019 specific)
	z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_PAGE_3 | NE_CR_RD2 | NE_CR_STP);
	z80_outp(ne2k.iobase + NE_P3_9346CR, NE_EEM0 | NE_EEM1);
	z80_outp(ne2k.iobase + NE_P3_CONFIG3, 0x50); // fdx, leds on

	// Set page 0 registers, abort remote DMA, stop NIC
	z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD2 | NE_CR_STP);

	// Clear remote byte count registers
	z80_outp(ne2k.iobase + NE_P0_RBCR0, 0);
	z80_outp(ne2k.iobase + NE_P0_RBCR1, 0);

	// Initialize receiver (ring-buffer) page stop and boundry
	z80_outp(ne2k.iobase + NE_P0_PSTART, ne2k.rx_page_start);
	z80_outp(ne2k.iobase + NE_P0_PSTOP, ne2k.rx_page_stop);
	z80_outp(ne2k.iobase + NE_P0_BNRY, ne2k.rx_page_start);

	// Enable the following interrupts: receive/transmit complete, receive/transmit error, 
	// receiver overwrite and remote dma complete.
	z80_outp(ne2k.iobase + NE_P0_IMR, NE_IMR_PRXE | NE_IMR_PTXE | NE_IMR_RXEE | NE_IMR_TXEE | NE_IMR_OVWE | NE_IMR_RDCE);

	// Set page 1 registers
	z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_PAGE_1 | NE_CR_RD2 | NE_CR_STP);

	// Copy out our station address
	for (i = 0; i < ETHER_ADDR_LEN; i++) z80_outp(ne2k.iobase + NE_P1_PAR0 + i, ne2k.mac[i]);

	// Set current page pointer 
	z80_outp(ne2k.iobase + NE_P1_CURR, ne2k.next_pkt);

	// Initialize multicast address hashing registers to not accept multicasts
	for (i = 0; i < 8; i++) z80_outp(ne2k.iobase + NE_P1_MAR0 + i, 0);

	// Set page 0 registers
	z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD2 | NE_CR_STP);

	// Accept broadcast packets
	z80_outp(ne2k.iobase + NE_P0_RCR, NE_RCR_AB);

	// Take NIC out of loopback
	z80_outp(ne2k.iobase + NE_P0_TCR, 0);

	// Clear any pending interrupts
	z80_outp(ne2k.iobase + NE_P0_ISR, 0xFF);

	// Start NIC
	z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD2 | NE_CR_STA);

	rom_putstring_uart("init done!\n");
}


int ne2k_transmit(uint8_t *packet, uint16_t length)
{
  unsigned short dst;
  uint16_t i; 

  // Set page 0 registers
  z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD2 | NE_CR_STA);

  // Reset remote DMA complete flag
  z80_outp(ne2k.iobase + NE_P0_ISR, NE_ISR_RDC);

  // Set up DMA byte count
  z80_outp(ne2k.iobase + NE_P0_RBCR0, (unsigned char) length);
  z80_outp(ne2k.iobase + NE_P0_RBCR1, (unsigned char) (length >> 8));

  // Set up destination address in NIC memory
  dst = ne2k.rx_page_stop; // for now we only use one tx buffer
  z80_outp(ne2k.iobase + NE_P0_RSAR0, (dst * NE_PAGE_SIZE));
  z80_outp(ne2k.iobase + NE_P0_RSAR1, (dst * NE_PAGE_SIZE) >> 8);

  // Set remote DMA write
  z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD1 | NE_CR_STA);

  for (i = 0; i < length; ++i)
  {
  	z80_outp(ne2k.iobase + NE_NOVELL_DATA, packet[i]);
  }

  // Set TX buffer start page
  z80_outp(ne2k.iobase + NE_P0_TPSR, (unsigned char) dst);

  // Set TX length (packets smaller than 64 bytes must be padded)
  if (length > 64) {
    z80_outp(ne2k.iobase + NE_P0_TBCR0, length);
    z80_outp(ne2k.iobase + NE_P0_TBCR1, length >> 8);
  } else {
    z80_outp(ne2k.iobase + NE_P0_TBCR0, 64);
    z80_outp(ne2k.iobase + NE_P0_TBCR1, 0);
  }

  // Set page 0 registers, transmit packet, and start
  z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD2 | NE_CR_TXP | NE_CR_STA);

  return 0;
}


void main()
{
	rom_putstring_uart("rtl8019 driver!\n");

  	ne2k_setup(0x20);


	while (1)
	{
		ne2k_transmit(etherpack, sizeof(etherpack));
		rom_putstring_uart("transmitted a packet!\n");
		for (uint32_t i = 0; i < 1000; ++i)
		{
			z80_delay_ms(1);
		}
	}
}
