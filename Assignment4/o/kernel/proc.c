
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               proc.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "tty.h"
#include "console.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"



/**
 * FCFS算法
 */
PUBLIC void p_process(SEMAPHORE* s){
    s->value--;
    if (s->value < 0){
        p_proc_ready->blocked = 1;
        s->p_list[s->tail] = p_proc_ready;
        s->tail = (s->tail + 1) % NR_TASKS;
        schedule();
    }
}

PUBLIC void v_process(SEMAPHORE* s){
    s->value++;
    if (s->value <= 0){
        s->p_list[s->head]->blocked = 0; // 唤醒最先进入队列的进程
        s->head = (s->head + 1) % NR_TASKS;
    }
}

/*======================================================================*
                              schedule
 *======================================================================*/
PUBLIC void schedule()
{
	PROCESS* p;
	int	greatest_ticks = 0;
	int flag = 1;

	while (!greatest_ticks) {
		for (p = proc_table; p < proc_table+NR_TASKS; p++) {

			if (p->sleeping > 0 || p->blocked) continue; 
			// 正在睡眠/阻塞的进程不会被执行（也就是不会被分配时间片）
			flag = 0;
			if (p->ticks > greatest_ticks) {
				greatest_ticks = p->ticks;
				p_proc_ready = p;
			}
		}
		// 如果都是0，那么需要重设ticks
		if (!greatest_ticks) {
			for (p = proc_table; p < proc_table+NR_TASKS; p++) {
				if (p->ticks > 0) continue; // 正在睡眠/阻塞的进程（>0却还进入这个if语句块的唯一可能）可能还剩时间片，不应重置
				p->ticks = p->priority;
			}
		}
		if (flag) break; // 如果全都在阻塞状态，那么放弃本次调度
	}
}

/*======================================================================*
                           sys_get_ticks
 *======================================================================*/
PUBLIC int sys_get_ticks()
{
	return ticks;
}

PUBLIC void sys_sleep(int milli_sec) 
{
	int ticks = milli_sec / 1000 * HZ * 10;  // 乘10是为了修正系统的时钟中断错误
	p_proc_ready->sleeping = ticks;
    schedule();
}

PUBLIC void sys_write_str(char* buf, int color)
{
    disp_color_str(buf, color);
}

