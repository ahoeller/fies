/*
 * imx28_timer.c
 *
 *  Created on: 23.07.2014
 *      Author: schoeni
 */

#include "hw/sysbus.h"
#include "qemu/timer.h"
#include "qemu-common.h"
#include "hw/qdev.h"
#include "hw/ptimer.h"
#include "qemu/main-loop.h"


#define TYPE_IMX28_TIMER "imx28_timer"
#define IMX28_TIMER(obj) OBJECT_CHECK(IMX28TimerState, (obj), TYPE_IMX28_TIMER)

#define TIMER_CTRL_IE           					(1 << 14)
#define TIMER_PRESACLER_SHIFT 			(4)
#define TIMER_PRESACLER_MASK				(0x3)
#define TIMER_CTRL_SELECT_MASK			(0xF)
#define TIMER_CTRL_ONESHOT_SHIFT 		(6)
#define TIMER_CTRL_ONESHOT_MASK 		(0x1)
#define TIMER_CTRL_IRQ_SHIFT 				(15)
#define TIMER_CTRL_IRQ_MASK 				(0x1)
#define TIMER_CTRL_UPDATE_SHIFT		(7)
#define TIMER_CTRL_UPDATE_MASK			(0x1)

typedef struct IMX28TimerState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    ptimer_state *timer;

    uint32_t freq;
    qemu_irq irq;

    uint32_t control;
    uint32_t fixed_cnt;
    uint32_t match_cnt;
    uint32_t int_level;
    uint32_t update;

} IMX28TimerState;

static void imx28_timer_update(IMX28TimerState *s)
{
	//printf("\n++++++++++++++Timer update interrupt %x; %x\n", s->int_level, (s->control & TIMER_CTRL_IE));
    /* Update interrupts.  */
    if (s->int_level && (s->control & TIMER_CTRL_IE))
    {
    	//printf("\nRaise irq\n");
        qemu_irq_raise(s->irq);

    }
    else
    {
    	//printf("\nLower irq\n");
        qemu_irq_lower(s->irq);
    }
}

static void imx28_timer_tick(void *opaque)
{
	//printf("\n++++++++++++++Timer tick\n");
	IMX28TimerState *s = IMX28_TIMER(opaque);

    s->int_level = 1;

    if (s->update)
    {
        ptimer_set_limit(s->timer, s->fixed_cnt , 1);
        s->update = 0;
    }

    imx28_timer_update(s);
}

static uint64_t imx28_timer_read(void *opaque, hwaddr offset,
                           unsigned size)
{
	//printf("\n++++++++++++++Timer Read %08x\n", (unsigned int)offset);
	IMX28TimerState *s = (IMX28TimerState *) opaque;

    uint32_t reg = offset >> 4;

    switch (reg)
    {
    	case 0: /* Timer Control and Status Register */
    		return s->control;
    	case 1: /* Timer Runing Count Register */
    		return ptimer_get_count(s->timer);
    	case 2: /* Timer Fixed Count Register */
    		return s->fixed_cnt;
    	case 3: /* Timer Match Count Register */
    		return s->match_cnt;
    	default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            return 0;
    }

    return 0;
}

