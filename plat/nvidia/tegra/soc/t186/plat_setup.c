/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arch_helpers.h>
#include <assert.h>
#include <bl_common.h>
#include <console.h>
#include <context.h>
#include <context_mgmt.h>
#include <debug.h>
#include <denver.h>
#include <interrupt_mgmt.h>
#include <platform.h>
#include <tegra_def.h>
#include <tegra_private.h>
#include <xlat_tables.h>

/*******************************************************************************
 * The Tegra power domain tree has a single system level power domain i.e. a
 * single root node. The first entry in the power domain descriptor specifies
 * the number of power domains at the highest power level.
 *******************************************************************************
 */
const unsigned char tegra_power_domain_tree_desc[] = {
	/* No of root nodes */
	1,
	/* No of clusters */
	PLATFORM_CLUSTER_COUNT,
	/* No of CPU cores - cluster0 */
	PLATFORM_MAX_CPUS_PER_CLUSTER,
	/* No of CPU cores - cluster1 */
	PLATFORM_MAX_CPUS_PER_CLUSTER
};

/*
 * Table of regions to map using the MMU.
 */
static const mmap_region_t tegra_mmap[] = {
	MAP_REGION_FLAT(TEGRA_MISC_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_MC_STREAMID_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_MC_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_UARTA_BASE, 0x20000, /* 128KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_GICD_BASE, 0x20000, /* 128KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_SE0_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_PKA1_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_RNG1_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_CAR_RESET_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_PMC_BASE, 0x40000, /* 256KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_SCRATCH_BASE, 0x10000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_MMCRAB_BASE, 0x60000, /* 384KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	MAP_REGION_FLAT(TEGRA_SMMU_BASE, 0x1000000, /* 64KB */
			MT_DEVICE | MT_RW | MT_SECURE),
	{0}
};

/*******************************************************************************
 * Set up the pagetables as per the platform memory map & initialize the MMU
 ******************************************************************************/
const mmap_region_t *plat_get_mmio_map(void)
{
	/* MMIO space */
	return tegra_mmap;
}

/*******************************************************************************
 * Handler to get the System Counter Frequency
 ******************************************************************************/
unsigned int plat_get_syscnt_freq2(void)
{
	return 31250000;
}

/*******************************************************************************
 * Maximum supported UART controllers
 ******************************************************************************/
#define TEGRA186_MAX_UART_PORTS		7

/*******************************************************************************
 * This variable holds the UART port base addresses
 ******************************************************************************/
static uint32_t tegra186_uart_addresses[TEGRA186_MAX_UART_PORTS + 1] = {
	0,	/* undefined - treated as an error case */
	TEGRA_UARTA_BASE,
	TEGRA_UARTB_BASE,
	TEGRA_UARTC_BASE,
	TEGRA_UARTD_BASE,
	TEGRA_UARTE_BASE,
	TEGRA_UARTF_BASE,
	TEGRA_UARTG_BASE,
};

/*******************************************************************************
 * Retrieve the UART controller base to be used as the console
 ******************************************************************************/
uint32_t plat_get_console_from_id(int id)
{
	if (id > TEGRA186_MAX_UART_PORTS)
		return 0;

	return tegra186_uart_addresses[id];
}

/* Secure IRQs for Tegra186 */
static const irq_sec_cfg_t tegra186_sec_irqs[] = {
	{
		TEGRA186_TOP_WDT_IRQ,
		TEGRA186_SEC_IRQ_TARGET_MASK,
		INTR_TYPE_EL3,
	},
	{
		TEGRA186_AON_WDT_IRQ,
		TEGRA186_SEC_IRQ_TARGET_MASK,
		INTR_TYPE_EL3,
	},
};

/*******************************************************************************
 * Initialize the GIC and SGIs
 ******************************************************************************/
void plat_gic_setup(void)
{
	tegra_gic_setup(tegra186_sec_irqs,
		sizeof(tegra186_sec_irqs) / sizeof(tegra186_sec_irqs[0]));

	/*
	 * Initialize the FIQ handler only if the platform supports any
	 * FIQ interrupt sources.
	 */
	if (sizeof(tegra186_sec_irqs) > 0)
		tegra_fiq_handler_setup();
}
