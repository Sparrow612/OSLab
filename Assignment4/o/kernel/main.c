
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"


int strategy;

PRIVATE void init_tasks()
{
	clean(console_table);
	init_screen(tty_table);
	
	proc_table[0].ticks = proc_table[0].priority = 1;
	proc_table[1].ticks = proc_table[1].priority = TIME_SLICE;
	proc_table[2].ticks = proc_table[2].priority = TIME_SLICE;
	proc_table[3].ticks = proc_table[3].priority = TIME_SLICE;
	proc_table[4].ticks = proc_table[4].priority = TIME_SLICE;
	proc_table[5].ticks = proc_table[5].priority = TIME_SLICE;
	proc_table[6].ticks = proc_table[6].priority = TIME_SLICE;

	k_reenter = 0;
	ticks = 0;
	readers = 0;
	writters = 0;
	true_rc = 0;

	strategy = 0;

	p_proc_ready = proc_table;
}
/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	disp_str("-----\"kernel_main\" begins-----\n");

	TASK*		p_task		= task_table;
	PROCESS*	p_proc		= proc_table;
	char*		p_task_stack	= task_stack + STACK_SIZE_TOTAL;
	u16		selector_ldt	= SELECTOR_LDT_FIRST;
	int i;
    u8              privilege;
    u8              rpl;
    int             eflags;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
        if (i < NR_TASKS) {     /* 任务 */
                        p_task    = task_table + i;
                        privilege = PRIVILEGE_TASK;
                        rpl       = RPL_TASK;
                        eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
                }
                else {                  /* 用户进程 */
                        p_task    = user_proc_table + (i - NR_TASKS);
                        privilege = PRIVILEGE_USER;
                        rpl       = RPL_USER;
                        eflags    = 0x202; /* IF=1, bit 2 is always 1 */
                }
                
		strcpy(p_proc->p_name, p_task->name);	// name of the process
		p_proc->pid = i;			// pid
		p_proc->sleeping = 0; // 初始化结构体新增成员
		p_proc->blocked = 0;
		
		p_proc->ldt_sel = selector_ldt;

		memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[0].attr1 = DA_C | privilege << 5;
		memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
		p_proc->regs.cs	= (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ds	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.es	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.fs	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ss	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

		p_proc->regs.eip = (u32)p_task->initial_eip;
		p_proc->regs.esp = (u32)p_task_stack;
		p_proc->regs.eflags = eflags;

		p_proc->nr_tty = 0;

		p_task_stack -= p_task->stacksize;
		p_proc++;
		p_task++;
		selector_ldt += 1 << 3;
	}

	init_tasks();

	init_clock();
    init_keyboard();

	restart();

	while(1){}
}

PRIVATE read_proc(char proc, int slices, int color){
	printf("%c%c R start", color, proc);

	printf("%c%c in R", color, proc);
	sleep_ms(slices * TIME_SLICE); // 读耗时slices个时间片
	
	printf("%c%c R end", color, proc);
}

PRIVATE	write_proc(char proc, int slices, int color){
	printf("%c%c W start", color, proc);

	printf("%c%c in W", color, proc);
	sleep_ms(slices * TIME_SLICE); // 写耗时slices个时间片
	
	printf("%c%c W end", color, proc);
}

// 读写公平方案
void read_v0(char proc, int slices, int color){
	while (1)
	{
		P(&queue);
		P(&r_mutex); // 每个读者原子地访问readers变量
		if (readers==0)
			P(&rw_mutex); // 有读者，禁止写
		readers++;
		V(&r_mutex);
		V(&queue);

		P(&n_r_mutex); // 读有人数限制
		// 读过程
		true_rc++;
		read_proc(proc, slices, color);
		true_rc--;
		V(&n_r_mutex);
		
		P(&r_mutex);
		readers--;
		if (readers==0)
			V(&rw_mutex); // 没有读者，可以开始写了
		V(&r_mutex);
		sleep_ms(slices*TIME_SLICE);
	}	
}

