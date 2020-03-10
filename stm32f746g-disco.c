#include <stdlib.h>
#include <stdint.h>

#include "stm32f4_regs.h"
#include "usart.h"
#include "gpio.h"
#include "mpu.h"
#include "start_kernel.h"
#include "qspi.h"

#define CONFIG_HSE_HZ	25000000
#define CONFIG_PLL_M	25
#define CONFIG_PLL_N	432
#define CONFIG_PLL_P	2
#define CONFIG_PLL_Q	9
#define PLLCLK_HZ (((CONFIG_HSE_HZ / CONFIG_PLL_M) * CONFIG_PLL_N) / CONFIG_PLL_P)
#if PLLCLK_HZ == 216000000
#define FLASH_LATENCY	9
#else
#error PLL clock does not match 216 MHz
#endif

static void *usart_base = (void *)USART1_BASE;
static void *gpio_base = (void *)GPIOA_BASE;

static void usart_putint(uint32_t);
static void kokot(void);
static void testmem(void);

static void clock_setup(void)
{
	volatile uint32_t *RCC_CR = (void *)(RCC_BASE + 0x00);
	volatile uint32_t *RCC_PLLCFGR = (void *)(RCC_BASE + 0x04);
	volatile uint32_t *RCC_CFGR = (void *)(RCC_BASE + 0x08);
	volatile uint32_t *FLASH_ACR = (void *)(FLASH_BASE + 0x00);
	volatile uint32_t *RCC_AHB1ENR = (void *)(RCC_BASE + 0x30);
	volatile uint32_t *RCC_AHB2ENR = (void *)(RCC_BASE + 0x34);
	volatile uint32_t *RCC_AHB3ENR = (void *)(RCC_BASE + 0x38);
	volatile uint32_t *RCC_APB1ENR = (void *)(RCC_BASE + 0x40);
	volatile uint32_t *RCC_APB2ENR = (void *)(RCC_BASE + 0x44);
	volatile uint32_t *RCC_AHB3RST = (void *)(RCC_BASE + 0x18);
	uint32_t val;


	*RCC_CR |= RCC_CR_HSEON;
	while (!(*RCC_CR & RCC_CR_HSERDY)) {
	}

	val = *RCC_CFGR;
	val &= ~RCC_CFGR_HPRE_MASK;
	//val |= 0 << 4; // not divided
	val &= ~RCC_CFGR_PPRE1_MASK;
	val |= 0x5 << 10; // divided by 4
	val &= ~RCC_CFGR_PPRE2_MASK;
	val |= 0x4 << 13; // divided by 2
	*RCC_CFGR = val;

	val = 0;
	val |= RCC_PLLCFGR_PLLSRC_HSE;
	val |= CONFIG_PLL_M;
	val |= CONFIG_PLL_N << 6;
	val |= ((CONFIG_PLL_P >> 1) - 1) << 16;
	val |= CONFIG_PLL_Q << 24;
	*RCC_PLLCFGR = val;

	*RCC_CR |= RCC_CR_PLLON;
	while (!(*RCC_CR & RCC_CR_PLLRDY));

	*FLASH_ACR = FLASH_ACR_ICEN | FLASH_ACR_PRFTEN | FLASH_LATENCY;

	*RCC_CFGR &= ~RCC_CFGR_SW_MASK;
	*RCC_CFGR |= RCC_CFGR_SW_PLL;
	while ((*RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) {
	}

	/*  Enable all clocks, unused ones will be gated at end of kernel boot */
	*RCC_AHB1ENR |= 0x7ef417ff;
	*RCC_AHB2ENR |= 0xf1;
	*RCC_AHB3ENR |= 0x3;
	*RCC_APB1ENR |= 0xf6fec9ff;
	*RCC_APB2ENR |= 0x4777f33;

	/* togle reset QSPI */
	*RCC_AHB3RST |= 0x2;
	*RCC_AHB3RST &= 0xfffffffd;
}



static void fmc_wait_busy(void)
{
	volatile uint32_t *FMC_SDSR = (void *)(FMC_BASE + 0x158);

	while ((*FMC_SDSR & FMC_SDSR_BUSY)) {
	}
}

int main(void)
{
	volatile uint32_t *FLASH_KEYR = (void *)(FLASH_BASE + 0x04);
	volatile uint32_t *FLASH_CR = (void *)(FLASH_BASE + 0x10);
	volatile uint32_t *FMC_SDCR1 = (void *)(FMC_BASE + 0x140);
	volatile uint32_t *FMC_SDTR1 = (void *)(FMC_BASE + 0x148);
	volatile uint32_t *FMC_SDCMR = (void *)(FMC_BASE + 0x150);
	volatile uint32_t *FMC_SDRTR = (void *)(FMC_BASE + 0x154);
	struct qspi_params qspi_746_params = {
		.address_size = QUADSPI_CCR_ADSIZE_24BITS,
		.fifo_threshold = QUADSPI_CR_FTHRES(1),
		.sshift = QUADSPI_CR_SSHIFT,
		.fsize = QUADSPI_DCR_FSIZE_16MB,
		.prescaler = 1,
		.dummy_cycle = 10,
		.fsel = 0,
		.dfm = 0,
	};
	int i;

	mpu_config(0xc0000000);

	if (*FLASH_CR & FLASH_CR_LOCK) {
		*FLASH_KEYR = 0x45670123;
		*FLASH_KEYR = 0xCDEF89AB;
	}
	*FLASH_CR &= ~(FLASH_CR_ERRIE | FLASH_CR_EOPIE | FLASH_CR_PSIZE_MASK);
	*FLASH_CR |= FLASH_CR_PSIZE_X32;
	*FLASH_CR |= FLASH_CR_LOCK;

	clock_setup();

	gpio_set_fmc(gpio_base, 'F', 0);  //A0
	gpio_set_fmc(gpio_base, 'F', 1);  //A1
	gpio_set_fmc(gpio_base, 'F', 2);  //A2
	gpio_set_fmc(gpio_base, 'F', 3);  //A3
	gpio_set_fmc(gpio_base, 'F', 4);  //A4
	gpio_set_fmc(gpio_base, 'F', 5);  //A5
	gpio_set_fmc(gpio_base, 'F', 12); //A6
	gpio_set_fmc(gpio_base, 'F', 13); //A7
	gpio_set_fmc(gpio_base, 'F', 14); //A8
	gpio_set_fmc(gpio_base, 'F', 15); //A9
	gpio_set_fmc(gpio_base, 'G', 0);  //A10
	gpio_set_fmc(gpio_base, 'G', 1);  //A11
	gpio_set_fmc(gpio_base, 'D', 14); //D0
	gpio_set_fmc(gpio_base, 'D', 15); //D1
	gpio_set_fmc(gpio_base, 'D', 0);  //D2
	gpio_set_fmc(gpio_base, 'D', 1);  //D3
	gpio_set_fmc(gpio_base, 'E', 7);  //D4
	gpio_set_fmc(gpio_base, 'E', 8);  //D5
	gpio_set_fmc(gpio_base, 'E', 9);  //D6
	gpio_set_fmc(gpio_base, 'E', 10); //D7
	gpio_set_fmc(gpio_base, 'E', 11); //D8
	gpio_set_fmc(gpio_base, 'E', 12); //D9
	gpio_set_fmc(gpio_base, 'E', 13); //D10
	gpio_set_fmc(gpio_base, 'E', 14); //D11
	gpio_set_fmc(gpio_base, 'E', 15); //D12
	gpio_set_fmc(gpio_base, 'D', 8);  //D13
	gpio_set_fmc(gpio_base, 'D', 9);  //D14
	gpio_set_fmc(gpio_base, 'D', 10); //D15
	gpio_set_fmc(gpio_base, 'H', 5);  //SDNWE
	gpio_set_fmc(gpio_base, 'F', 11); //SDNRAS
	gpio_set_fmc(gpio_base, 'G', 15); //SDNCAS
	gpio_set_fmc(gpio_base, 'G', 8);  //SDCLK
	gpio_set_fmc(gpio_base, 'G', 4);  //BA0
	gpio_set_fmc(gpio_base, 'G', 5);  //BA1
	gpio_set_fmc(gpio_base, 'H', 3);  //SDNE0
	gpio_set_fmc(gpio_base, 'C', 3);  //SDCKE0
	gpio_set_fmc(gpio_base, 'E', 0);  //NBL0
	gpio_set_fmc(gpio_base, 'E', 1);  //NBL1
	*FMC_SDCR1 = 0x00001954;
	*FMC_SDTR1 = 0x01116361;

	fmc_wait_busy();
	*FMC_SDCMR = 0x00000011; // clock
	for (i = 0; i < 60000000; i++) { // 10 ms
		asm volatile ("nop");
	}

	fmc_wait_busy();
	*FMC_SDCMR = 0x00000012; // PALL
	fmc_wait_busy();
	*FMC_SDCMR = 0x000000F3; // auto-refresh
	fmc_wait_busy();
	*FMC_SDCMR = 0x00044014; // external memory mode
	fmc_wait_busy();

	*FMC_SDRTR |= 0x0000050C<<1; // refresh rate
	*FMC_SDCR1 &= 0xFFFFFDFF;

	gpio_set_qspi(gpio_base, 'B', 6, GPIOx_PUPDR_PULLUP,  0xa); //CS
	gpio_set_qspi(gpio_base, 'B', 2, GPIOx_PUPDR_NOPULL,  0x9); //CLK
	gpio_set_qspi(gpio_base, 'D', 11, GPIOx_PUPDR_NOPULL, 0x9); //DO
	gpio_set_qspi(gpio_base, 'D', 12, GPIOx_PUPDR_NOPULL, 0x9); //D1
	gpio_set_qspi(gpio_base, 'E', 2, GPIOx_PUPDR_NOPULL,  0x9); //D2
	gpio_set_qspi(gpio_base, 'D', 13, GPIOx_PUPDR_NOPULL, 0x9); //D3

	quadspi_init(&qspi_746_params, (void *)QUADSPI_BASE);

	gpio_set_usart(gpio_base,'C', 6, 8);
	gpio_set_usart(gpio_base,'C', 7, 8);

	usart_setup(usart_base, 108000000);
	usart_putch(usart_base, '.');
    usart_putint(0x00006666);

    //kokot();
    //testmem();
	start_kernel();

	return 0;
}

#define TO_HEX(i) (i <= 9 ? '0' + i : 'A' - 10 + i)

static void usart_putint(uint32_t x) {
    usart_putch(usart_base, '0');
    usart_putch(usart_base, 'x');
    usart_putch(usart_base, TO_HEX(((x & 0xF0000000) >> 28)));
    usart_putch(usart_base, TO_HEX(((x & 0x0F000000) >> 24)));
    usart_putch(usart_base, TO_HEX(((x & 0x00F00000) >> 20)));
    usart_putch(usart_base, TO_HEX(((x & 0x000F0000) >> 16)));
    usart_putch(usart_base, TO_HEX(((x & 0x0000F000) >> 12)));
    usart_putch(usart_base, TO_HEX(((x & 0x00000F00) >> 8)));
    usart_putch(usart_base, TO_HEX(((x & 0x000000F0) >> 4)));
    usart_putch(usart_base, TO_HEX(((x & 0x0000000F))));
}

static void testmem(void) {
    volatile uint32_t *f = 0x90000000;
    volatile uint32_t *r = 0xc0001000;
    
    *f = 0x13371337;
    *r = 0x06660666;
    usart_putint(*f);
    usart_putint(*r);
}

static void kokot(void) {
    volatile uint32_t *mem = 0;
    uint32_t addr = 0x90003950;

    while (addr <=  0x90004020) {

        mem = (uint32_t *) addr;
        usart_putint(addr);
	    usart_putch(usart_base, ' ');
        usart_putint((*mem));
	    usart_putch(usart_base, '\r');
	    usart_putch(usart_base, '\n');
        addr=addr+4;
	}
}
static void noop(void)
{
	usart_putch(usart_base, 'E');
	while (1) {
	}
}

extern unsigned int _end_text;
extern unsigned int _start_data;
extern unsigned int _end_data;
extern unsigned int _start_bss;
extern unsigned int _end_bss;

void reset(void)
{
	unsigned int *src, *dst;

	asm volatile ("cpsid i");

	src = &_end_text;
	dst = &_start_data;
	while (dst < &_end_data) {
		*dst++ = *src++;
	}

	dst = &_start_bss;
	while (dst < &_end_bss) {
		*dst++ = 0;
	}

	main();
}

extern unsigned long _stack_top;

__attribute__((section(".vector_table")))
void (*vector_table[16 + 91])(void) = {
	(void (*))&_stack_top,
	reset,
	noop,
	noop,
	noop,
	noop,
	noop,
	NULL,
	NULL,
	NULL,
	NULL,
	noop,
	noop,
	NULL,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
};

