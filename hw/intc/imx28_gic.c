/*
 * imx28_gic.c
 *
 *  Created on: 25.06.2014
 *      Author: Schoenfelder
 */

#include "hw/intc/imx28_gic.h"

#define FIQ_ENABLE_MASK		(0x20000)
#define IRQ_ENABLE_MASK		(0x10000)
#define VECTOR_PITCH_SHIFT	(21)
#define VECTOR_PITCH_MASK	(0x7)

static const VMStateDescription vmstate_imx28_gic = {
    .name = "imx28_gic",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(hw_icoll_regs, IMX28GICState, NUM_HW_ICOLL_REGS),
        VMSTATE_UINT64(pending_low, IMX28GICState),
        VMSTATE_UINT64(pending_high, IMX28GICState),
        VMSTATE_END_OF_LIST()
    },
};

static void imx28_gic_set_irq(void *opaque, int irq, int level)
{
	//printf("\nset irq %x; %x\n", irq, level);
	IMX28GICState *s = (IMX28GICState *)opaque;
	uint32_t fiq_enable = 0, irq_enable = 0;
	uint32_t is_fiq = 0, is_enabled = 0, is_soft_irq = 0;
	uint32_t irq_reg_index = HW_ICOLL_INTERRUPT0 + irq;
	uint32_t irq_priority = 0;
	uint32_t vector_pitch = 0;

	fiq_enable = s->hw_icoll_regs[HW_ICOLL_CTRL] & FIQ_ENABLE_MASK;
	irq_enable = s->hw_icoll_regs[HW_ICOLL_CTRL] & IRQ_ENABLE_MASK;
	vector_pitch = (s->hw_icoll_regs[HW_ICOLL_CTRL] >> VECTOR_PITCH_SHIFT) & VECTOR_PITCH_MASK;
	is_fiq = s->hw_icoll_regs[irq_reg_index] & 0x10;
	is_enabled = s->hw_icoll_regs[irq_reg_index] & 0x4;
	is_soft_irq = s->hw_icoll_regs[irq_reg_index] & 0x8;
	is_enabled |= is_soft_irq; /* IRQ triggered by software or by hardware */
	irq_priority = s->hw_icoll_regs[irq_reg_index] & 0x3;

	//printf("\nIRQ: %x;%x;%x;%x;%x;%x\n",fiq_enable, irq_enable,is_fiq,is_enabled,is_soft_irq,irq_priority);

	if (fiq_enable && is_fiq && is_enabled) /* FIQ */
	{
	    qemu_set_irq(s->fiq, level);
	    return; /* FIQ has no priorities */
	}
	else if (!is_fiq && irq_enable && is_enabled) /* IRQ */
	{
		qemu_set_irq(s->irq, level);
	}
	else
	{
        qemu_log_mask(LOG_GUEST_ERROR, "Interrupt disabled %x;%x %x %x\n",fiq_enable,irq_enable, is_enabled, is_fiq);
        return;
	}

	if (level)
	{
		//printf("\nICOLL_VECTOR %08x;%08x;%08x;%08x\n", s->hw_icoll_regs[HW_ICOLL_VECTOR] , s->hw_icoll_regs[HW_ICOLL_VBASE], vector_pitch, irq);
		/* Set IRQ vector address of current  interrupt*/

		if (vector_pitch)
			s->hw_icoll_regs[HW_ICOLL_VECTOR] = s->hw_icoll_regs[HW_ICOLL_VBASE] + 4* vector_pitch * irq;
		else
			s->hw_icoll_regs[HW_ICOLL_VECTOR] = s->hw_icoll_regs[HW_ICOLL_VBASE] + 4* irq;

		/* Set vector number of current  interrupt to status register */
		s->hw_icoll_regs[HW_ICOLL_STAT] = irq;
		/* Ack of completion of interrupt */
		s->hw_icoll_regs[HW_ICOLL_LEVELACK] = 0x0;
	}
	else
	{
		/* Set IRQ vector address of current  interrupt*/
		s->hw_icoll_regs[HW_ICOLL_VECTOR] = 0x0;
		/* Set vector number of current  interrupt to status register */
		s->hw_icoll_regs[HW_ICOLL_STAT] = 0x7F;
		/* Ack of completion of interrupt */
		s->hw_icoll_regs[HW_ICOLL_LEVELACK] = (1 << irq_priority);
	}

}