static void imx28_timer_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
	//printf("\n+++++++++++++Timer Write %08x; %08x\n", (unsigned int) offset, (unsigned int) value);
	IMX28TimerState *s = (IMX28TimerState *) opaque;
    uint32_t reg = offset >> 4;
    int freq = -1, reloaded = -1;

    switch (reg)
    {
    	case 0: /* Timer Control and Status Register */

            if (s->control & TIMER_CTRL_SELECT_MASK)
            {
                /* Pause the timer if it is running.  This may cause some
                   inaccuracy dure to rounding, but avoids a whole lot of other
                   messyness.  */
                ptimer_stop(s->timer);
            }

            if (offset == 0x0)
            	s->control = value;
            if (offset == 0x4)
            	s->control |= value;
            if (offset == 0x8)
            	s->control &= ~value;
            if (offset == 0xC)
            	s->control ^= value;

            switch (s->control & TIMER_CTRL_SELECT_MASK)
            {
            	case 1: /* source of timer ticks is PWM0 */
            	case 2: /* source of timer ticks is PWM1 */
            	case 3: /* source of timer ticks is PWM2 */
            	case 4: /* source of timer ticks is PWM3 */
            	case 5: /* source of timer ticks is PWM4 */
            	case 6: /* source of timer ticks is PWM5 */
            	case 7: /* source of timer ticks is PWM6 */
            	case 8: /* source of timer ticks is PWM7 */
            	case 9: /* source of timer ticks is Rotary A */
            	case 10: /* source of timer ticks is Rotary B */
            		/* not supported */
            		freq = -1;
            		break;
            	case 11: /* source of timer ticks is 32kHz crystal*/
            		freq = 32000;
            		break;
            	case 12: /* source of timer ticks is 8kHz crystal*/
            		freq = 8000;
            		break;
            	case 13: /* source of timer ticks is 4kHz crystal*/
            		freq = 4000;
            		break;
            	case 14: /* source of timer ticks is 1kHz crystal*/
                    freq = 1000;
            		break;
                case 15: /* always ticks*/
                    freq = s->freq;
            		break;
                default:
                    freq = s->freq;
                	break;
            }

            switch ( (value >> TIMER_PRESACLER_SHIFT) & TIMER_PRESACLER_MASK )
            {
            	case 1: freq >>= 1; break;
            	case 2: freq >>= 2; break;
            	case 3: freq >>= 3; break;
            }

            ptimer_set_freq(s->timer, freq);


        	reloaded = (s->control >> TIMER_CTRL_ONESHOT_SHIFT) & TIMER_CTRL_ONESHOT_MASK;

        	if ( (s->control >> TIMER_CTRL_UPDATE_SHIFT) & TIMER_CTRL_UPDATE_MASK )
        	{
                ptimer_set_limit(s->timer, s->fixed_cnt , reloaded);
        		s->update = 0;
        	}
        	else
        	{
        		s->update = 1;
        	}


            s->int_level = (s->control >> TIMER_CTRL_IRQ_SHIFT) & TIMER_CTRL_IRQ_MASK;

            if (s->control & TIMER_CTRL_SELECT_MASK)
            {
                /* Restart the timer if still enabled.  */
                ptimer_run(s->timer, !reloaded);
            }

    		break;
    	case 1: /* Timer Runing Count Register */
    		/* Read only */
    		return;
    	case 2: /* Timer Fixed Count Register */
    		s->fixed_cnt = value;
    		break;
    	case 3: /* Timer Match Count Register */
    		s->match_cnt = value; /* not supported */
    		break;
    	default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            break;
    }
    imx28_timer_update(s);

}

static const MemoryRegionOps imx28_timer_ops = {
    .read = imx28_timer_read,
    .write = imx28_timer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_imx28_timer = {
    .name = "imx28_timer",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
    	VMSTATE_UINT32(control, IMX28TimerState),
    	VMSTATE_UINT32(fixed_cnt, IMX28TimerState),
    	VMSTATE_UINT32(match_cnt, IMX28TimerState),
    	VMSTATE_UINT32(int_level, IMX28TimerState),
    	VMSTATE_UINT32(update, IMX28TimerState),
        VMSTATE_END_OF_LIST()
    }
};

static int imx28_init(SysBusDevice *sbd)
{
	//printf("\nTimer Init\n");
    DeviceState *dev = DEVICE(sbd);
    IMX28TimerState *s = IMX28_TIMER(dev);
    QEMUBH *bh;

    sysbus_init_irq(sbd, &s->irq);
    memory_region_init_io(&s->iomem, OBJECT(s), &imx28_timer_ops, s, TYPE_IMX28_TIMER,
                          0x40);
    sysbus_init_mmio(sbd, &s->iomem);

    s->control = 0x0;
	s->update = 0;

    bh = qemu_bh_new(imx28_timer_tick, s);
    s->timer = ptimer_init(bh);

    return 0;
}

static Property imx28_properties[] = {
    DEFINE_PROP_UINT32("freq", IMX28TimerState, freq, 24000000),
    DEFINE_PROP_END_OF_LIST(),
};

static void imx28_timer_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);
    DeviceClass *k = DEVICE_CLASS(klass);

    sdc->init = imx28_init;
    k->props = imx28_properties;
}

static const TypeInfo imx28_timer_info = {
    .name          = TYPE_IMX28_TIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMX28TimerState),
    .class_init    = imx28_timer_class_init,
};

static void imx28_timer_register_types(void)
{
    type_register_static(&imx28_timer_info);
}

type_init(imx28_timer_register_types)

