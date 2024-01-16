/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/pcb-list-g.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <os/glucose.h>
#include <os/mm.h>
#include <os/malloc-g.h>
#include <os/time.h>
#include <os/ioremap.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/smp.h>
#include <plic.h>
#include <os/net.h>
#include <os/gfs.h>

extern void ret_from_exception();

// Task info array
task_info_t taskinfo[UTASK_MAX];
unsigned int tasknum;


static void init_jmptab(void)
{
	volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

	jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
	jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
	jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
	jmptab[SD_READ]         = (long (*)())sd_read;
	jmptab[SD_WRITE]        = (long (*)())sd_write;
	jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
	jmptab[SET_TIMER]       = (long (*)())set_timer;
	jmptab[READ_FDT]        = (long (*)())read_fdt;
	jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
	jmptab[PRINT]           = (long (*)())printk;
	jmptab[YIELD]           = (long (*)())do_scheduler;
	jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
	jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
	jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

	// TODO: [p2-task1] (S-core) initialize system call table.
	jmptab[WRITE]			= (long (*)())screen_write;
	jmptab[CLEAR]			= (long (*)())screen_clear;
	jmptab[REFLUSH]			= (long (*)())screen_reflush;

	/*jmptab[EXEC]			= (long (*)())do_exec;
	jmptab[EXIT]			= (long (*)())do_exit;*/
	jmptab[KILL]			= (long (*)())do_kill;
	/*jmptab[WAITPID]			= (long (*)())do_waitpid;*/
	jmptab[PS]				= (long (*)())do_process_show;
	/*jmptab[GETPID]			= (long (*)())do_getpid;
	jmptab[BARRIER_INIT]	= (long (*)())do_barrier_init;
	jmptab[BARRIER_WAIT]	= (long (*)())do_barrier_wait;
	jmptab[BARRIER_DESTROY]	= (long (*)())do_barrier_destroy;*/
}

static void init_taskinfo(void)
{
	// TODO: [p1-task4] Init 'tasks' array via reading app-info sector
	// NOTE: You need to get some related arguments from bootblock first
	uint32_t //imagesize = *(uint32_t *)(BOOTLOADER_ADDR + 256),	/* unit: sector */
		taskinfo_offset = *(uint32_t *)(BOOTLOADER_VADDR + TINFO_OFFSET);
	tasknum = *(uint32_t *)(BOOTLOADER_VADDR + TNUM_OFFSET);
	uint32_t taskinfo_size = tasknum * sizeof(task_info_t),
		taskinfo_start_sector, taskinfo_end_sector,
		taskinfo_sectors;
	
	/*
	 * A location to store taskinfo temporarily.
	 */
	const ptr_t Taskinfo_buffer = 0x52510000U;

	if (tasknum == 0U)
		return;
	taskinfo_start_sector = lbytes2sectors(taskinfo_offset);
	taskinfo_end_sector = lbytes2sectors(taskinfo_offset + taskinfo_size - 1U);
	taskinfo_sectors = taskinfo_end_sector - taskinfo_start_sector + 1U;
	if (tasknum > UTASK_MAX || taskinfo_sectors > TASKINFO_SECTORS_MAX)
		bios_putstr("**Warning: taskinfo is too big\n\r");

	/* Load taskinfo into the space of bootloader */
	bios_sd_read(Taskinfo_buffer, taskinfo_sectors, taskinfo_start_sector);

	memcpy((uint8_t*)taskinfo,
		(uint8_t *)pa2kva(
			(Taskinfo_buffer + (taskinfo_offset - taskinfo_start_sector * SECTOR_SIZE))
		), taskinfo_size);
}

/* NOTE: static function "init_pcb_stack" has been moved to sched.c */

static void init_pcb(void)
{
	/* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
	/* TODO: [p2-task1] remember to initialize 'current_running' */
	//pcb_t *temp;

	ready_queue = NULL;
	//ready_queue = lpcb_add_node_to_tail(ready_queue, &current_running[0], &ready_queue);
	ready_queue = lpcb_insert_node(ready_queue, &pid0_pcb, NULL, &ready_queue);
	current_running[0] = &pid0_pcb;
#if NCPU == 2
	//ready_queue = lpcb_add_node_to_tail(ready_queue, &current_running[1], &ready_queue);
	ready_queue = lpcb_insert_node(ready_queue, &pid1_pcb, NULL, &ready_queue);
	current_running[1] = &pid1_pcb;
#endif
	/*
	 * NOTE: current_running->phead is ensured in definition of pid0_pcb
	 */
#if NCPU == 2
	if (ready_queue == NULL || ready_queue->next == ready_queue)
#else
	if (ready_queue == NULL)
#endif
		panic_g("Failed to init ready_queue");
	if (pcb_table_add(current_running[0]) < 0 ||
#if NCPU == 2
		pcb_table_add(current_running[1]) < 0)
#else
		0)
