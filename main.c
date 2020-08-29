#include "main.h"

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#ifndef NULL
#define NULL (void *)0
#endif /* NULL */

int i;
uip_ipaddr_t ipaddr;
struct timer periodic_timer, arp_timer;
uint32_t ticks = 0; 

void main()
{
	rom_putstring_uart("rtl8019 driver!\n");
  	ne2k_setup(0x20);

	timer_set(&periodic_timer, CLOCK_SECOND / 2);
	timer_set(&arp_timer, CLOCK_SECOND * 10);

	uip_init();

	uip_ipaddr(ipaddr, 10,0,0,2);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 10,0,0,1);
	uip_setdraddr(ipaddr);
	uip_ipaddr(ipaddr, 255,255,255,0);
	uip_setnetmask(ipaddr);

	hello_world_init();

	while(1)
	{
		uip_len = ne2k_receive();
		if(uip_len > 0)
		{
			if(BUF->type == htons(UIP_ETHTYPE_IP))
			{
				uip_arp_ipin();
				uip_input();
				/* If the above function invocation resulted in data that
					should be sent out on the network, the global variable
					uip_len is set to a value > 0. */
				if(uip_len > 0)
				{
					uip_arp_out();
					ne2k_transmit(uip_buf, uip_len);
				}
			}
			else if(BUF->type == htons(UIP_ETHTYPE_ARP))
			{
				uip_arp_arpin();
				/* If the above function invocation resulted in data that
					should be sent out on the network, the global variable
					uip_len is set to a value > 0. */
				if(uip_len > 0)
				{
					ne2k_transmit(uip_buf, uip_len);
				}
			}
		} 
		else if(timer_expired(&periodic_timer))
		{
			timer_reset(&periodic_timer);
			//myprintf("periodic timer fired!\n");
			for(i = 0; i < UIP_CONNS; i++)
			{
				uip_periodic(i);
				/* If the above function invocation resulted in data that
					should be sent out on the network, the global variable
					uip_len is set to a value > 0. */
				if(uip_len > 0)
				{
					uip_arp_out();
					ne2k_transmit(uip_buf, uip_len);
				}
			}
			
			/* Call the ARP timer function every 10 seconds. */
			if(timer_expired(&arp_timer))
			{
				timer_reset(&arp_timer);
				//myprintf("arp timer fired!\n");
				uip_arp_timer();
			}
		}
		ticks++;

		/*if (ticks % 1000 == 0)
		{
			myprintf("ticks: %lu\n", ticks);
		}*/
	}


	while (1)
	{
		rom_putstring_uart("This should never happen!\n");
		/*ne2k_transmit(uip_buf, uip_len);*/
		/*for (uint32_t i = 0; i < 500; ++i)
		{
			z80_delay_ms(1);
		}

		

		for (uint32_t i = 0; i < 500; ++i)
		{
			z80_delay_ms(1);
		}
		*/
	}
}

void uip_log(char *msg)
{
	rom_putstring_uart(msg);
	rom_putchar_uart('\n');
}