void write_v0(char proc, int slices, int color){
	while (1) 
	{	
		P(&queue);
		P(&rw_mutex);
		writters++;
		V(&queue);
		// 写过程
		write_proc(proc, slices, color);
		
		writters--;
		V(&rw_mutex);
		sleep_ms(slices*TIME_SLICE);
	}
}
// 读者优先
void read_v1(char proc, int slices, int color){
	while (1)
	{
		P(&r_mutex); // 每个读者原子地访问readers变量
		if (readers==0)
			P(&rw_mutex); // 有读者，禁止写
		readers++;
		V(&r_mutex);
		
		P(&n_r_mutex); // 读有人数限制
		// 读过程
		true_rc++;
		read_proc(proc, slices, color);
		true_rc--;
		V(&n_r_mutex);

		P(&r_mutex);
		readers--;
		if (readers==0)
			V(&rw_mutex); // 没有读者，可以开始写了
		V(&r_mutex);
		sleep_ms(slices*TIME_SLICE);
	}	
}

void write_v1(char proc, int slices, int color){
	while (1) 
	{
		
		P(&rw_mutex);
		writters++;
		// 写过程
		write_proc(proc, slices, color);
		writters--;
		V(&rw_mutex);

		sleep_ms(slices*TIME_SLICE);
	}
}

// 写者优先
void read_v2(char proc, int slices, int color){
	while (1)
	{
		P(&queue);
		P(&r_mutex); // 每个读者原子地访问readers变量
		if (readers==0)
			P(&rw_mutex); // 有读者，禁止写
		readers++;
		V(&r_mutex);
		V(&queue);
		
		P(&n_r_mutex); // 读有人数限制
		// 读过程
		true_rc++;
		read_proc(proc, slices, color);
		true_rc--;
		V(&n_r_mutex);

		P(&r_mutex);
		readers--;
		if (readers==0)
			V(&rw_mutex); // 没有读者，可以开始写了
		V(&r_mutex);
		sleep_ms(slices*TIME_SLICE);
	}
}

void write_v2(char proc, int slices, int color){
	while (1) 
	{
		P(&w_mutex); // 写者互斥地访问
		// 写过程
		if (writters==0)
			P(&queue); // 第一个写者抢到资源就不放手，从而实现写者优先
		writters++;
		V(&w_mutex);

		P(&rw_mutex); // 写者互斥地写
		
		write_proc(proc, slices, color);

		V(&rw_mutex);

		P(&w_mutex);
		writters--;
		if (writters==0)
			V(&queue);
		V(&w_mutex);
		sleep_ms(slices*TIME_SLICE);
	}
}

read_f read_funcs[3] = {read_v0, read_v1, read_v2};
write_f write_funcs[3] = {write_v0, write_v1, write_v2};

/*======================================================================*
                               ReaderA
 *======================================================================*/
void ReaderA()
{
	read_funcs[strategy]('A', 2, '\01');
}

/*======================================================================*
                               ReaderB
 *======================================================================*/
void ReaderB()
{
	read_funcs[strategy]('B', 3, '\02');
}

/*======================================================================*
                               ReaderC
 *======================================================================*/
void ReaderC()
{
	read_funcs[strategy]('C', 3, '\03');
}

/*======================================================================*
                               WriterD
 *======================================================================*/
void WritterD()
{
	write_funcs[strategy]('D', 3, '\04');
}

/*======================================================================*
                               WriterE
 *======================================================================*/
void WritterE()
{
	write_funcs[strategy]('E', 4, '\05');
}

/*======================================================================*
                               ReporterF
 *======================================================================*/
void ReporterF()
{
	char color = '\06';
	while (1) {
        if (readers > 0 && true_rc >0 ){
            printf("%c%c reading", color, true_rc);
        } else if (writters > 0){
            printf("%cWritting", color);
        } else {
            printf("%cboth not", color);
        }
        sleep_ms(TIME_SLICE);
    }	
}