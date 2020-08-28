#include "ne2k.h"
//
// NE2000 network driver
// Originally written by Michael Ringgaard
// Modified by Tobias Mädel
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.  
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.  
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
// SUCH DAMAGE.

struct ne2k_struct ne2k;

void ne2k_setup(uint16_t iobase)
{
    uint8_t i = 0; 

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

    ne2k.rx_ring_start = ne2k.rx_page_start * NE_PAGE_SIZE;
    ne2k.rx_ring_end = ne2k.rx_page_stop * NE_PAGE_SIZE;


	rom_putstring_uart("[NE2k] Resetting card...\n");

	z80_outp(ne2k.iobase + 0x1F, z80_inp(ne2k.iobase + 0x1F));  // write the value of RESET into the RESET register
	while ((z80_inp(ne2k.iobase + 0x07) & 0x80) == 0);      // wait for the RESET to complete
	z80_outp(ne2k.iobase + 0x07, 0xFF);                     // mask interrupts

	rom_putstring_uart("[NE2k] Card reset successfully.\n");

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

	rom_putstring_uart("[NE2k] init done!\n");
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

    myprintf("[NE2k] Transmitted packet with length %d\n", length);

    return 0;
}

void ne2k_readmem(uint16_t src, void *dst, uint16_t len)
{
    uint16_t i;

    // Abort any remote DMA already in progress
    z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD2 | NE_CR_STA);

    // Setup DMA byte count
    z80_outp(ne2k.iobase + NE_P0_RBCR0, (uint8_t) len);
    z80_outp(ne2k.iobase + NE_P0_RBCR1, (uint8_t) (len >> 8));

    // Setup NIC memory source address
    z80_outp(ne2k.iobase + NE_P0_RSAR0, (uint8_t) src);
    z80_outp(ne2k.iobase + NE_P0_RSAR1, (uint8_t) (src >> 8));

    // Select remote DMA read
    z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_RD0 | NE_CR_STA);

    // Read NIC memory
    for (i = 0; i < len; i++)
    {
        ((uint8_t*)dst)[i] = z80_inp(ne2k.iobase + NE_NOVELL_DATA);
    }
}

void ne2k_get_packet(uint16_t src, char *dst, uint16_t len)
{
    if (src + len > ne2k.rx_ring_end)
    {
        uint16_t split = ne2k.rx_ring_end - src;

        ne2k_readmem(src, dst, split);
        len -= split;
        src = ne2k.rx_ring_start;
        dst += split;
    }

    ne2k_readmem(src, dst, len);
}


uint8_t packetbuffer[1500];


void ne2k_receive()
{
    struct recv_ring_desc packet_hdr;
    unsigned short packet_ptr;
    unsigned short len;
    unsigned char bndry;
    int rc;

    // Set page 1 registers
    z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_PAGE_1 | NE_CR_RD2 | NE_CR_STA);

    while (ne2k.next_pkt != z80_inp(ne2k.iobase + NE_P1_CURR))
    {
        // Get pointer to buffer header structure
        packet_ptr = ne2k.next_pkt * NE_PAGE_SIZE;

        // Read receive ring descriptor
        ne2k_readmem(packet_ptr, &packet_hdr, sizeof(struct recv_ring_desc));

        // Allocate packet buffer
        len = packet_hdr.count - sizeof(struct recv_ring_desc);
        //p = pbuf_alloc(PBUF_RAW, len, PBUF_RW);

        // Get packet from nic and send to upper layer
        //if (p != NULL)
        //{
            packet_ptr += sizeof(struct recv_ring_desc);
            //for (q = p; q != NULL; q = q->next)
            //{

            ne2k_get_packet(packet_ptr, packetbuffer, len);
            packet_ptr += len;

            //}

            myprintf("[NE2k] received packet, %d bytes\n", len);
            print_memory(packetbuffer, len);
            /*rc = dev_receive(ne->devno, p); 
            if (rc < 0)
            {
            myprintf("[NE2k] error %d processing packet\n", rc);
            pbuf_free(p);
            }*/
        /*}
        else
        {
            // Drop packet
            myprintf("[NE2k] packet dropped\n");
        }*/

        // Update next packet pointer
        ne2k.next_pkt = packet_hdr.next_pkt;

        // Set page 0 registers
        z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_PAGE_0 | NE_CR_RD2 | NE_CR_STA);

        // Update boundry pointer
        bndry = ne2k.next_pkt - 1;
        if (bndry < ne2k.rx_page_start) bndry = ne2k.rx_page_stop - 1;
        z80_outp(ne2k.iobase + NE_P0_BNRY, bndry);

        myprintf("[NE2k] start: %02x stop: %02x next: %02x bndry: %02x\n", ne2k.rx_page_start, ne2k.rx_page_stop, ne2k.next_pkt, bndry);

        // Set page 1 registers
        z80_outp(ne2k.iobase + NE_P0_CR, NE_CR_PAGE_1 | NE_CR_RD2 | NE_CR_STA);
    }
}
