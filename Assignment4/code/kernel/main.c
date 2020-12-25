
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
int tr;

PRIVATE void init_tasks()
{
	init_screen(tty_table);

	clean(console_table);
	
	proc_table[0].ticks = proc_table[0].priority = 1;
	proc_table[1].ticks = proc_table[1].priority = 1;
	proc_table[2].ticks = proc_table[2].priority = 1;
	proc_table[3].ticks = proc_table[3].priority = 1;
	proc_table[4].ticks = proc_table[4].priority = 1;
	proc_table[5].ticks = proc_table[5].priority = 1;
	proc_table[6].ticks = proc_table[6].priority = 1;

	k_reenter = 0;
	ticks = 0;
	readers = 0;
	writters = 0;
	writting = 0;

        tr = 0;

	strategy = 1; // 切换策略

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

PRIVATE read_proc(char proc, int slices, char color){
	printf("%c%c starts reading\n", color, proc);

	printf("%c%c is reading\n", color, proc);
	sleep_ms(slices * TIME_SLICE); // 读耗时slices个时间片
	
	printf("%c%c finishes reading\n", color, proc);
}

PRIVATE	write_proc(char proc, int slices, char color){
	printf("%c%c starts writting\n", color, proc);

	printf("%c%c is writting\n", color, proc);
	sleep_ms(slices * TIME_SLICE); // 写耗时slices个时间片
	
	printf("%c%c finishes writting\n", color, proc);
}

//读写公平方案
void read_v0(char proc, int slices, char color){
	printf("%c%c arrives\n", color, proc);
	P(&queue);

	P(&r_mutex); // 每个读者原子地访问readers变量
	if (readers==0)
		P(&rw_mutex); // 有读者，禁止写
	readers++;
	V(&r_mutex);
	V(&queue);

	P(&n_r_mutex);
	
	P(&r_mutex);
	tr++;
	V(&r_mutex);
	
	read_proc(proc, slices, color);
	
	P(&r_mutex);
	tr--;
	V(&r_mutex);
	
	V(&n_r_mutex);

	P(&r_mutex);
	readers--;
	if (readers==0)
		V(&rw_mutex); // 没有读者，可以开始写了
	V(&r_mutex);
}

void write_v0(char proc, int slices, char color){
	printf("%c%c arrives\n", color, proc);
	P(&queue);
	P(&rw_mutex);
	writting = 1;
	V(&queue);
	// 写过程
	write_proc(proc, slices, color);
	writting = 0;
	V(&rw_mutex);
}
// 读者优先
void read_v1(char proc, int slices, char color){
	printf("%c%c arrives\n", color, proc);	
		
	P(&r_mutex); // 每个读者原子地访问readers变量
	if (readers==0)
		P(&rw_mutex); // 有读者，禁止写
	readers++;
	V(&r_mutex);
	// 读过程
	
	P(&n_r_mutex);
	
	P(&r_mutex);
	tr++;
	V(&r_mutex);
	
	read_proc(proc, slices, color);
	
	P(&r_mutex);
	tr--;
	V(&r_mutex);
	
	V(&n_r_mutex);

	P(&r_mutex);
	readers--;
	if (readers==0)
		V(&rw_mutex); // 没有读者，可以开始写了
	V(&r_mutex);

}

void write_v1(char proc, int slices, char color){
	printf("%c%c arrives\n", color, proc);
	P(&rw_mutex);
	writting = 1;
	// 写过程
	write_proc(proc, slices, color);
	writting = 0;
	V(&rw_mutex);
}

// 写者优先
void read_v2(char proc, int slices, char color){
	printf("%c%c arrives\n", color, proc);
	P(&queue);
	P(&r_mutex); // 每个读者原子地访问readers变量
	if (readers==0)
		P(&rw_mutex); // 有读者，禁止写
	readers++;
	V(&r_mutex);
	V(&queue);
	//读过程开始
	P(&n_r_mutex);
	
	P(&r_mutex);
	tr++;
	V(&r_mutex);
	
	read_proc(proc, slices, color);
	
	P(&r_mutex);
	tr--;
	V(&r_mutex);
	
	V(&n_r_mutex);

	P(&r_mutex);
	readers--;
	if (readers==0)
		V(&rw_mutex); // 没有读者，可以开始写了
	V(&r_mutex);
}

void write_v2(char proc, int slices, char color){
	printf("%c%c arrives\n", color, proc);	
	P(&w_mutex); // 写者互斥地访问
	// 写过程
	if (writters==0)
		P(&queue); // 第一个写者抢到资源就不放手，从而实现写者优先
	writters++; 
	V(&w_mutex);

	P(&rw_mutex); // 写者互斥地写
	writting = 1;
	write_proc(proc, slices, color);
	writting = 0;
	V(&rw_mutex);

	P(&w_mutex);
	writters--;
	if (writters==0)
		V(&queue);
	V(&w_mutex);
}

read_f read_funcs[3] = {read_v0, read_v1, read_v2};
write_f write_funcs[3] = {write_v0, write_v1, write_v2};

/*======================================================================*
                               ReaderA
 *======================================================================*/
void ReaderA()
{
	sleep_ms(TIME_SLICE);
	while(1){
		read_funcs[strategy]('A', 2, '\01');
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReaderB
 *======================================================================*/
void ReaderB()
{
	sleep_ms(2*TIME_SLICE);
	while(1){
		read_funcs[strategy]('B', 3, '\02');
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReaderC
 *======================================================================*/
void ReaderC()
{
	sleep_ms(3*TIME_SLICE);
	while(1){
		read_funcs[strategy]('C', 3, '\03');
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               WriterD
 *======================================================================*/
void WritterD()
{
	sleep_ms(4*TIME_SLICE);
	while(1){
		write_funcs[strategy]('D', 3, '\04');
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               WriterE
 *======================================================================*/
void WritterE()
{
	sleep_ms(5*TIME_SLICE);
	while(1){
		write_funcs[strategy]('E', 4, '\05');
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReporterF
 *======================================================================*/
void ReporterF()
{
	sleep_ms(TIME_SLICE);
	char color = '\06';
	while (1) {
        if (readers > 0 && tr > 0){
            printf("%c%d person is reading\n", color, tr);
        } else if (writting){
            printf("%cThe book is being written\n", color);
        } else {
            printf("%cNo user is reading or writting\n", color);
			// printf("sema rw:%d r:%d w:%d q:%d\n", rw_mutex.value, 
			// 	  r_mutex.value, w_mutex.value, queue.value);
			//printf("rw: %d", rw_mutex.p_list[rw_mutex.head] - proc_table);
        }
        sleep_ms(TIME_SLICE);
    }	
}