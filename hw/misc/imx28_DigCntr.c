/*
 * imx28_DigCntr.c
 *
 *  Created on: 05.07.2014
 *      Author: schoeni
 */

#include "hw/sysbus.h"

#define TYPE_IMX28_DIGCNTR "imx28_digcntr"
#define IMX28_DIGCNTR(obj) \
    OBJECT_CHECK(imx28_digcntr_state, (obj), TYPE_IMX28_DIGCNTR)

uint32_t dflpt_mpte_loc_global[16];

typedef struct {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t dflpt_mpte_loc[16];
} imx28_digcntr_state;

static const VMStateDescription vmstate_imx28_digcntr = {
    .name = "imx28_digcntr",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(dflpt_mpte_loc, imx28_digcntr_state, 16),
        VMSTATE_END_OF_LIST()
    }
};

static void imx28_digcntr_write(void *opaque, hwaddr offset,
                             uint64_t val, unsigned size)
{
	imx28_digcntr_state *s = (imx28_digcntr_state *) opaque;

    //printf("\n#########imx28_digcntr_write: %08x, %08x #########\n",  (unsigned int)offset, (unsigned int) val);

    switch (offset) {
    case 0x000: /* DIGCTL Control Register */
    case 0x010: /* DIGCTL Status Register */
    case 0x020: /* Free-Running HCLK Counter Register */
    case 0x030: /* On-Chip RAM Control Register */
    case 0x040: /* EMI Status Register */
    case 0x050: /* On-Chip Memories Read Margin Register*/
    case 0x060: /* Software Write-Once Register */
    case 0x070: /* BIST Control Register */
    case 0x080: /* DIGCTL Status  Register */
    case 0x090: /* Entropy Register */
    case 0x0A0: /* Entropy Latched Register */
    case 0x0C0: /* Digital Control Microseconds Counter Register */
    case 0x0D0: /*Digital Control Debug Read Test Register */
    case 0x0E0: /* Digital Control Debug  Register */
    case 0x100: /* USB LOOP BACK */
    case 0x110: /* SRAM Status Register 0 */
    case 0x120: /* SRAM Status Register 1 */
    case 0x130: /* SRAM Status Register 2 */
    case 0x140: /* SRAM Status Register 3 */
    case 0x150: /* SRAM Status Register 4 */
    case 0x160: /* SRAM Status Register 5 */
    case 0x170: /* SRAM Status Register 6 */
    case 0x180: /* SRAM Status Register 7 */
    case 0x190: /* SRAM Status Register 8 */
    case 0x1A0: /* SRAM Status Register 9 */
    case 0x1B0: /* SRAM Status Register 10 */
    case 0x1C0: /* SRAM Status Register 11 */
    case 0x1D0: /* SRAM Status Register 12 */
    case 0x1E0: /* SRAM Status Register 13 */
    case 0x280: /* Digital Control Scratch Register 0 */
    case 0x290: /* Digital Control Scratch Register 1 */
    case 0x2A0: /* Digital  ARM Cache Register */
    case 0x2B0: /* Debug  Trap Control and  Status for AHB Layer 0 and 3 */
    case 0x2C0: /* Debug Trap Range Low Address for AHB Layer 0 */
    case 0x2D0: /* Debug Trap Range High Address for AHB Layer 0 */
    case 0x2E0: /* Debug Trap Range Low Address for AHB Layer 3 */
    case 0x2F0: /* Debug Trap Range High Address for AHB Layer 3 */
    case 0x300: /* Freescale Copyright Identifier Register */
    case 0x310: /* Digital Control Chip Revision Register */
    case 0x330: /* AHB Statistics Control Register */
    case 0x370: /* AHB Layer 1 Transfer Count Register */
    case 0x380: /* AHB Layer 1 Performance Metric for Stalled Bus Cycles Register */
    case 0x390: /* AHB Layer 1 Performance Metric for Valid Bus Cycles Register */
    case 0x3A0: /* AHB Layer 2 Transfer Count register Register */
    case 0x3B0: /* AHB Layer 2 Performance Metric for Stalled Bus Cycles Register */
    case 0x3C0: /* AHB Layer 2 Performance Metric for Valid Bus Cycles Register */
    case 0x3D0: /* AHB Layer 3 Transfer Count register Register */
    case 0x3E0: /* AHB Layer 3 Performance Metric for Stalled Bus Cycles Register */
    case 0x3F0: /* AHB Layer 3 Performance Metridflptc for Valid Bus Cycles Register */
        qemu_log_mask(LOG_GUEST_ERROR,
                      "imx28_digcntr_read: Register not implemented  (offset: 0x%x)\n", (int)offset);
        break;
    case 0x500: /* Default First Level Page Table Movable PTE Locator 0 */
    	s->dflpt_mpte_loc[0] = dflpt_mpte_loc_global[0] = val;
    	break;
    case 0x510: /* Default First Level Page Table Movable PTE Locator 1 */
    	s->dflpt_mpte_loc[1] = dflpt_mpte_loc_global[1] = val;
    	break;
    case 0x520: /* Default First Level Page Table Movable PTE Locator 2 */
    	s->dflpt_mpte_loc[2] = dflpt_mpte_loc_global[2] = val;
    	break;
    case 0x530: /* Default First Level Page Table Movable PTE Locator 3 */
    	s->dflpt_mpte_loc[3] = dflpt_mpte_loc_global[3] = val;
    	break;
    case 0x540: /* Default First Level Page Table Movable PTE Locator 4 */
    	s->dflpt_mpte_loc[4] = dflpt_mpte_loc_global[4] = val;
    	break;
    case 0x550: /* Default First Level Page Table Movable PTE Locator 5 */
    	s->dflpt_mpte_loc[5] = dflpt_mpte_loc_global[5] = val;
    	break;
    case 0x560: /* Default First Level Page Table Movable PTE Locator 6 */
    	s->dflpt_mpte_loc[6] = dflpt_mpte_loc_global[6] = val;
    	break;
    case 0x570: /* Default First Level Page Table Movable PTE Locator 7 */
    	//printf("\n###########imx28_digcntr_write return: %08x #########\n",  (unsigned int)val);
    	s->dflpt_mpte_loc[7] = dflpt_mpte_loc_global[7] = val;
    	break;
    case 0x580: /* Default First Level Page Table Movable PTE Locator 8 */
    	s->dflpt_mpte_loc[8] = dflpt_mpte_loc_global[8] = val;
    	break;
    case 0x590: /* Default First Level Page Table Movable PTE Locator 9 */
    	s->dflpt_mpte_loc[9]  = dflpt_mpte_loc_global[9] = val;
    	break;
    case 0x5A0: /* Default First Level Page Table Movable PTE Locator 10 */
    	s->dflpt_mpte_loc[10] = dflpt_mpte_loc_global[10] = val;
    	break;
    case 0x5B0: /* Default First Level Page Table Movable PTE Locator 11 */
    	s->dflpt_mpte_loc[11] = dflpt_mpte_loc_global[11] = val;
    	break;
    case 0x5C0: /* Default First Level Page Table Movable PTE Locator 12 */
    	s->dflpt_mpte_loc[12] = dflpt_mpte_loc_global[12] = val;
    	break;
    case 0x5D0: /* Default First Level Page Table Movable PTE Locator 13 */
    	s->dflpt_mpte_loc[13] = dflpt_mpte_loc_global[13] = val;
    	break;
    case 0x5E0: /* Default First Level Page Table Movable PTE Locator 14 */
    	s->dflpt_mpte_loc[14] = dflpt_mpte_loc_global[14] = val;
    	break;
    case 0x5F0: /* Default First Level Page Table Movable PTE Locator 15 */
    	s->dflpt_mpte_loc[15] = dflpt_mpte_loc_global[15] = val;
    	break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "imx28_digcntr_read: Bad register offset 0x%x\n",
                      (int)offset);
        return;
    }
}