#endif
		panic_g("Failed to add 0 to pcb_table");
}

static void init_syscall(void)
{
	// TODO: [p2-task3] initialize system call table.
	int i;

	for (i = 0; i < NUM_SYSCALLS; ++i)
		syscall[i] = (long (*)())invalid_syscall;

	/* Process and scheduler */ {
		syscall[SYS_exec] = (long (*)())do_exec;
		syscall[SYS_kill] = (long (*)())do_kill;
		syscall[SYS_exit] = (long (*)())do_exit;
		syscall[SYS_waitpid] = (long (*)())do_waitpid;
		syscall[SYS_getpid] = (long (*)())do_getpid;
		syscall[SYS_sleep] = (long (*)())do_sleep;
		syscall[SYS_yield] = (long (*)())do_scheduler;
		syscall[SYS_ps] = (long (*)())do_process_show;
		syscall[SYS_taskset] = (long (*)())do_taskset;
		syscall[SYS_fork] = (long (*)())do_fork;
#if MULTITHREADING != 0
		syscall[SYS_thread_create] = (long (*)())do_thread_create;
		syscall[SYS_thread_wait] = (long (*)())do_thread_wait;
		syscall[SYS_thread_exit] = (long (*)())do_thread_exit;
#endif
	}
	/* IO: Screen, clock and log */ {
		syscall[SYS_screen_write] = (long (*)())do_screen_write;
		syscall[SYS_bios_getchar] = (long (*)())bios_getchar;
		syscall[SYS_move_cursor] = (long (*)())screen_move_cursor;
		syscall[SYS_reflush] = (long (*)())screen_reflush;
		syscall[SYS_clear] = (long (*)())screen_clear;
		syscall[SYS_rclear] = (long (*)())screen_rclear;
		syscall[SYS_set_cylim] = (long (*)())screen_set_cylim;
		syscall[SYS_ulog] = (long (*)())do_ulog;
		syscall[SYS_kprint_avail_table] = (long (*)())kprint_avail_table;
		syscall[SYS_get_timebase] = (long(*)())get_time_base;
		syscall[SYS_get_tick] = (long (*)())get_ticks;
	}
	/* Sync: mutex lock, semaphore and barrier */ {
		syscall[SYS_mlock_init] = (long (*)())do_mutex_lock_init;
		syscall[SYS_mlock_acquire] = (long (*)())do_mutex_lock_acquire;
		syscall[SYS_mlock_release] = (long (*)())do_mutex_lock_release;
		syscall[SYS_sema_init] = (long (*)())do_semaphore_init;
		syscall[SYS_sema_up] = (long (*)())do_semaphore_up;
		syscall[SYS_sema_down] = (long (*)())do_semaphore_down;
		syscall[SYS_sema_destroy] = (long (*)())do_semaphore_destroy;
		syscall[SYS_barr_init] = (long (*)())do_barrier_init;
		syscall[SYS_barr_wait] = (long (*)())do_barrier_wait;
		syscall[SYS_barr_destroy] = (long (*)())do_barrier_destroy;
	}
	/* Communication: mailbox */ {
		syscall[SYS_mbox_open] = (long (*)())do_mbox_open;
		syscall[SYS_mbox_close] = (long (*)())do_mbox_close;
		syscall[SYS_mbox_send] = (long (*)())do_mbox_send;
		syscall[SYS_mbox_recv] = (long (*)())do_mbox_recv;
	}
	/* Memory management */ {
		syscall[SYS_sbrk] = (long (*)())do_sbrk;
		syscall[SYS_shm_get] = (long (*)())shm_page_get;
		syscall[SYS_shm_dt] = (long (*)())do_shm_page_dt;
	}
#if NIC != 0
	/* IO: NIC */ {
		syscall[SYS_net_send] = (long (*)())do_net_send;
		syscall[SYS_net_recv] = (long (*)())do_net_recv;
		syscall[SYS_net_send_array] = (long (*)())do_net_send_array;
		syscall[SYS_net_recv_stream] = (long (*)())do_net_recv_stream;
	}
#endif
	/* File system */ {
		syscall[SYS_mkfs] = (long (*)())do_mkfs;
		syscall[SYS_fsinfo] = (long (*)())do_fsinfo;
		syscall[SYS_open] = (long (*)())do_open;
		syscall[SYS_close] = (long (*)())do_close;
		syscall[SYS_changedir] = (long (*)())do_changedir;
		syscall[SYS_read] = (long (*)())do_read;
		syscall[SYS_write] = (long (*)())do_write;
		syscall[SYS_lseek] = (long (*)())do_lseek;
		syscall[SYS_getpath] = (long (*)())do_getpath;
		syscall[SYS_readdir] = (long (*)())do_readdir;
		syscall[SYS_mkdir] = (long (*)())do_mkdir;
		syscall[SYS_rm] = (long (*)())do_remove;
		syscall[SYS_hlink] = (long (*)())do_hlink;
	}
}

