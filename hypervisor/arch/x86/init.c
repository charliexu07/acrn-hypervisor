/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <init.h>
#include <hypervisor.h>
#include <schedule.h>

/* Push sp magic to top of stack for call trace */
#define SWITCH_TO(rsp, to)                                              \
{                                                                       \
	asm volatile ("movq %0, %%rsp\n"                                \
			"pushq %1\n"                                    \
			"call *%2\n"                                    \
			 :                                              \
			 : "r"(rsp), "rm"(SP_BOTTOM_MAGIC), "a"(to));   \
}

/*TODO: move into debug module */
static void init_debug_pre(void)
{
	/* Initialize console */
	console_init();

	/* Enable logging */
	init_logmsg(CONFIG_LOG_DESTINATION);
}

/*TODO: move into debug module */
static void init_debug_post(uint16_t pcpu_id)
{
	if (pcpu_id == BOOT_CPU_ID) {
		/* Initialize the shell */
		shell_init();
		console_setup_timer();
	}

	profiling_setup();
}

/*TODO: move into pass-thru module */
static void init_passthru(void)
{
	if (init_iommu() != 0) {
		panic("failed to initialize iommu!");
	}

	ptdev_init();
}

/*TODO: move into guest-vcpu module */
static void init_guest(void)
{
	init_scheduler();
}

/*TODO: move into guest-vcpu module */
static void enter_guest_mode(uint16_t pcpu_id)
{
	vmx_on();

#ifdef CONFIG_PARTITION_MODE
	(void)prepare_vm(pcpu_id);
#else
	if (pcpu_id == BOOT_CPU_ID) {
		(void)prepare_vm(pcpu_id);
	}
#endif

	default_idle();

	/* Control should not come here */
	cpu_dead();
}

static void init_primary_cpu_post(void)
{
	/* Perform any necessary BSP initialization */
	init_bsp();

	init_debug_pre();

	init_guest();

	init_cpu_post(BOOT_CPU_ID);

	init_debug_post(BOOT_CPU_ID);

	init_passthru();

	enter_guest_mode(BOOT_CPU_ID);
}

/* NOTE: this function is using temp stack, and after SWITCH_TO(runtime_sp, to)
 * it will switch to runtime stack.
 */
void init_primary_cpu(void)
{
	uint64_t rsp;

	init_cpu_pre(BOOT_CPU_ID);

	/* Switch to run-time stack */
	rsp = (uint64_t)(&get_cpu_var(stack)[CONFIG_STACK_SIZE - 1]);
	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	SWITCH_TO(rsp, init_primary_cpu_post);
}

void init_secondary_cpu(void)
{
	uint16_t pcpu_id;

	init_cpu_pre(INVALID_CPU_ID);

	pcpu_id = get_cpu_id();

	init_cpu_post(pcpu_id);

	init_debug_post(pcpu_id);

	enter_guest_mode(pcpu_id);
}