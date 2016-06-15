/*
 * imnx28_dflpt.c
 *
 *  Created on: 02.07.2014
 *      Author: schoeni
 */

//#include "hw/hw.h"
//#include "qemu/timer.h"
//#include "qemu/bitops.h"
#include "hw/sysbus.h"
#include "math.h"
//#include "sysemu/sysemu.h"

#define TYPE_IMX28_DFLPT "imx28_dflpt"
#define IMX28_DFLPT(obj) \
    OBJECT_CHECK(imx28_dflpt_state, (obj), TYPE_IMX28_DFLPT)

#define DFLPT_BASE_ADDRESS	0x800C0000

extern uint32_t dflpt_mpte_loc_global[16];

char is_safe_rtos = 1;

typedef struct {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t moveable_pte[16];
    hwaddr mpte_base_addr[16];
    uint32_t pio_register_map_entry;

} imx28_dflpt_state;

static const VMStateDescription vmstate_imx28_dflpt = {
    .name = "imx28_dflpt",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
    	VMSTATE_UINT32_ARRAY(moveable_pte, imx28_dflpt_state, 16),
    	VMSTATE_UINT64_ARRAY(mpte_base_addr, imx28_dflpt_state, 16),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t imx28_dflpt_read(void *opaque, hwaddr offset,
                                unsigned size)
{
	//printf("\nDFLPT read %08x\n",(DFLPT_BASE_ADDRESS+offset));
	imx28_dflpt_state *s = (imx28_dflpt_state *) opaque;
	hwaddr virtual_addr = (DFLPT_BASE_ADDRESS+offset);
	hwaddr addr_13_2;
	hwaddr hw_digctl_mpte_span = 0, exp_span_value = 0;
	hwaddr hw_digctl_mpte_dis = 0;
	hwaddr hw_digctl_mpte_loc = 0;
	uint64_t ret = 0;
	int i = 0;

	 /* fixed entry PIO register map */
    if (offset == 0x2000)
        return ((s->pio_register_map_entry) & 0xFFFFFFFB);

    /* MPTEi */
    for (i = 0; i < 16; i++)
    {
    	hw_digctl_mpte_span = (dflpt_mpte_loc_global[i] >> 24) & 0x00000007;
    	hw_digctl_mpte_dis = (dflpt_mpte_loc_global[i] >> 31);
    	hw_digctl_mpte_loc = dflpt_mpte_loc_global[i] & 0x00000FFF;
    	addr_13_2 = (virtual_addr >> 2)  & 0x00000FFF;
        exp_span_value = (unsigned char) pow(2, hw_digctl_mpte_span);

        if ((virtual_addr >= (DFLPT_BASE_ADDRESS + (hw_digctl_mpte_loc << 2))) &&
             (virtual_addr < (DFLPT_BASE_ADDRESS + ( (hw_digctl_mpte_loc + exp_span_value) << 2))))
        {
            /* Disabled DFLPT */
            if (hw_digctl_mpte_dis == 1)
            {
            	//printf("\nhw_digctl_mpte %x\n", dflpt_mpte_loc_global[i]);
                qemu_log_mask(LOG_GUEST_ERROR, "imx28_dflpt_read: MPTE disabled (offset: 0x%x)\n",
                              (int)offset);
                return 0;
            }

        	hw_digctl_mpte_loc += (virtual_addr - s->mpte_base_addr[i]) / 4                                                                                                                                                                                                                                                                                                    ;

        	ret =  s->moveable_pte[i] + ((addr_13_2 -  hw_digctl_mpte_loc) << 20);
//                if (i==7){
//        	    	printf("\n++++++++imx28_dflpt_read: %08x %08x+++++++\n",
//        	    			(unsigned int)(virtual_addr),  (unsigned int)dflpt_mpte_loc_global[i]);
//
////
//        	        printf("\nDEBUG: %08x; %08x; %08x; %08x; %08x; %08x; %08x\n", hw_digctl_mpte_dis, hw_digctl_mpte_span, hw_digctl_mpte_loc,
//        	        		exp_span_value, addr_13_2, virtual_addr, ret);
//        //}

        	return ret;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR, "imx28_dflpt_read: Bad register offset 0x%x\n",
                  (int)offset);
    return 0;
}


static void imx28_dflpt_write(void *opaque, hwaddr offset,
                             uint64_t val, unsigned size)
{
	//printf("\nDFLPT write %08x\n",(DFLPT_BASE_ADDRESS+offset));
	imx28_dflpt_state *s = (imx28_dflpt_state *) opaque;
	hwaddr virtual_addr = (DFLPT_BASE_ADDRESS+offset);
	hwaddr hw_digctl_mpte_span = 0, exp_span_value = 0;
	hwaddr hw_digctl_mpte_dis = 0;
	hwaddr hw_digctl_mpte_loc = 0;
	int i = 0;

	 /* fixed entry PIO register map */
    if (offset == 0x2000)
    {
    	/* Set Acess Permission */
    	s->pio_register_map_entry |= (val & 0xC00);
    	/* Set Domain */
    	s->pio_register_map_entry |= (val & 0x1E0);
    	/* Set Bufferable */
    	s->pio_register_map_entry |= (val & 0x4);
        return;
    }

    /* MPTEi */
    for (i = 0; i < 16; i++)
    {
    	hw_digctl_mpte_span = (dflpt_mpte_loc_global[i] >> 24) & 0x00000007;
    	hw_digctl_mpte_dis = (dflpt_mpte_loc_global[i] >> 31);
    	hw_digctl_mpte_loc = dflpt_mpte_loc_global[i] & 0x00000FFF;
        exp_span_value = pow(2, hw_digctl_mpte_span);

		/* fixed entry PIO register map */
		if (hw_digctl_mpte_loc == 0x800)
		{
			qemu_log_mask(LOG_GUEST_ERROR, "imx28_dflpt_write: Bad register offset 0x%x\n",
			            			 (int)offset);
		    return;
		}

        if ((virtual_addr >= (DFLPT_BASE_ADDRESS + (hw_digctl_mpte_loc << 2))) &&
             (virtual_addr < (DFLPT_BASE_ADDRESS + ( (hw_digctl_mpte_loc + exp_span_value) << 2))))
        {
    	    /* Disabled DFLPT */
    	    if (hw_digctl_mpte_dis == 1)
    	    {
    	        qemu_log_mask(LOG_GUEST_ERROR, "imx28_dflpt_write: MPTE disabled (offset: 0x%x)\n",
    	                      (int)offset);
    	        return;
    	    }

            /* Reset MPTE and MPTE_LOC*/
            if (val == 0x0)
            {
            	s->moveable_pte[i] = 0x0;
            	dflpt_mpte_loc_global[i] = i;
            	return;
            }

         	s->moveable_pte[i] = (uint32_t) val;
         	s->mpte_base_addr[i] = virtual_addr;
//        	if (i==7){
//
//       	    printf("\n++++++++imx28_dflpt_write: %08x, %08x %08x+++++++\n",
//        	    	(unsigned int)(virtual_addr),  (unsigned int)s->moveable_pte[i],  (unsigned int) dflpt_mpte_loc_global[i] );
//
//
//       	        printf("\nDEBUG: %08x; %08x; %08x; %08x; %08x\n", hw_digctl_mpte_dis, hw_digctl_mpte_span, hw_digctl_mpte_loc,
//        	        		exp_span_value, virtual_addr);
//        	}
        	return;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR, "imx28_dflpt_write: Bad register offset 0x%x\n",
                  (int)offset);
    return;
}

static const MemoryRegionOps imx28_dflpt_ops = {
    .read = imx28_dflpt_read,
    .write = imx28_dflpt_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void imx28_dflpt_init(Object *obj)
{
	//printf("\n++++++imx28_dflpt_init++++++++\n");
    DeviceState *dev = DEVICE(obj);
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    imx28_dflpt_state *s = IMX28_DFLPT(obj);

    s->pio_register_map_entry = 0x80000C12;

   memory_region_init_io(&s->iomem, OBJECT(dev), &imx28_dflpt_ops, s,
                          "imx28_dflpt", 0x10000);
    sysbus_init_mmio(sd, &s->iomem);
}

static Property imx28_dflpt_properties[] = {
    //DEFINE_PROP_UINT32("dflpt_mpte_0", imx28_dflpt_state, dflpt_mpte_0, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void imx28_dflpt_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    k->vmsd = &vmstate_imx28_dflpt;
    k->props = imx28_dflpt_properties;
}

static const TypeInfo imx28_dflpt_info = {
    .name          = TYPE_IMX28_DFLPT,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(imx28_dflpt_state),
    .instance_init = imx28_dflpt_init,
    .class_init    = imx28_dflpt_class_init,
};

static void imx28_dflpt_register_types(void)
{
    type_register_static(&imx28_dflpt_info);
}

type_init(imx28_dflpt_register_types)