/*
 * Revoke the temporary mapping from
 * 0x50000000 to 0x51000000.
 */
static void revoke_temp_mapping(void)
{
	uintptr_t va = 0x51000000UL;
	va &= VA_MASK;
	uint64_t vpn2 =
		va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
	((PTE*)PGDIR_VA)[vpn2] = 0UL;
	local_flush_tlb_all();
}

pid_t start_glush(void)
{
#ifndef TERMINAL_BEGIN
#define TERMINAL_BEGIN "14"
#endif
#ifndef TERMINAL_END
#define TERMINAL_END "27"
#endif

	char *argv[] = {"glush", TERMINAL_BEGIN, TERMINAL_END,
#if DEBUG_EN != 0
		"1",	/* Autolog for glush */
#else
		"0",
#endif
		NULL};
	pid_glush = do_exec("glush", -1, argv);
	if (pid_glush == INVALID_PID)
		return INVALID_PID;
	if (do_taskset(0, NULL, pid_glush, ~0U) != pid_glush)
		panic_g("Failed to set `cpu_mask` of glush %d", pid_glush);
	return pid_glush;
}

/************************************************************/
/*
 * a0, a1 are arguments passed from boot_kernel() and _start.
 * Now, a0 is CPU ID, and a1 is `npages_used` in boot.c.
 */