static uint64_t imx28_digcntr_read(void *opaque, hwaddr offset,
                                unsigned size)
{
	imx28_digcntr_state *s = (imx28_digcntr_state *) opaque;

	//printf("\n#########imx28_digcntr_read: %08x #########\n",  (unsigned int)offset);

    switch (offset) {
    case 0x000: /* DIGCTL Control Register */
    case 0x010: /* DIGCTL Status Register */
    case 0x020: /* Free-Running HCLK Counter Register */
    case 0x030: /* On-Chip RAM Control Register */
    case 0x040: /* EMI Status Register */
    case 0x050: /* On-Chip Memories Read Margin Register*/
    case 0x060: /* Software Write-Once Register */
    case 0x070: /* BIST Control Register */
    case 0x080: /* DIGCTL Status  Register */
    case 0x090: /* Entropy Register */
    case 0x0A0: /* Entropy Latched Register */
    case 0x0C0: /* Digital Control Microseconds Counter Register */
    case 0x0D0: /*Digital Control Debug Read Test Register */
    case 0x0E0: /* Digital Control Debug  Register */
    case 0x100: /* USB LOOP BACK */
    case 0x110: /* SRAM Status Register 0 */
    case 0x120: /* SRAM Status Register 1 */
    case 0x130: /* SRAM Status Register 2 */
    case 0x140: /* SRAM Status Register 3 */
    case 0x150: /* SRAM Status Register 4 */
    case 0x160: /* SRAM Status Register 5 */
    case 0x170: /* SRAM Status Register 6 */
    case 0x180: /* SRAM Status Register 7 */
    case 0x190: /* SRAM Status Register 8 */
    case 0x1A0: /* SRAM Status Register 9 */
    case 0x1B0: /* SRAM Status Register 10 */
    case 0x1C0: /* SRAM Status Register 11 */
    case 0x1D0: /* SRAM Status Register 12 */
    case 0x1E0: /* SRAM Status Register 13 */
    case 0x280: /* Digital Control Scratch Register 0 */
    case 0x290: /* Digital Control Scratch Register 1 */
    case 0x2A0: /* Digital  ARM Cache Register */
    case 0x2B0: /* Debug  Trap Control and  Status for AHB Layer 0 and 3 */
    case 0x2C0: /* Debug Trap Range Low Address for AHB Layer 0 */
    case 0x2D0: /* Debug Trap Range High Address for AHB Layer 0 */
    case 0x2E0: /* Debug Trap Range Low Address for AHB Layer 3 */
    case 0x2F0: /* Debug Trap Range High Address for AHB Layer 3 */
    case 0x300: /* Freescale Copyright Identifier Register */
    case 0x310: /* Digital Control Chip Revision Register */
    case 0x330: /* AHB Statistics Control Register */
    case 0x370: /* AHB Layer 1 Transfer Count Register */
    case 0x380: /* AHB Layer 1 Performance Metric for Stalled Bus Cycles Register */
    case 0x390: /* AHB Layer 1 Performance Metric for Valid Bus Cycles Register */
    case 0x3A0: /* AHB Layer 2 Transfer Count register Register */
    case 0x3B0: /* AHB Layer 2 Performance Metric for Stalled Bus Cycles Register */
    case 0x3C0: /* AHB Layer 2 Performance Metric for Valid Bus Cycles Register */
    case 0x3D0: /* AHB Layer 3 Transfer Count register Register */
    case 0x3E0: /* AHB Layer 3 Performance Metric for Stalled Bus Cycles Register */
    case 0x3F0: /* AHB Layer 3 Performance Metric for Valid Bus Cycles Register */
        qemu_log_mask(LOG_GUEST_ERROR,
                      "imx28_digcntr_read: Register not implemented  (offset: 0x%x)\n", (int)offset);
        return 0;
    case 0x500: /* Default First Level Page Table Movable PTE Locator 0 */
    	return s->dflpt_mpte_loc[0];
    case 0x510: /* Default First Level Page Table Movable PTE Locator 1 */
    	return s->dflpt_mpte_loc[1];
    case 0x520: /* Default First Level Page Table Movable PTE Locator 2 */
    	return s->dflpt_mpte_loc[2];
    case 0x530: /* Default First Level Page Table Movable PTE Locator 3 */
    	return s->dflpt_mpte_loc[3];
    case 0x540: /* Default First Level Page Table Movable PTE Locator 4 */
    	return s->dflpt_mpte_loc[4];
    case 0x550: /* Default First Level Page Table Movable PTE Locator 5 */
    	return s->dflpt_mpte_loc[5];
    case 0x560: /* Default First Level Page Table Movable PTE Locator 6 */
    	return s->dflpt_mpte_loc[6];
    case 0x570: /* Default First Level Page Table Movable PTE Locator 7 */
    	return s->dflpt_mpte_loc[7];
    case 0x580: /* Default First Level Page Table Movable PTE Locator 8 */
    	return s->dflpt_mpte_loc[8];
    case 0x590: /* Default First Level Page Table Movable PTE Locator 9 */
    	return s->dflpt_mpte_loc[9];
    case 0x5A0: /* Default First Level Page Table Movable PTE Locator 10 */
    	return s->dflpt_mpte_loc[10];
    case 0x5B0: /* Default First Level Page Table Movable PTE Locator 11 */
    	return s->dflpt_mpte_loc[11];
    case 0x5C0: /* Default First Level Page Table Movable PTE Locator 12 */
    	return s->dflpt_mpte_loc[12];
    case 0x5D0: /* Default First Level Page Table Movable PTE Locator 13 */
    	return s->dflpt_mpte_loc[13];
    case 0x5E0: /* Default First Level Page Table Movable PTE Locator 14 */
    	return s->dflpt_mpte_loc[14];
    case 0x5F0: /* Default First Level Page Table Movable PTE Locator 15 */
    	return s->dflpt_mpte_loc[15];
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "imx28_digcntr_read: Bad register offset 0x%x\n",
                      (int)offset);
        return 0;
    }
}

