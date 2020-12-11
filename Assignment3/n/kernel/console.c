
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);


// 存储换行前后信息
struct LfNode{
	unsigned int prev;
	unsigned int post;
};

PRIVATE int lfTop;
PRIVATE struct LfNode lfs[SCREEN_SIZE/SCREEN_WIDTH];

PRIVATE int opTop;
PRIVATE	char ops[SCREEN_SIZE]; // 将所有操作记录下来，撤销时候会用到

/*======================================================================*
			   isTab
 *======================================================================*/
PRIVATE int isTab(unsigned int cur){
	u8* start = (u8*)(V_MEM_BASE + cur * 2);
	if (start[0] == TAB_CHAR && start[2] == TAB_CHAR && start[4] == TAB_CHAR && start[6] == TAB_CHAR)
		return 1;
	return 0;
}

/*======================================================================*
			   the code for search is here
 *======================================================================*/
PRIVATE int matched(unsigned int cur, char* text, int len)
{
	unsigned int cursor = cur;
	for (int i=0;i<len;i++){
		u8* start = (u8*)(V_MEM_BASE + cursor * 2);
		if (isTab(cursor)){
			if (text[i] == '\t'){
				cursor += 4;
				continue;
			}
			else{
				return 0;
			}
		}
		else if (*start!=text[i])
			return 0;
		cursor++;
	}
	return cursor - cur;
}

PRIVATE void turnToRed(u8* start, int len){
	for (int i=0;i<len;i++){
		*start = RED_CHAR_COLOR;
		start += 2;
	}
}

PUBLIC void search(CONSOLE* p_con, char* text, int len){
	int t;
	for (unsigned int i=p_con->original_addr;i<p_con->cursor-len;i++){
		u8* start = (u8*)(V_MEM_BASE + i * 2) + 1;
		if (*start == RED_CHAR_COLOR) break; // 遇到了搜索文字
		if (t = matched(i, text, len)){
			turnToRed(start, t);
			i += t - 1;
		}
	}
}

PUBLIC void endSearch(CONSOLE* p_con, int keyLen){
	for (int i=0;i<keyLen;i++){
		out_char(p_con, '\b');
	}
	for (int i=p_con->original_addr;i<p_con->cursor;i++){
		u8* start = (u8*)(V_MEM_BASE + i * 2) + 1;
		*start = DEFAULT_CHAR_COLOR;
	}
}
/*======================================================================*
			   the code for search is end
 *======================================================================*/



/*======================================================================*
			   init_screen
 *======================================================================*/
PUBLIC void init_screen(TTY* p_tty)
{
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;
	/* 默认非查找模式 */
	p_tty->p_console->searchMode = 0;
	p_tty->p_console->blocked = 0;
	p_tty->p_console->rolling = 0;
	p_tty->p_console->cleaning = 0;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	//else {
		// out_char(p_tty->p_console, nr_tty + '0');
		// out_char(p_tty->p_console, '#');
	//}
	clean_screen(p_tty->p_console);
}

// clean and reset, maybe the name "refresh" is more suitable
PUBLIC void clean_screen(CONSOLE* p_con){
	u8* p_vmem = (u8*)(V_MEM_BASE);
	for (int i=p_con->original_addr;i<p_con->cursor;i++){
		*p_vmem++ = ' ';
		*p_vmem++ = DEFAULT_CHAR_COLOR;
	}
	p_con->cursor = p_con->current_start_addr 
	= p_con->original_addr;
	lfTop = opTop = -1;
	flush(p_con);
}

/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
	u8 color = DEFAULT_CHAR_COLOR;
	switch(ch) {
	case '\n':
		if (p_con->cursor < p_con->original_addr +
		    p_con->v_mem_limit - SCREEN_WIDTH) {
			lfs[++lfTop].prev = p_con->cursor;
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * 
				((p_con->cursor - p_con->original_addr) /
				 SCREEN_WIDTH + 1);
			lfs[lfTop].post = p_con->cursor;
			if (!p_con->rolling)	
				ops[++opTop] = '\b';
		}
		break;
	case '\b':
		if (p_con->cursor > p_con->original_addr) {
			// 首先需要判断退格什么字符
			char c;
			if (isTab(p_con->cursor-TAB_LEN)){
			// 退格tab
				c = '\t';
				for (int i=1;i<=TAB_LEN;i++){
					p_con->cursor--;
					*(p_vmem-2*i) = ' ';
					*(p_vmem-(2*i-1)) = DEFAULT_CHAR_COLOR;
				}
			}else if (p_con->cursor == lfs[lfTop].post)
			{ // 退格换行符
				c = '\n';
				p_con->cursor = lfs[lfTop].prev; 
				lfTop--;
			}else
			{ // others
				p_con->cursor--;
				c = *(p_vmem-2);
				*(p_vmem-2) = ' ';
				*(p_vmem-1) = DEFAULT_CHAR_COLOR;
			}
			if (!p_con->rolling)	
				ops[++opTop] = c;
		}
		break;
	case '\t':
		// 输入四个空格，保存加入四个空格后的p_con->curson的值；删除的时候要一起删（四个空格是一个整体）
		if (p_con->cursor <  
			p_con->original_addr + p_con->v_mem_limit - TAB_LEN){
			for(int i=0;i<TAB_LEN;i++){
				*p_vmem++ = TAB_CHAR;
				*p_vmem++ = DEFAULT_CHAR_COLOR;
				p_con->cursor++;
			}
			if (!p_con->rolling)	
				ops[++opTop] = '\b';
		}
		break;
	default:
		if (p_con->searchMode){
			color = RED_CHAR_COLOR;
		} // 如果是查找模式，字体颜色要变红
		if (p_con->cursor <
		    p_con->original_addr + p_con->v_mem_limit - 1) {
			*p_vmem++ = ch;
			*p_vmem++ = color;
			p_con->cursor++;
			if (!p_con->rolling)	
				ops[++opTop] = '\b';
		}
		break;
	}
	
	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}

PUBLIC void rollback(CONSOLE* p_con) {
// 先前已经将字符存入栈，所以这里直接调用outchar即可
	p_con->rolling = 1;
	if (opTop >= 0)
		out_char(p_con, ops[opTop--]);
	p_con->rolling = 0;
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
        set_cursor(p_con->cursor);
        set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}



/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction)
{
	if (direction == SCR_UP) {
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}