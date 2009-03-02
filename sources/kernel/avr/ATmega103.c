/* uOS interrupt handlers for ATmega103.
 * This file was generated by script 'mkintr.py' from data 'interrupt.cfg'.
 * Do not edit it manually! */

	HANDLE (0, clearb_r30 (0, EIMSK))
	HANDLE (1, clearb_r30 (1, EIMSK))
	HANDLE (2, clearb_r30 (2, EIMSK))
	HANDLE (3, clearb_r30 (3, EIMSK))
	HANDLE (4, clearb_r30 (4, EIMSK))
	HANDLE (5, clearb_r30 (5, EIMSK))
	HANDLE (6, clearb_r30 (6, EIMSK))
	HANDLE (7, clearb_r30 (7, EIMSK))
	HANDLE (8, clearb_r30 (7, TIMSK))
	HANDLE (9, clearb_r30 (6, TIMSK))
	HANDLE (10, clearb_r30 (5, TIMSK))
	HANDLE (11, clearb_r30 (4, TIMSK))
	HANDLE (12, clearb_r30 (3, TIMSK))
	HANDLE (13, clearb_r30 (2, TIMSK))
	HANDLE (14, clearb_r30 (1, TIMSK))
	HANDLE (15, clearb_r30 (0, TIMSK))
	HANDLE (16, clearb_r30 (SPIE, SPCR))
	HANDLE (17, clearb_r30 (RXCIE, UCR))
	HANDLE (18, clearb_r30 (UDRIE, UCR))
	HANDLE (19, clearb_r30 (TXCIE, UCR))
	HANDLE (20, clearb_r30 (ADIE, ADCSR))
	HANDLE (21, clearb_r30 (EERIE, EECR))
	HANDLE (22, clearb_r30 (ACIE, ACSR))

void arch_intr_allow (int irq)
{
	switch ((unsigned char) irq) {
	case 0: setb (0, EIMSK); break;
	case 1: setb (1, EIMSK); break;
	case 2: setb (2, EIMSK); break;
	case 3: setb (3, EIMSK); break;
	case 4: setb (4, EIMSK); break;
	case 5: setb (5, EIMSK); break;
	case 6: setb (6, EIMSK); break;
	case 7: setb (7, EIMSK); break;
	case 8: setb (7, TIMSK); break;
	case 9: setb (6, TIMSK); break;
	case 10: setb (5, TIMSK); break;
	case 11: setb (4, TIMSK); break;
	case 12: setb (3, TIMSK); break;
	case 13: setb (2, TIMSK); break;
	case 14: setb (1, TIMSK); break;
	case 15: setb (0, TIMSK); break;
	case 16: setb (SPIE, SPCR); break;
	case 17: setb (RXCIE, UCR); break;
	case 18: break;
	case 19: setb (TXCIE, UCR); break;
	case 20: setb (ADIE, ADCSR); break;
	case 21: setb (EERIE, EECR); break;
	case 22: setb (ACIE, ACSR); break;
	}
}