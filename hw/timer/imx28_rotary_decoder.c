/*
 * imx28_rotary_decoder.c
 *
 *  Created on: 24.07.2014
 *      Author: schoeni
 */

#include "hw/sysbus.h"
#include "hw/qdev.h"

#define TYPE_IMX28_ROTARY "imx28_rotary_decoder"
#define IMX28_ROTARY(obj) OBJECT_CHECK(IMX28RotaryState, (obj), TYPE_IMX28_ROTARY )

#define NUM_HW_ROT_DEC_REGS				2
#define HW_TIMROT_ROTCTRL						0
#define HW_TIMROT_ROTCOUNT					1

typedef struct IMX28RotaryState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t hw_rot_dec_regs[NUM_HW_ROT_DEC_REGS];

    qemu_irq irq;
} IMX28RotaryState;

static uint64_t imx28_rotary_read(void *opaque, hwaddr offset,
                           unsigned size)
{
	//printf("\nTimer Read %08x\n", (unsigned int)offset);
	IMX28RotaryState *s = (IMX28RotaryState *)opaque;

	switch (offset)
	{
	   	case 0x0000: /* Rotary Decoder Control Register */
	   		return s->hw_rot_dec_regs[HW_TIMROT_ROTCTRL];
	   	case 0x0010: /* Rotary Decoder Control Register */
	   	    return 0;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "imx28_rotary_decoder_read: Bad offset 0x%x\n",
                          (int)offset);
            return 0;
    }
}

static void imx28_rotary_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
	//printf("\nTimer Write %08x; %08x\n", (unsigned int) offset, (unsigned int) value);

	IMX28RotaryState *s = (IMX28RotaryState *)opaque;

    switch (offset)
    {
    	case 0x0004: /* Rotary Decoder Control Register - Set */
    		if (value & 0x80000000) /* Soft reset */
    		{
    			/* set default values */
    			//printf("\nSoft-Reset\n");
    			s->hw_rot_dec_regs[HW_TIMROT_ROTCTRL] |= 0xFE000000;
    		}

    		s->hw_rot_dec_regs[HW_TIMROT_ROTCTRL] |= value;
    		break;
    	case 0x0008: /* Rotary Decoder Control Register - Reset */
    		s->hw_rot_dec_regs[HW_TIMROT_ROTCTRL] &= ~value;
    		break;
    	case 0x000C: /* Rotary Decoder Control Register - Toggle */
    		s->hw_rot_dec_regs[HW_TIMROT_ROTCTRL] ^= value;
    		break;

    	case 0x0014: /* Rotary Decoder Up/Down Counter Register - Set */
    	case 0x0018: /* Rotary Decoder Up/Down Counter Register - Reset */
    	case 0x001C: /* Rotary Decoder Up/Down Counter Register - Toggle */
    		/* read only */
    		return;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "imx28_rotary_decoder_write: Bad offset 0x%x\n",
                          (int)offset);
    }
}

static const MemoryRegionOps imx28_rotary_ops = {
    .read = imx28_rotary_read,
    .write = imx28_rotary_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_imx28_rotary_decoder = {
    .name = "imx28_rotary_decoder",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(hw_rot_dec_regs, IMX28RotaryState, NUM_HW_ROT_DEC_REGS),
        VMSTATE_END_OF_LIST()
    }
};

static int imx28_rotary_init(SysBusDevice *sbd)
{
	//printf("\nTimer Init\n");
    DeviceState *dev = DEVICE(sbd);
	IMX28RotaryState *s = IMX28_ROTARY(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &imx28_rotary_ops, s,
                          "imx28_rotary", 0x20);
    sysbus_init_mmio(sbd, &s->iomem);
    vmstate_register(dev, -1, &vmstate_imx28_rotary_decoder, s);

    return 0;
}

static void imx28_rotary_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = imx28_rotary_init;
}

static const TypeInfo imx28_info = {
    .name          = TYPE_IMX28_ROTARY,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMX28RotaryState),
    .class_init    = imx28_rotary_class_init,
};

static void arm_timer_register_types(void)
{
    type_register_static(&imx28_info);
}

type_init(arm_timer_register_types)