static uint64_t imx28_gic_read(void *opaque,
                             hwaddr offset, unsigned size)
{
	IMX28GICState *s = (IMX28GICState *)opaque;

    //printf("imx28_gic_read (0x%08x)\n",
     //       (unsigned int)offset);

    switch (offset)
    {
    	case 0x0000: /* Interrupt Collector Interrupt Vector Address Register */
    		return s->hw_icoll_regs[HW_ICOLL_VECTOR];
    	case 0x0010: /* Interrupt Collector Level Acknowledge Register */
    		return s->hw_icoll_regs[HW_ICOLL_LEVELACK];
    	case 0x0020: /* Interrupt Collector Control Register */
    		//printf("\nHW_ICOLL_CTRL read%08x\n",s->hw_icoll_regs[HW_ICOLL_CTRL]);
    		return s->hw_icoll_regs[HW_ICOLL_CTRL];
    	case 0x0040: /* Interrupt Collector Interrupt Vector Base Address Register */
    		return s->hw_icoll_regs[HW_ICOLL_VBASE];
    	case 0x0070: /* Interrupt Collector Status Register */
    		return s->hw_icoll_regs[HW_ICOLL_STAT];
    	case 0x00A0: /* Interrupt Collector Raw Interrupt Input Register 0 */
    		return s->hw_icoll_regs[HW_ICOLL_RAW0];
    	case 0x00B0: /* Interrupt Collector Raw Interrupt Input Register 1 */
    		return s->hw_icoll_regs[HW_ICOLL_RAW1];
    	case 0x00C0: /* Interrupt Collector Raw Interrupt Input Register 2 */
    		return s->hw_icoll_regs[HW_ICOLL_RAW2];
    	case 0x00D0: /* Interrupt Collector Raw Interrupt Input Register 3 */
    		return s->hw_icoll_regs[HW_ICOLL_RAW3];
    	case 0x0120: /* Interrupt Collector Interrupt Register 0 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT0];
    	case 0x0130: /* Interrupt Collector Interrupt Register 1 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT1];
    	case 0x0140: /* Interrupt Collector Interrupt Register 2 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT2];
    	case 0x0150: /* Interrupt Collector Interrupt Register 3 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT3];
    	case 0x0160: /* Interrupt Collector Interrupt Register 4 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT4];
    	case 0x0170: /* Interrupt Collector Interrupt Register 5 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT5];
    	case 0x0180: /* Interrupt Collector Interrupt Register 6 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT6];
    	case 0x0190: /* Interrupt Collector Interrupt Register 7 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT7];
    	case 0x01A0: /* Interrupt Collector Interrupt Register 8 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT8];
    	case 0x01B0: /* Interrupt Collector Interrupt Register 9 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT9];
    	case 0x01C0: /* Interrupt Collector Interrupt Register 10 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT10];
    	case 0x01D0: /* Interrupt Collector Interrupt Register 11 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT11];
    	case 0x01E0: /* Interrupt Collector Interrupt Register 12 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT12];
    	case 0x01F0: /* Interrupt Collector Interrupt Register 13 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT13];
    	case 0x0200: /* Interrupt Collector Interrupt Register 14 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT14];
    	case 0x0210: /* Interrupt Collector Interrupt Register 15 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT15];
    	case 0x0220: /* Interrupt Collector Interrupt Register 16 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT16];
    	case 0x0230: /* Interrupt Collector Interrupt Register 17 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT17];
    	case 0x0240: /* Interrupt Collector Interrupt Register 18 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT18];
    	case 0x0250: /* Interrupt Collector Interrupt Register 19 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT19];
    	case 0x0260: /* Interrupt Collector Interrupt Register 20 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT20];
    	case 0x0270: /* Interrupt Collector Interrupt Register 21 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT21];
    	case 0x0280: /* Interrupt Collector Interrupt Register 22 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT22];
    	case 0x0290: /* Interrupt Collector Interrupt Register 23 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT23];
    	case 0x02A0: /* Interrupt Collector Interrupt Register 24 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT24];
    	case 0x02B0: /* Interrupt Collector Interrupt Register 25 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT25];
    	case 0x02C0: /* Interrupt Collector Interrupt Register 26 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT26];
    	case 0x02D0: /* Interrupt Collector Interrupt Register 27 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT27];
    	case 0x02E0: /* Interrupt Collector Interrupt Register 28 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT28];
    	case 0x02F0: /* Interrupt Collector Interrupt Register 29 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT29];
    	case 0x0300: /* Interrupt Collector Interrupt Register 30 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT30];
    	case 0x0310: /* Interrupt Collector Interrupt Register 31 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT31];
    	case 0x0320: /* Interrupt Collector Interrupt Register 32 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT32];
    	case 0x0330: /* Interrupt Collector Interrupt Register 33 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT33];
    	case 0x0340: /* Interrupt Collector Interrupt Register 34 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT34];
    	case 0x0350: /* Interrupt Collector Interrupt Register 35 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT35];
    	case 0x0360: /* Interrupt Collector Interrupt Register 36 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT36];
    	case 0x0370: /* Interrupt Collector Interrupt Register 37 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT37];
    	case 0x0380: /* Interrupt Collector Interrupt Register 38 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT38];
    	case 0x0390: /* Interrupt Collector Interrupt Register 39 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT39];
    	case 0x03A0: /* Interrupt Collector Interrupt Register 40 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT40];
    	case 0x03B0: /* Interrupt Collector Interrupt Register 41 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT41];
    	case 0x03C0: /* Interrupt Collector Interrupt Register 42 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT42];
    	case 0x03D0: /* Interrupt Collector Interrupt Register 43 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT43];
    	case 0x03E0: /* Interrupt Collector Interrupt Register 44 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT44];
    	case 0x03F0: /* Interrupt Collector Interrupt Register 45 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT45];
    	case 0x0400: /* Interrupt Collector Interrupt Register 46 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT46];
    	case 0x0410: /* Interrupt Collector Interrupt Register 47 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT47];
    	case 0x0420: /* Interrupt Collector Interrupt Register 48 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT48];
    	case 0x0430: /* Interrupt Collector Interrupt Register 49 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT49];
    	case 0x0440: /* Interrupt Collector Interrupt Register 50 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT50];
    	case 0x0450: /* Interrupt Collector Interrupt Register 51 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT51];
    	case 0x0460: /* Interrupt Collector Interrupt Register 52 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT52];
    	case 0x0470: /* Interrupt Collector Interrupt Register 53 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT53];
    	case 0x0480: /* Interrupt Collector Interrupt Register 54 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT54];
    	case 0x0490: /* Interrupt Collector Interrupt Register 55 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT55];
    	case 0x04A0: /* Interrupt Collector Interrupt Register 56 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT56];
    	case 0x04B0: /* Interrupt Collector Interrupt Register 57 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT57];
    	case 0x04C0: /* Interrupt Collector Interrupt Register 58 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT58];
    	case 0x04D0: /* Interrupt Collector Interrupt Register 59 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT59];
    	case 0x04E0: /* Interrupt Collector Interrupt Register 60 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT60];
    	case 0x04F0: /* Interrupt Collector Interrupt Register 61 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT61];
    	case 0x0500: /* Interrupt Collector Interrupt Register 62 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT62];
    	case 0x0510: /* Interrupt Collector Interrupt Register 63 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT63];
    	case 0x0520: /* Interrupt Collector Interrupt Register 64 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT64];
    	case 0x0530: /* Interrupt Collector Interrupt Register 65 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT65];
    	case 0x0540: /* Interrupt Collector Interrupt Register 66 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT66];
    	case 0x0550: /* Interrupt Collector Interrupt Register 67 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT67];
    	case 0x0560: /* Interrupt Collector Interrupt Register 68 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT68];
    	case 0x0570: /* Interrupt Collector Interrupt Register 69 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT69];
    	case 0x0580: /* Interrupt Collector Interrupt Register 70 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT70];
    	case 0x0590: /* Interrupt Collector Interrupt Register 71 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT71];
    	case 0x05A0: /* Interrupt Collector Interrupt Register 72 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT72];
    	case 0x05B0: /* Interrupt Collector Interrupt Register 73 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT73];
    	case 0x05C0: /* Interrupt Collector Interrupt Register 74 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT74];
    	case 0x05D0: /* Interrupt Collector Interrupt Register 75 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT75];
    	case 0x05E0: /* Interrupt Collector Interrupt Register 76 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT76];
    	case 0x05F0: /* Interrupt Collector Interrupt Register 77 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT77];
    	case 0x0600: /* Interrupt Collector Interrupt Register 78 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT78];
    	case 0x0610: /* Interrupt Collector Interrupt Register 79 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT79];
    	case 0x0620: /* Interrupt Collector Interrupt Register 80 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT80];
    	case 0x0630: /* Interrupt Collector Interrupt Register 81 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT81];
    	case 0x0640: /* Interrupt Collector Interrupt Register 82 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT82];
    	case 0x0650: /* Interrupt Collector Interrupt Register 83 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT83];
    	case 0x0660: /* Interrupt Collector Interrupt Register 84 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT84];
    	case 0x0670: /* Interrupt Collector Interrupt Register 85 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT85];
    	case 0x0680: /* Interrupt Collector Interrupt Register 86 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT86];
    	case 0x0690: /* Interrupt Collector Interrupt Register 87 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT87];
    	case 0x06A0: /* Interrupt Collector Interrupt Register 88 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT88];
    	case 0x06B0: /* Interrupt Collector Interrupt Register 89 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT89];
    	case 0x06C0: /* Interrupt Collector Interrupt Register 90 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT90];
    	case 0x06D0: /* Interrupt Collector Interrupt Register 91 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT91];
    	case 0x06E0: /* Interrupt Collector Interrupt Register 92 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT92];
    	case 0x06F0: /* Interrupt Collector Interrupt Register 93 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT93];
    	case 0x0700: /* Interrupt Collector Interrupt Register 94 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT94];
    	case 0x0710: /* Interrupt Collector Interrupt Register 95 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT95];
    	case 0x0720: /* Interrupt Collector Interrupt Register 96 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT96];
    	case 0x0730: /* Interrupt Collector Interrupt Register 97 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT97];
    	case 0x0740: /* Interrupt Collector Interrupt Register 98 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT98];
    	case 0x0750: /* Interrupt Collector Interrupt Register 99 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT99];
    	case 0x0760: /* Interrupt Collector Interrupt Register 100 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT100];
    	case 0x0770: /* Interrupt Collector Interrupt Register 101 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT101];
    	case 0x0780: /* Interrupt Collector Interrupt Register 102 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT102];
    	case 0x0790: /* Interrupt Collector Interrupt Register 103 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT103];
    	case 0x07A0: /* Interrupt Collector Interrupt Register 104 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT104];
    	case 0x07B0: /* Interrupt Collector Interrupt Register 105 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT105];
    	case 0x07C0: /* Interrupt Collector Interrupt Register 106 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT106];
    	case 0x07D0: /* Interrupt Collector Interrupt Register 107 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT107];
    	case 0x07E0: /* Interrupt Collector Interrupt Register 108 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT108];
    	case 0x07F0: /* Interrupt Collector Interrupt Register 109 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT109];
    	case 0x0800: /* Interrupt Collector Interrupt Register 110 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT110];
    	case 0x0810: /* Interrupt Collector Interrupt Register 111 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT111];
    	case 0x0820: /* Interrupt Collector Interrupt Register 112 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT112];
    	case 0x0830: /* Interrupt Collector Interrupt Register 113 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT113];
    	case 0x0840: /* Interrupt Collector Interrupt Register 114 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT114];
    	case 0x0850: /* Interrupt Collector Interrupt Register 115 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT115];
    	case 0x0860: /* Interrupt Collector Interrupt Register 116 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT116];
    	case 0x0870: /* Interrupt Collector Interrupt Register 117 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT117];
    	case 0x0880: /* Interrupt Collector Interrupt Register 118 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT118];
    	case 0x0890: /* Interrupt Collector Interrupt Register 119 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT119];
    	case 0x08A0: /* Interrupt Collector Interrupt Register 120 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT120];
    	case 0x08B0: /* Interrupt Collector Interrupt Register 121 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT121];
    	case 0x08C0: /* Interrupt Collector Interrupt Register 122 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT122];
    	case 0x08D0: /* Interrupt Collector Interrupt Register 123 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT123];
    	case 0x08E0: /* Interrupt Collector Interrupt Register 124 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT124];
    	case 0x08F0: /* Interrupt Collector Interrupt Register 125 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT125];
    	case 0x0900: /* Interrupt Collector Interrupt Register 126 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT126];
    	case 0x0910: /* Interrupt Collector Interrupt Register 127 */
    		return s->hw_icoll_regs[HW_ICOLL_INTERRUPT127];
    	case 0x1120: /* Interrupt Collector Debug Register */
    		return s->hw_icoll_regs[HW_ICOLL_DEBUG];
    	case 0x1130: /* Interrupt Collector Debug Read Register 0 */
    		return s->hw_icoll_regs[HW_ICOLL_DBGREAD0];
    	case 0x1140: /* Interrupt Collector Debug Read Register 1 */
    		return s->hw_icoll_regs[HW_ICOLL_DBGREAD1];
    	case 0x1150: /* Interrupt Collector Debug Flag Register */
    		return s->hw_icoll_regs[HW_ICOLL_DBGFLAG];
    	case 0x1160: /* Interrupt Collector Debug Read Request Register 0 */
    		return s->hw_icoll_regs[HW_ICOLL_DBGREQUEST0];
    	case 0x1170: /* Interrupt Collector Debug Read Request Register 1 */
    		return s->hw_icoll_regs[HW_ICOLL_DBGREQUEST1];
    	case 0x1180: /* Interrupt Collector Debug Read Request Register 2 */
    		return s->hw_icoll_regs[HW_ICOLL_DBGREQUEST2];
    	case 0x1190: /* Interrupt Collector Debug Read Request Register 3 */
    		return s->hw_icoll_regs[HW_ICOLL_DBGREQUEST3];
    	case 0x11E0: /* Interrupt Collector Version Register */
    		return s->hw_icoll_regs[HW_ICOLL_VERSION];
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "imx28_gic_read: Bad offset 0x%x\n",
                          (int)offset);
            return 0;
    }
}

