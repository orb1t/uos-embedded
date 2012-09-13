/*
 * task_coremark.c
 *
 *  Created on: Apr 1, 2011
 *      Author: krabat
 */

#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include <linpack/linpack.h>

#define SIZEOF_ARRAY(array) (sizeof array)/(sizeof array[0])

ARRAY (stack_console, 6000);                /* Task: console menu */

timer_t timer;

#ifdef ENABLE_DCACHE
unsigned _mempool_start = 0x81000000;
#else
unsigned _mempool_start = 0xa1000000;
#endif

void linpack_run (void *data)
{
	int lp_size = 20;
	for (;;) 
	{    
		debug_printf("linpack task start %ld\n", lp_size);
			main_linpack(lp_size);
		lp_size = lp_size + 250;
		if(lp_size > 2000) lp_size = 500;
	}
}

void uos_init (void)
{
	/* Configure 16 Mbyte of external Flash memory at nCS3. */
	MC_CSCON3 = MC_CSCON_WS (4);		/* Wait states  */

	/* Configure 64 Mbytes of external 32-bit SDRAM memory at nCS0. */
	MC_CSCON0 = MC_CSCON_E |		/* Enable nCS0 */
		MC_CSCON_WS (0) |		/* Wait states  */
		MC_CSCON_T |			/* Sync memory */
		MC_CSCON_CSBA (0x00000000) |	/* Base address */
		MC_CSCON_CSMASK (0xF8000000);	/* Address mask */

	MC_SDRCON = MC_SDRCON_PS_512 |		/* Page size 512 */
		MC_SDRCON_CL_3 |		/* CAS latency 3 cycles */
		MC_SDRCON_RFR (64000000/8192, MPORT_KHZ); /* Refresh period */

	MC_SDRTMR = MC_SDRTMR_TWR(2) |		/* Write recovery delay */
		MC_SDRTMR_TRP(2) |		/* Минимальный период Precharge */
		MC_SDRTMR_TRCD(2) |		/* Между Active и Read/Write */
		MC_SDRTMR_TRAS(5) |		/* Между * Active и Precharge */
		MC_SDRTMR_TRFC(15);		/* Интервал между Refresh */

	MC_SDRCSR = 1;				/* Initialize SDRAM */
        udelay (2);
        
        timer_init (&timer, KHZ, 10);
	task_create (linpack_run, 0, "linpack_run", 10, stack_console, sizeof (stack_console));
}