#pragma GCC diagnostic ignored "-Wmain"
int main(reg_t a0, reg_t a1)
{
	int cgfs_rt;

	// Init jump table provided by kernel and bios(ΦωΦ)
	init_jmptab();

	// Init task information (〃'▽'〃)
	init_taskinfo();

	// Init memory management
	init_mm();
	
	// Init Process Control Blocks |•'-'•) ✧
	init_pcb();
	printk("> [INIT] PCB initialization succeeded.\n");

	// Init screen (QAQ)
	init_screen();
	printk("> [INIT] SCREEN initialization succeeded.\n");

	// Read CPU frequency (｡•ᴗ-)_
	time_base = bios_read_fdt(TIMEBASE);
	time_max_sec = UINT64_MAX / time_base;

#if NIC != 0
	uint64_t plic_addr;
	uint32_t nr_irqs;

    e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
    plic_addr = bios_read_fdt(PLIC_ADDR);
    nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
    printk("> [INIT] e1000: 0x%lx, plic_addr: 0x%lx, nr_irqs: 0x%lx.\n",
		e1000, plic_addr, nr_irqs);

    // IOremap
    plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
    e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
	if (plic_addr != 0UL && e1000 != NULL)
    	printk("> [INIT] IOremap initialization succeeded.\n");
	else
		panic_g("IOremap failed: 0x%lx, 0x%lx", plic_addr, (uint64_t)e1000);
#endif

	// Init lock mechanism o(´^｀)o
	init_locks();
	printk("> [INIT] Lock mechanism initialization succeeded.\n");

	// Init interrupt (^_^)
	init_exception();
	printk("> [INIT] Interrupt processing initialization succeeded.\n");

#if NIC != 0
    // TODO: [p5-task3] Init plic
	plic_init(plic_addr, nr_irqs);
	printk("> [INIT] PLIC initialized successfully.\n");

    // Init network device
    e1000_init();
    printk("> [INIT] E1000 device initialized successfully.\n");
#endif
	// Init system call table (0_0)
	init_syscall();
	printk("> [INIT] System call initialized successfully.\n");

	printk("> [INIT] GlucOS built at %s, %s.\n", __DATE__, __TIME__);

	writelog("=============================\n"
		"          GlucOS, boot! I am CPU %lu.", get_current_cpu_id());
	writelog("Multithreading: %d, Timer_interval_ms: %d,\n"
		"          NCPU: %d, NPF: %u, NPSWAP: %u, DEBUG_EN: %d,\n"
		"          USEG_MAX: %u MiB, USTACK_NPG: %u,\n"
		"          Built at %s, %s",
		MULTITHREADING, TIMER_INTERVAL_MS, NCPU, NPF, NPSWAP, DEBUG_EN,
		(unsigned)(USEG_MAX >> 20), USTACK_NPG, __DATE__, __TIME__);
#if DEBUG_EN != 0
	printk("\n> [INFO] Debug mode is enabled.\n");
	printk("> [INFO] Multithreading: %d, Timer_interval_ms: %d,\n"
		"         NCPU: %d, NPF: %u, NPSWAP: %u,\n"
		"         USEG_MAX: %u MiB, USTACK_NPG: %u\n",
		MULTITHREADING, TIMER_INTERVAL_MS, NCPU, NPF, NPSWAP,
		(unsigned)(USEG_MAX >> 20), USTACK_NPG);
#endif

	// Init Glucose file system
	if ((cgfs_rt = GFS_check()) < 0)
	{
		GFS_init();
		printk("> [INIT] GFS initialized successfully.\n");
	}
	else if (cgfs_rt > 0)
		printk("> [INIT] An invalid GFS (%d) is found.\n", cgfs_rt);
	if (cgfs_rt >= 0)
		/*
		 * `flist_init()` is called in `GFS_init()`, so if
		 * `cgfs_rt < 0`, calling it again is unnecessary.
		 */
		GFS_mount();


	printk("> [INFO] Press any key to keep these information...\n");

#if NCPU == 2
	smp_init();
	wakeup_other_hart();
	local_flush_tlb_all();
#else
	if (a1 != 3UL)
		panic_g("`npages_used` from boot.c is %lu", a1);
	revoke_temp_mapping();
#endif
	/*
	 * Delay 3 s to keep information on the screen
	 * and wait for another CPU to start.
	 * NOTE: CPU0 cannot start a user process before
	 * the other CPU enters kernel, because the temporary
	 * page table (the 3rd page table) will be used to
	 * start the first process.
	 */
	latency(3U);
	if (bios_getchar() != NOI)
	{
		printk("> [INFO] Press any key to continue...\n");
		while (bios_getchar() == NOI)
			;
	}

	/*
	 * Lock kernel to protect do_exec(glush)
	 * from being affected by another CPU
	 */
	lock_kernel();
	/* Clear screen and start glush */
	screen_clear();

	if ((pid_glush = start_glush()) == INVALID_PID)
		panic_g("Failed to start glush");
	unlock_kernel();

	set_preempt();
#if DEBUG_EN != 0
	writelog("CPU 0: Timer interrupt is enabled");
#endif

	while (1)
		// Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
		__asm__ volatile("wfi");
	return 0;
}

/* For secondary CPU */
int main_s(reg_t a0, reg_t a1)
{
	pcb_t *p0;

#if NCPU != 2
	panic_g("Only %d CPU but main_s() is entered", NCPU);
#endif
	if (a1 != 3UL)
		panic_g("npages_used from boot.c is %lu", a1);
	revoke_temp_mapping();

	init_exception_s();

	/* Print below the string CPU 0 has printed */
	p0 = pcb_search(0);
	screen_move_cursor(0, p0->cursor_y + 1);

	writelog("I am CPU %lu and has started!", get_current_cpu_id());
	printk("> [INIT] I am CPU %lu and has started!\n", get_current_cpu_id());

	set_preempt();
#if DEBUG_EN != 0
	writelog("CPU %lu: Timer interrupt is enabled", get_current_cpu_id());
#endif
	while (1)
		asm volatile("wfi");
	return 0;
}