static void imx28_gic_write(void *opaque, hwaddr offset,
                          uint64_t val, unsigned size)
{
	IMX28GICState *s = (IMX28GICState *)opaque;

	int i = 0;

//    printf("imx28_gic_write (0x%08x) = %08x\n",
//             (unsigned int)offset, (unsigned int)val);

    switch (offset)
    {
		case 0x0000: /* Interrupt Collector Interrupt Vector Address Register - Set */
		s->hw_icoll_regs[HW_ICOLL_VECTOR] = val;
		break;
    	case 0x0004: /* Interrupt Collector Interrupt Vector Address Register - Set */
    		s->hw_icoll_regs[HW_ICOLL_VECTOR] |= val;
    		break;
    	case 0x0008: /* Interrupt Collector Interrupt Vector Address Register - Reset */
    		s->hw_icoll_regs[HW_ICOLL_VECTOR] &= ~val;
    		break;
    	case 0x000C: /* Interrupt Collector Interrupt Vector Address Register - Toggle */
    		s->hw_icoll_regs[HW_ICOLL_VECTOR] ^= val;
    		break;
    	case 0x0010: /* Interrupt Collector Level Acknowledge Register - RW */
    		s->hw_icoll_regs[HW_ICOLL_LEVELACK] |= val;
    		break;
    	case 0x0024: /* Interrupt Collector Control Register - Set*/
    		if (val & 0x80000000) /* Soft reset */
    		{
    			/* set default values */
    			//printf("\nSoft-Reset\n");
    			for (i = 0; i < NUM_HW_ICOLL_REGS; i++)
    				s->hw_icoll_regs[i] = 0x0;

    			s->hw_icoll_regs[HW_ICOLL_CTRL] |= 0xC0030000;
    			s->hw_icoll_regs[HW_ICOLL_STAT] |= 0xECA94567;
    			s->hw_icoll_regs[HW_ICOLL_DBGREAD0] |= 0x1356DA98;
    			s->hw_icoll_regs[HW_ICOLL_VERSION] |= 0x03010000;
    		}

			s->hw_icoll_regs[HW_ICOLL_CTRL] |= val;
			break;
    	case 0x0028: /* Interrupt Collector Control Register - Reset*/
    		s->hw_icoll_regs[HW_ICOLL_CTRL] &= ~val;
    		break;
    	case 0x002C: /* Interrupt Collector Control Register - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_CTRL] ^= val;
			break;
    	case 0x0044: /* Interrupt Collector Interrupt Vector Base Address Register - Set */
			s->hw_icoll_regs[HW_ICOLL_VBASE] |= val;
			break;
    	case 0x0048: /* Interrupt Collector Interrupt Vector Base Address Register - Reset*/
    		s->hw_icoll_regs[HW_ICOLL_CTRL] &= ~val;
    		break;
    	case 0x004C: /* Interrupt Collector Interrupt Vector Base Address Register - Toggle*/
    		s->hw_icoll_regs[HW_ICOLL_VBASE] ^= val;
    		break;
    	case 0x0070: /* Interrupt Collector Status Register */
    	case 0x0074: /* Interrupt Collector Status Register */
    	case 0x0078: /* Interrupt Collector Status Register */
    	case 0x007C: /* Interrupt Collector Status Register */
    		/*read only */
    		return;
    	case 0x00A0: /* Interrupt Collector Raw Interrupt Input Register 0 */
    	case 0x00A4: /* Interrupt Collector Raw Interrupt Input Register 0 */
    	case 0x00A8: /* Interrupt Collector Raw Interrupt Input Register 0 */
    	case 0x00AC: /* Interrupt Collector Raw Interrupt Input Register 0 */
    		/*read only */
    		return;
    	case 0x00B0: /* Interrupt Collector Raw Interrupt Input Register 1 */
    	case 0x00B4: /* Interrupt Collector Raw Interrupt Input Register 1 */
    	case 0x00B8: /* Interrupt Collector Raw Interrupt Input Register 1 */
    	case 0x00BC: /* Interrupt Collector Raw Interrupt Input Register 1 */
    		/*read only */
    		return;
    	case 0x00C0: /* Interrupt Collector Raw Interrupt Input Register 2 */
    	case 0x00C4: /* Interrupt Collector Raw Interrupt Input Register 2 */
    	case 0x00C8: /* Interrupt Collector Raw Interrupt Input Register 2 */
    	case 0x00CC: /* Interrupt Collector Raw Interrupt Input Register 2 */
    		/*read only */
    		return;
    	case 0x00D0: /* Interrupt Collector Raw Interrupt Input Register 3 */
    	case 0x00D4: /* Interrupt Collector Raw Interrupt Input Register 3 */
    	case 0x00D8: /* Interrupt Collector Raw Interrupt Input Register 3 */
    	case 0x00DC: /* Interrupt Collector Raw Interrupt Input Register 3 */
    		/*read only */
    		return;
    	case 0x0124: /* Interrupt Collector Interrupt Register 0 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT0] |= val;
			break;
    	case 0x0128: /* Interrupt Collector Interrupt Register 0 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT0] &= ~val;
			break;
    	case 0x012C: /* Interrupt Collector Interrupt Register 0 - Toggle*/
     		s->hw_icoll_regs[HW_ICOLL_INTERRUPT0] ^= val;
			break;
    	case 0x0134: /* Interrupt Collector Interrupt Register 1 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT1] |= val;
			break;
    	case 0x0138: /* Interrupt Collector Interrupt Register 1 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT1] &= ~val;
			break;
    	case 0x013C: /* Interrupt Collector Interrupt Register 1 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT1] ^= val;
			break;
    	case 0x0144: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT2] |= val;
			break;
    	case 0x0148: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT2] &= ~val;
			break;
    	case 0x014C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT2] ^= val;
			break;
    	case 0x0154: /* Interrupt Collector Interrupt Register 3 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT3] |= val;
			break;
    	case 0x0158: /* Interrupt Collector Interrupt Register 3 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT3] &= ~val;
			break;
    	case 0x015C: /* Interrupt Collector Interrupt Register 3 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT3] ^= val;
			break;
    	case 0x0164: /* Interrupt Collector Interrupt Register 4 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT4] |= val;
			break;
    	case 0x0168: /* Interrupt Collector Interrupt Register 4 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT4] &= ~val;
			break;
    	case 0x016C: /* Interrupt Collector Interrupt Register 4 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT4] ^= val;
			break;
    	case 0x0174: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT5] |= val;
			break;
    	case 0x0178: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT5] &= ~val;
			break;
    	case 0x017C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT5] ^= val;
			break;
    	case 0x0184: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT6] |= val;
			break;
    	case 0x0188: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT6] &= ~val;
			break;
    	case 0x018C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT6] ^= val;
			break;
    	case 0x0194: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT7] |= val;
			break;
    	case 0x0198: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT7] &= ~val;
			break;
    	case 0x019C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT7] ^= val;
			break;
    	case 0x01A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT8] |= val;
			break;
    	case 0x01A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT8] &= ~val;
			break;
    	case 0x01AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT8] ^= val;
			break;
    	case 0x01B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT9] |= val;
			break;
    	case 0x01B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT9] &= ~val;
			break;
    	case 0x01BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT9] ^= val;
			break;
    	case 0x01C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT10] |= val;
			break;
    	case 0x01C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT10] &= ~val;
			break;
    	case 0x01CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT10] ^= val;
			break;
    	case 0x01D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT11] |= val;
			break;
    	case 0x01D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT11] &= ~val;
			break;
    	case 0x01DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT11] ^= val;
			break;
    	case 0x01E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT12] |= val;
			break;
    	case 0x01E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT12] &= ~val;
			break;
    	case 0x01EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT12] ^= val;
			break;
    	case 0x01F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT13] |= val;
			break;
    	case 0x01F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT13] &= ~val;
			break;
    	case 0x01FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT13] ^= val;
			break;
    	case 0x0204: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT14] |= val;
			break;
    	case 0x0208: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT14] &= ~val;
			break;
    	case 0x020C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT14] ^= val;
			break;
    	case 0x0214: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT15] |= val;
			break;
    	case 0x0218: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT15] &= ~val;
			break;
    	case 0x021C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT15] ^= val;
			break;
    	case 0x0224: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT16] |= val;
			break;
    	case 0x0228: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT16] &= ~val;
			break;
    	case 0x022C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT16] ^= val;
			break;
    	case 0x0234: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT17] |= val;
			break;
    	case 0x0238: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT17] &= ~val;
			break;
    	case 0x023C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT17] ^= val;
			break;
    	case 0x0244: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT18] |= val;
			break;
    	case 0x0248: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT18] &= ~val;
			break;
    	case 0x024C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT18] ^= val;
			break;
    	case 0x0254: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT19] |= val;
			break;
    	case 0x0258: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT19] &= ~val;
			break;
    	case 0x025C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT19] ^= val;
			break;
    	case 0x0264: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT20] |= val;
			break;
    	case 0x0268: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT20] &= ~val;
			break;
    	case 0x026C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT20] ^= val;
			break;
    	case 0x0274: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT21] |= val;
			break;
    	case 0x0278: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT21] &= ~val;
			break;
    	case 0x027C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT21] ^= val;
			break;
    	case 0x0284: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT22] |= val;
			break;
    	case 0x0288: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT22] &= ~val;
			break;
    	case 0x028C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT22] ^= val;
			break;
    	case 0x0294: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT23] |= val;
			break;
    	case 0x0298: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT23] &= ~val;
			break;
    	case 0x029C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT23] ^= val;
			break;
    	case 0x02A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT24] |= val;
			break;
    	case 0x02A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT24] &= ~val;
			break;
    	case 0x02AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT24] ^= val;
			break;
    	case 0x02B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT25] |= val;
			break;
    	case 0x02B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT25] &= ~val;
			break;
    	case 0x02BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT25] ^= val;
			break;
    	case 0x02C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT26] |= val;
			break;
    	case 0x02C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT26] &= ~val;
			break;
    	case 0x02CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT26] ^= val;
			break;
    	case 0x02D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT27] |= val;
			break;
    	case 0x02D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT27] &= ~val;
			break;
    	case 0x02DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT27] ^= val;
			break;
    	case 0x02E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT28] |= val;
			break;
    	case 0x02E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT28] &= ~val;
			break;
    	case 0x02EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT28] ^= val;
			break;
    	case 0x02F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT29] |= val;
			break;
    	case 0x02F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT29] &= ~val;
			break;
    	case 0x02FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT29] ^= val;
			break;
    	case 0x0304: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT30] |= val;
			break;
    	case 0x0308: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT30] &= ~val;
			break;
    	case 0x030C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT30] ^= val;
			break;
    	case 0x0314: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT31] |= val;
			break;
    	case 0x0318: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT31] &= ~val;
			break;
    	case 0x031C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT31] ^= val;
			break;
    	case 0x0324: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT32] |= val;
			break;
    	case 0x0328: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT32] &= ~val;
			break;
    	case 0x032C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT32] ^= val;
			break;
    	case 0x0334: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT33] |= val;
			break;
    	case 0x0338: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT33] &= ~val;
			break;
    	case 0x033C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT33] ^= val;
			break;
    	case 0x0344: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT34] |= val;
			break;
    	case 0x0348: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT34] &= ~val;
			break;
    	case 0x034C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT34] ^= val;
			break;
    	case 0x0354: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT35] |= val;
			break;
    	case 0x0358: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT35] &= ~val;
			break;
    	case 0x035C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT35] ^= val;
			break;
    	case 0x0364: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT36] |= val;
			break;
    	case 0x0368: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT36] &= ~val;
			break;
    	case 0x036C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT36] ^= val;
			break;
    	case 0x0374: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT37] |= val;
			break;
    	case 0x0378: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT37] &= ~val;
			break;
    	case 0x037C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT37] ^= val;
			break;
    	case 0x0384: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT38] |= val;
			break;
    	case 0x0388: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT38] &= ~val;
			break;
    	case 0x038C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT38] ^= val;
			break;
    	case 0x0394: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT39] |= val;
			break;
    	case 0x0398: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT39] &= ~val;
			break;
    	case 0x039C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT39] ^= val;
			break;
    	case 0x03A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT40] |= val;
			break;
    	case 0x03A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT40] &= ~val;
			break;
    	case 0x03AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT40] ^= val;
			break;
    	case 0x03B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT41] |= val;
			break;
    	case 0x03B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT41] &= ~val;
			break;
    	case 0x03BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT41] ^= val;
			break;
    	case 0x03C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT42] |= val;
			break;
    	case 0x03C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT42] &= ~val;
			break;
    	case 0x03CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT42] ^= val;
			break;
    	case 0x03D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT43] |= val;
			break;
    	case 0x03D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT43] &= ~val;
			break;
    	case 0x03DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT43] ^= val;
			break;
    	case 0x03E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT44] |= val;
			break;
    	case 0x03E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT44] &= ~val;
			break;
    	case 0x03EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT44] ^= val;
			break;
    	case 0x03F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT45] |= val;
			break;
    	case 0x03F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT45] &= ~val;
			break;
    	case 0x03FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT45] ^= val;
			break;
    	case 0x0404: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT46] |= val;
			break;
    	case 0x0408: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT46] &= ~val;
			break;
    	case 0x040C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT46] ^= val;
			break;
    	case 0x0414: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT47] |= val;
			break;
    	case 0x0418: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT47] &= ~val;
			break;
    	case 0x041C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT47] ^= val;
			break;
    	case 0x0424: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT48] |= val;
			break;
    	case 0x0428: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT48] &= ~val;
			break;
    	case 0x042C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT48] ^= val;
			break;
    	case 0x0434: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT49] |= val;
			break;
    	case 0x0438: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT49] &= ~val;
			break;
    	case 0x043C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT49] ^= val;
			break;
    	case 0x0444: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT50] |= val;
			break;
    	case 0x0448: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT50] &= ~val;
			break;
    	case 0x044C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT50] ^= val;
			break;
    	case 0x0454: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT51] |= val;
			break;
    	case 0x0458: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT51] &= ~val;
			break;
    	case 0x045C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT51] ^= val;
			break;
    	case 0x0464: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT52] |= val;
			break;
    	case 0x0468: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT52] &= ~val;
			break;
    	case 0x046C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT52] ^= val;
			break;
    	case 0x0474: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT53] |= val;
			break;
    	case 0x0478: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT53] &= ~val;
			break;
    	case 0x047C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT53] ^= val;
			break;
    	case 0x0484: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT54] |= val;
			break;
    	case 0x0488: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT54] &= ~val;
			break;
    	case 0x048C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT54] ^= val;
			break;
    	case 0x0494: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT55] |= val;
			break;
    	case 0x0498: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT55] &= ~val;
			break;
    	case 0x049C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT55] ^= val;
			break;
    	case 0x04A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT56] |= val;
			break;
    	case 0x04A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT56] &= ~val;
			break;
    	case 0x04AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT56] ^= val;
			break;
    	case 0x04B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT57] |= val;
			break;
    	case 0x04B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT57] &= ~val;
			break;
    	case 0x04BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT57] ^= val;
			break;
    	case 0x04C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT58] |= val;
			break;
    	case 0x04C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT58] &= ~val;
			break;
    	case 0x04CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT58] ^= val;
			break;
    	case 0x04D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT59] |= val;
			break;
    	case 0x04D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT59] &= ~val;
			break;
    	case 0x04DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT59] ^= val;
			break;
    	case 0x04E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT60] |= val;
			break;
    	case 0x04E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT60] &= ~val;
			break;
    	case 0x04EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT60] ^= val;
			break;
    	case 0x04F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT61] |= val;
			break;
    	case 0x04F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT61] &= ~val;
			break;
    	case 0x04FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT61] ^= val;
			break;
    	case 0x0504: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT62] |= val;
			break;
    	case 0x0508: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT62] &= ~val;
			break;
    	case 0x050C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT62] ^= val;
			break;
    	case 0x0514: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT63] |= val;
			break;
    	case 0x0518: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT63] &= ~val;
			break;
    	case 0x051C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT63] ^= val;
			break;
    	case 0x0524: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT64] |= val;
			break;
    	case 0x0528: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT64] &= ~val;
			break;
    	case 0x052C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT64] ^= val;
			break;
    	case 0x0534: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT65] |= val;
			break;
    	case 0x0538: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT65] &= ~val;
			break;
    	case 0x053C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT65] ^= val;
			break;
    	case 0x0544: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT66] |= val;
			break;
    	case 0x0548: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT66] &= ~val;
			break;
    	case 0x054C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT66] ^= val;
			break;
    	case 0x0554: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT67] |= val;
			break;
    	case 0x0558: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT67] &= ~val;
			break;
    	case 0x055C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT67] ^= val;
			break;
    	case 0x0564: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT68] |= val;
			break;
    	case 0x0568: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT68] &= ~val;
			break;
    	case 0x056C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT68] ^= val;
			break;
    	case 0x0574: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT69] |= val;
			break;
    	case 0x0578: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT69] &= ~val;
			break;
    	case 0x057C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT69] ^= val;
			break;
    	case 0x0584: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT70] |= val;
			break;
    	case 0x0588: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT70] &= ~val;
			break;
    	case 0x058C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT70] ^= val;
			break;
    	case 0x0594: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT71] |= val;
			break;
    	case 0x0598: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT71] &= ~val;
			break;
    	case 0x059C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT71] ^= val;
			break;
    	case 0x05A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT72] |= val;
			break;
    	case 0x05A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT72] &= ~val;
			break;
    	case 0x05AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT72] ^= val;
			break;
    	case 0x05B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT73] |= val;
			break;
    	case 0x05B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT73] &= ~val;
			break;
    	case 0x05BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT73] ^= val;
			break;
    	case 0x05C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT74] |= val;
			break;
    	case 0x05C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT74] &= ~val;
			break;
    	case 0x05CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT74] ^= val;
			break;
    	case 0x05D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT75] |= val;
			break;
    	case 0x05D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT75] &= ~val;
			break;
    	case 0x05DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT75] ^= val;
			break;
    	case 0x05E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT76] |= val;
			break;
    	case 0x05E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT76] &= ~val;
			break;
    	case 0x05EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT76] ^= val;
			break;
    	case 0x05F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT77] |= val;
			break;
    	case 0x05F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT77] &= ~val;
			break;
    	case 0x05FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT77] ^= val;
			break;
    	case 0x0604: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT78] |= val;
			break;
    	case 0x0608: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT78] &= ~val;
			break;
    	case 0x060C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT78] ^= val;
			break;
    	case 0x0614: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT79] |= val;
			break;
    	case 0x0618: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT79] &= ~val;
			break;
    	case 0x061C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT79] ^= val;
			break;
    	case 0x0624: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT80] |= val;
			break;
    	case 0x0628: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT80] &= ~val;
			break;
    	case 0x062C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT80] ^= val;
			break;
    	case 0x0634: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT81] |= val;
			break;
    	case 0x0638: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT81] &= ~val;
			break;
    	case 0x063C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT81] ^= val;
			break;
    	case 0x0644: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT82] |= val;
			break;
    	case 0x0648: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT82] &= ~val;
			break;
    	case 0x064C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT82] ^= val;
			break;
    	case 0x0654: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT83] |= val;
			break;
    	case 0x0658: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT83] &= ~val;
			break;
    	case 0x065C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT83] ^= val;
			break;
    	case 0x0664: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT84] |= val;
			break;
    	case 0x0668: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT84] &= ~val;
			break;
    	case 0x066C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT84] ^= val;
			break;
    	case 0x0674: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT85] |= val;
			break;
    	case 0x0678: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT85] &= ~val;
			break;
    	case 0x067C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT85] ^= val;
			break;
    	case 0x0684: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT86] |= val;
			break;
    	case 0x0688: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT86] &= ~val;
			break;
    	case 0x068C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT86] ^= val;
			break;
    	case 0x0694: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT87] |= val;
			break;
    	case 0x0698: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT87] &= ~val;
			break;
    	case 0x069C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT87] ^= val;
			break;
    	case 0x06A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT88] |= val;
			break;
    	case 0x06A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT88] &= ~val;
			break;
    	case 0x06AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT88] ^= val;
			break;
    	case 0x06B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT89] |= val;
			break;
    	case 0x06B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT89] &= ~val;
			break;
    	case 0x06BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT89] ^= val;
			break;
    	case 0x06C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT90] |= val;
			break;
    	case 0x06C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT90] &= ~val;
			break;
    	case 0x06CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT90] ^= val;
			break;
    	case 0x06D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT91] |= val;
			break;
    	case 0x06D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT91] &= ~val;
			break;
    	case 0x06DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT91] ^= val;
			break;
    	case 0x06E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT92] |= val;
			break;
    	case 0x06E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT92] &= ~val;
			break;
    	case 0x06EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT92] ^= val;
			break;
    	case 0x06F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT93] |= val;
			break;
    	case 0x06F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT93] &= ~val;
			break;
    	case 0x06FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT93] ^= val;
			break;
    	case 0x0704: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT94] |= val;
			break;
    	case 0x0708: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT94] &= ~val;
			break;
    	case 0x070C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT94] ^= val;
			break;
    	case 0x0714: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT95] |= val;
			break;
    	case 0x0718: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT95] &= ~val;
			break;
    	case 0x071C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT95] ^= val;
			break;
    	case 0x0724: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT96] |= val;
			break;
    	case 0x0728: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT96] &= ~val;
			break;
    	case 0x072C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT96] ^= val;
			break;
    	case 0x0734: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT97] |= val;
			break;
    	case 0x0738: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT97] &= ~val;
			break;
    	case 0x073C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT97] ^= val;
			break;
    	case 0x0744: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT98] |= val;
			break;
    	case 0x0748: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT98] &= ~val;
			break;
    	case 0x074C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT98] ^= val;
			break;
    	case 0x0754: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT99] |= val;
			break;
    	case 0x0758: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT99] &= ~val;
			break;
    	case 0x075C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT99] ^= val;
			break;
    	case 0x0764: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT100] |= val;
			break;
    	case 0x0768: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT100] &= ~val;
			break;
    	case 0x076C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT100] ^= val;
			break;
    	case 0x0774: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT101] |= val;
			break;
    	case 0x0778: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT101] &= ~val;
			break;
    	case 0x077C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT101] ^= val;
			break;
    	case 0x0784: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT102] |= val;
			break;
    	case 0x0788: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT102] &= ~val;
			break;
    	case 0x078C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT102] ^= val;
			break;
    	case 0x0794: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT103] |= val;
			break;
    	case 0x0798: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT103] &= ~val;
			break;
    	case 0x079C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT103] ^= val;
			break;
    	case 0x07A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT104] |= val;
			break;
    	case 0x07A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT104] &= ~val;
			break;
    	case 0x07AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT104] ^= val;
			break;
    	case 0x07B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT105] |= val;
			break;
    	case 0x07B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT105] &= ~val;
			break;
    	case 0x07BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT105] ^= val;
			break;
    	case 0x07C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT106] |= val;
			break;
    	case 0x07C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT106] &= ~val;
			break;
    	case 0x07CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT106] ^= val;
			break;
    	case 0x07D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT107] |= val;
			break;
    	case 0x07D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT107] &= ~val;
			break;
    	case 0x07DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT107] ^= val;
			break;
    	case 0x07E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT108] |= val;
			break;
    	case 0x07E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT108] &= ~val;
			break;
    	case 0x07EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT108] ^= val;
			break;
    	case 0x07F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT109] |= val;
			break;
    	case 0x07F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT109] &= ~val;
			break;
    	case 0x07FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT109] ^= val;
			break;
    	case 0x0804: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT110] |= val;
			break;
    	case 0x0808: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT110] &= ~val;
			break;
    	case 0x080C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT110] ^= val;
			break;
    	case 0x0814: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT111] |= val;
			break;
    	case 0x0818: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT111] &= ~val;
			break;
    	case 0x081C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT111] ^= val;
			break;
    	case 0x0824: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT112] |= val;
			break;
    	case 0x0828: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT112] &= ~val;
			break;
    	case 0x082C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT112] ^= val;
			break;
    	case 0x0834: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT113] |= val;
			break;
    	case 0x0838: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT113] &= ~val;
			break;
    	case 0x083C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT113] ^= val;
			break;
    	case 0x0844: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT114] |= val;
			break;
    	case 0x0848: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT114] &= ~val;
			break;
    	case 0x084C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT114] ^= val;
			break;
    	case 0x0854: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT115] |= val;
			break;
    	case 0x0858: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT115] &= ~val;
			break;
    	case 0x085C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT115] ^= val;
			break;
    	case 0x0864: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT116] |= val;
			break;
    	case 0x0868: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT116] &= ~val;
			break;
    	case 0x086C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT116] ^= val;
			break;
    	case 0x0874: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT117] |= val;
			break;
    	case 0x0878: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT117] &= ~val;
			break;
    	case 0x087C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT117] ^= val;
			break;
    	case 0x0884: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT118] |= val;
			break;
    	case 0x0888: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT118] &= ~val;
			break;
    	case 0x088C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT118] ^= val;
			break;
    	case 0x0894: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT119] |= val;
			break;
    	case 0x0898: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT119] &= ~val;
			break;
    	case 0x089C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT119] ^= val;
			break;
    	case 0x08A4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT120] |= val;
			break;
    	case 0x08A8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT120] &= ~val;
			break;
    	case 0x08AC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT120] ^= val;
			break;
    	case 0x08B4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT121] |= val;
			break;
    	case 0x08B8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT121] &= ~val;
			break;
    	case 0x08BC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT121] ^= val;
			break;
    	case 0x08C4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT122] |= val;
			break;
    	case 0x08C8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT122] &= ~val;
			break;
    	case 0x08CC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT122] ^= val;
			break;
    	case 0x08D4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT123] |= val;
			break;
    	case 0x08D8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT123] &= ~val;
			break;
    	case 0x08DC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT123] ^= val;
			break;
    	case 0x08E4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT124] |= val;
			break;
    	case 0x08E8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT124] &= ~val;
			break;
    	case 0x08EC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT124] ^= val;
			break;
    	case 0x08F4: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT125] |= val;
			break;
    	case 0x08F8: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT125] &= ~val;
			break;
    	case 0x08FC: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT125] ^= val;
			break;
    	case 0x0904: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT126] |= val;
			break;
    	case 0x0908: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT126] &= ~val;
			break;
    	case 0x090C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT126] ^= val;
			break;
    	case 0x0914: /* Interrupt Collector Interrupt Register 2 - Set*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT127] |= val;
			break;
    	case 0x0918: /* Interrupt Collector Interrupt Register 2 - Reset*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT127] &= ~val;
			break;
    	case 0x091C: /* Interrupt Collector Interrupt Register 2 - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_INTERRUPT127] ^= val;
			break;
    	case 0x1120: /* Interrupt Collector Debug Register 0 */
    	case 0x1124: /* Interrupt Collector Debug Register 0 */
    	case 0x1128: /* Interrupt Collector Debug Register 0 */
    	case 0x112C: /* Interrupt Collector Debug Register 0 */
    		/* read only */
    		return;
    	case 0x1130: /* Interrupt Collector Debug Read Register 0 */
    	case 0x1134: /* Interrupt Collector Debug Read Register 0 */
    	case 0x1138: /* Interrupt Collector Debug Read Register 0 */
    	case 0x113C: /* Interrupt Collector Debug Read Register 0 */
    		/* read only */
    		return;
    	case 0x1140: /* Interrupt Collector Debug Read Register 1 */
    	case 0x1144: /* Interrupt Collector Debug Read Register 1 */
    	case 0x1148: /* Interrupt Collector Debug Read Register 1 */
    	case 0x114C: /* Interrupt Collector Debug Read Register 1 */
    		/* read only */
    		return;
    	case 0x1154: /* Interrupt Collector Debug Flag Register - Set*/
			s->hw_icoll_regs[HW_ICOLL_DBGFLAG] |= val;
			break;
    	case 0x1158: /* Interrupt Collector Debug Flag Register - Reset*/
    		s->hw_icoll_regs[HW_ICOLL_DBGFLAG] &= ~val;
    		break;
    	case 0x115C: /* Interrupt Collector Debug Flag Register - Toggle*/
			s->hw_icoll_regs[HW_ICOLL_DBGFLAG] ^= val;
			break;
    	case 0x1160: /* Interrupt Collector Debug Read Request Register 0 */
    	case 0x1164: /* Interrupt Collector Debug Read Request Register 0 */
    	case 0x1168: /* Interrupt Collector Debug Read Request Register 0 */
    	case 0x116C: /* Interrupt Collector Debug Read Request Register 0 */
    		/* read only */
    		return;
    	case 0x1170: /* Interrupt Collector Debug Read Request Register 1 */
    	case 0x1174: /* Interrupt Collector Debug Read Request Register 1 */
    	case 0x1178: /* Interrupt Collector Debug Read Request Register 1 */
    	case 0x117C: /* Interrupt Collector Debug Read Request Register 1 */
    		/* read only */
    		return;
    	case 0x1180: /* Interrupt Collector Debug Read Request Register 2 */
    	case 0x1184: /* Interrupt Collector Debug Read Request Register 2 */
    	case 0x1188: /* Interrupt Collector Debug Read Request Register 2 */
    	case 0x118C: /* Interrupt Collector Debug Read Request Register 2 */
    		/* read only */
    		return;
    	case 0x1190: /* Interrupt Collector Debug Read Request Register 3 */
    	case 0x1194: /* Interrupt Collector Debug Read Request Register 3 */
    	case 0x1198: /* Interrupt Collector Debug Read Request Register 3 */
    	case 0x119C: /* Interrupt Collector Debug Read Request Register 3 */
    		/* read only */
    		return;
    	case 0x11E0: /* Interrupt Collector Version Register */
    	case 0x11E4: /* Interrupt Collector Version Register */
    	case 0x11E8: /* Interrupt Collector Version Register */
    	case 0x11EC: /* Interrupt Collector Version Register */
    		/* read only */
    		return;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "imx28_gic_write: Bad offset 0x%x\n",
                          (int)offset);
    }
}

