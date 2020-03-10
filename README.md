# afboot-stm32
Fork of https://github.com/afaerber/afboot-stm32


## OpenOCD fix for external flash programming
In order to program the external flash on STM32 boards, the following OpenOCD patch is required.

http://openocd.zylin.com/#/c/4321/

After cloning the patch git repository, follow these steps to build openocd.

Steps:

1- Git clone/checkout

2- ./bootstrap

3- ./configure

4- make

5- sudo make install (systemwide)
