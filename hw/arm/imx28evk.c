/*
 * imx28evk.c
 *
 *  Created on: 23.06.2014
 *      Author: Schoenfelder
 */

#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/devices.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/i2c/i2c.h"
#include "sysemu/blockdev.h"
#include "exec/address-spaces.h"

static struct arm_boot_info imx28evk_binfo;

#define GIC_BASE_ADDR 0x80000000

static void imx28evk_init(QEMUMachineInitArgs *args)
{
    ARMCPU *cpu = NULL;
    const char *cpu_model = "arm926";

    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *sram_alias = g_new(MemoryRegion, 1);
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *rom = g_new(MemoryRegion, 1);
    ram_addr_t sram_size, rom_size;
    ram_addr_t ram_size = args->ram_size;

    DeviceState *dev, *digcntr, *dflpt;

    cpu = cpu_arm_init(cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    /* 128kB On-Chip SRAM */
    sram_size = 0x00020000;
    memory_region_init_ram(sram, NULL, "imx28.sram", sram_size);
    vmstate_register_ram_global(sram);
    memory_region_add_subregion(sysmem, 0, sram);

    /* external DRAM */
    if (ram_size > 0x40000000) {
        /* 1GB is the maximum the address space permits */
        fprintf(stderr, "i.MX28: cannot model more than 1GB RAM\n");
        exit(1);
    }

    memory_region_init_ram(ram, NULL, "imx28.ram", args->ram_size);
    vmstate_register_ram_global(ram);
    memory_region_add_subregion(sysmem, 0x40000000, ram);

    /* 128kB On-Chip ROM */
    rom_size = 0x00020000;
    memory_region_init_ram(rom, NULL, "imx28.rom", rom_size);
    vmstate_register_ram_global(rom);
    memory_region_set_readonly(rom, true);
    memory_region_add_subregion(sysmem, 0xC0000000, rom);

    /* On-Chip SRAM alias */
    sram_size = 0x00020000;
    memory_region_init_alias(sram_alias, NULL, "imx28_sram.alias",
   		                 sram, 0, sram_size);
    memory_region_add_subregion(sysmem, 0x00020000, sram_alias);

    /* Digital Control Block */
    digcntr = qdev_create(NULL, "imx28_digcntr");
    qdev_init_nofail(digcntr);
    sysbus_mmio_map(SYS_BUS_DEVICE(digcntr), 0, 0x8001C000);

    /*  DFLPT */
    dflpt = qdev_create(NULL, "imx28_dflpt");
    qdev_init_nofail(dflpt);
    sysbus_mmio_map(SYS_BUS_DEVICE(dflpt), 0, 0x800C0000);

    /* Interrupt Controller */
    dev = sysbus_create_varargs("imx28_gic", 0x80000000,
            qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ),
            qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_FIQ),
            NULL);

    /* Rotary Decoder*/
    sysbus_create_simple("imx28_rotary_decoder", 0x80068000, NULL);
    /* Timer 0 */
    sysbus_create_simple("imx28_timer", 0x80068020, qdev_get_gpio_in(dev, 48));
    /* Timer 1 */
	sysbus_create_simple("imx28_timer", 0x80068060, qdev_get_gpio_in(dev, 49));
	 /* Timer 2 */
	sysbus_create_simple("imx28_timer", 0x800680A0, qdev_get_gpio_in(dev, 50));
	 /* Timer 3 */
	sysbus_create_simple("imx28_timer", 0x800680E0, qdev_get_gpio_in(dev, 51));

    /* DEBUG UART */
    sysbus_create_simple("pl011", 0x80074000, qdev_get_gpio_in(dev, 47));

    /* Application UART*/
    sysbus_create_simple("pl011", 0x8006A000, qdev_get_gpio_in(dev, 70));
    sysbus_create_simple("pl011", 0x8006A000, qdev_get_gpio_in(dev, 71));
    sysbus_create_simple("pl011", 0x8006C000, qdev_get_gpio_in(dev, 113));
    sysbus_create_simple("pl011", 0x8006E000, qdev_get_gpio_in(dev, 114));
    sysbus_create_simple("pl011", 0x80070000, qdev_get_gpio_in(dev, 115));
    sysbus_create_simple("pl011", 0x80072000, qdev_get_gpio_in(dev, 116));

    imx28evk_binfo.ram_size = args->ram_size;
    imx28evk_binfo.nb_cpus = 1;
    imx28evk_binfo.kernel_filename = args->kernel_filename;
    imx28evk_binfo.kernel_cmdline = args->kernel_cmdline;
    imx28evk_binfo.initrd_filename = args->initrd_filename;
    imx28evk_binfo.board_id = 0xCAFECAFE;
    arm_load_kernel(cpu, &imx28evk_binfo);
}

static QEMUMachine imx28evk_devel_board = {
    .name = "imx28evk",
    .desc = "i.MX28 EVK (ARM926EJ-S)",
    .init = imx28evk_init,
    .block_default_type = IF_SCSI,
};

static void imx28evk_machine_init(void)
{
    qemu_register_machine(&imx28evk_devel_board);
}

machine_init(imx28evk_machine_init);