static const MemoryRegionOps imx28_gic_ops = {
    .read = imx28_gic_read,
    .write = imx28_gic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int imx28_gic_init(SysBusDevice *sbd)
{
    DeviceState *gicdev = DEVICE(sbd);
    IMX28GICState *s = IMX28_GIC(gicdev);

    memory_region_init_io(&s->iomem, OBJECT(s), &imx28_gic_ops, s,
                          "imx28_gic", 0x2000);
    sysbus_init_mmio(sbd, &s->iomem);

    qdev_init_gpio_in(gicdev, imx28_gic_set_irq, IMX28_GIC_NUM_IRQS);
    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_irq(sbd, &s->fiq);

    return 0;
}

static void imx28_gic_reset(DeviceState *dev)
{
	IMX28GICState *s = IMX28_GIC(dev);

	s->pending_high = 0x0;
	s->pending_low = 0x0;

	memset(s->hw_icoll_regs, 0, sizeof s->hw_icoll_regs);

	s->hw_icoll_regs[HW_ICOLL_CTRL] |= 0xC0030000;
	s->hw_icoll_regs[HW_ICOLL_STAT] |= 0xECA94567;
	s->hw_icoll_regs[HW_ICOLL_DBGREAD0] |= 0x1356DA98;
	s->hw_icoll_regs[HW_ICOLL_VERSION] |= 0x03010000;
}

static void imx28_gic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = imx28_gic_init;
    dc->vmsd = &vmstate_imx28_gic;
    dc->reset = imx28_gic_reset;
    dc->desc = "i.MX28 Interrupt Collector";
}

static const TypeInfo imx28_gic_info = {
    .name          = TYPE_IMX28_GIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMX28GICState),
    .class_init    = imx28_gic_class_init,
};

static void imx28_gic_register_types(void)
{
    type_register_static(&imx28_gic_info);
}

type_init(imx28_gic_register_types)


