RC2014/Z80 RTL8019 webserver
---------------------

![RTL8019 network card](https://screenshot.tbspace.de/okqcvfgylrj.jpg)
![Screenshot of Chrome displaying a webpage hosted on an RC2014](https://screenshot.tbspace.de/ciqyeltprxk.png)

This is a combined HTTP server, TCP/IP stack and NE2000 driver for [RTL8019-based network cards](https://github.com/manawyrm/RC2014-Ethernet) on the RC2014.  

NE2000 driver is based on the [sanos ne2000](http://www.jbox.dk/sanos/source/sys/dev/ne2000.c.html) implementation.  
The network stack is a port of [uIP 1.0](https://en.wikipedia.org/wiki/UIP_(micro_IP)) by Adam Dunkels. It uses [Protothreads](http://dunkels.com/adam/pt/) for simple cooperative multitasking.  

Thanks to [@Toble_Miner](https://twitter.com/TobleMiner) for solving 2 compiler bugs in z88dk! ([first](https://github.com/z88dk/z88dk/issues/1559#issuecomment-683340028), [second](https://github.com/Manawyrm/RC2014-Ethernet-Firmware/commit/353755bb35eb4700d82cba3c9c8cf30b9866c8fd#diff-cc84a5a75c365e3fce21374e82363151))  

Known issues:   
- Only legacy IP is supported for now.
- Protothreads in z88dk currently require a very ugly hack using goto.
- Petit FatFs (in the bootloader ROM) only supports a single open file at a given time.