spi.c
-----

this sample is for an Atmel ATTiny84 (AVR uC)
test spi exchange with nRFL24l01+

to compile you need AVR-GCC toolchain for Linux.

install command for "debian like" GNU/Linux :
  sudo apt-get install gcc-avr binutils-avr avr-libc avrdude

* makefile :
to compile it                          : make
to read program size                   : make size
produce full disassembly file (.disasm): make disasm
to upload                              : make upload
to clean directory                     : make clean
read uC fuses                          : make rfuses
write uC fuses                         : make wfuses

web site : http://source.perl.free.fr