static const MemoryRegionOps imx28_digcntr_ops = {
    .read = imx28_digcntr_read,
    .write = imx28_digcntr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void imx28_digcntr_init(Object *obj)
{
	int i = 0;

	//printf("\n########imx28_digcntr_init#########\n");
    DeviceState *dev = DEVICE(obj);
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    imx28_digcntr_state *s = IMX28_DIGCNTR(obj);

   memory_region_init_io(&s->iomem, OBJECT(dev), &imx28_digcntr_ops, s,
                          "imx28_digcntr", 0x2000);
    sysbus_init_mmio(sd, &s->iomem);

    for (i = 0; i < 16; i++)
    	s->dflpt_mpte_loc[i] = dflpt_mpte_loc_global[i] = i;
}

static Property imx28_digcntr_properties[] = {
    //DEFINE_PROP_UINT32("freq0", imx28_digcntr_state, freq0, 24000000),
        DEFINE_PROP_END_OF_LIST(),
};

static void imx28_digcntr_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    k->vmsd = &vmstate_imx28_digcntr;
    k->props = imx28_digcntr_properties;
}

static const TypeInfo imx28_digcntr_info = {
    .name          = TYPE_IMX28_DIGCNTR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(imx28_digcntr_state),
    .instance_init = imx28_digcntr_init,
    .class_init    = imx28_digcntr_class_init,
};

static void imx28_digcntr_register_types(void)
{
    type_register_static(&imx28_digcntr_info);
}

type_init(imx28_digcntr_register_types)





