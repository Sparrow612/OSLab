
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
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
#include "keyboard.h"
#include "proto.h"

#define TTY_FIRST	(tty_table)
#define TTY_END		(tty_table + NR_CONSOLES)

PRIVATE void init_tty(TTY* p_tty);
PRIVATE void tty_do_read(TTY* p_tty);
PRIVATE void tty_do_write(TTY* p_tty);
PRIVATE void put_key(TTY* p_tty, u32 key);


PRIVATE char searchText[SCREEN_SIZE]; // 存放搜索关键字
PRIVATE int keyLen; // 关键词长度


/*======================================================================*
                           task_fresher
 *======================================================================*/
PUBLIC void task_fresher()
{
	while (1) {
		milli_delay(200000);
		for (TTY* p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
			while (p_tty->p_console->searchMode) {}
			clean_screen(p_tty->p_console);
		}
	}
}

/*======================================================================*
                           task_tty
 *======================================================================*/
PUBLIC void task_tty()
{
	TTY*	p_tty;

	init_keyboard();

	for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
		init_tty(p_tty);
	}
	keyLen = 0;
	select_console(0);
	// int start = get_ticks(); // 获取当前中断次数
	
	while (1) {
		for (p_tty=TTY_FIRST;p_tty<TTY_END;p_tty++) {
			while (p_tty->p_console->cleaning) {}
			tty_do_read(p_tty);
			tty_do_write(p_tty);
		}
		// if (get_ticks()-start >= 20000){
		// 	if (!console_table[nr_current_console].searchMode){
		// 		clean_screen(&console_table[nr_current_console]);
		// 		start = get_ticks();
		// 	}
		// }
	}
}

/*======================================================================*
			   init_tty
 *======================================================================*/
PRIVATE void init_tty(TTY* p_tty)
{
	p_tty->inbuf_count = 0;
	p_tty->p_inbuf_head = p_tty->p_inbuf_tail = p_tty->in_buf;

	init_screen(p_tty);
}

/*======================================================================*
				in_process
 *======================================================================*/
PUBLIC void in_process(TTY* p_tty, u32 key)
{
    char output[2] = {'\0', '\0'};
	CONSOLE* p_con = p_tty->p_console;
	int raw_code = key & MASK_RAW;

	if (raw_code == ESC){
		// 删除关键词，恢复光标位置
		switch(p_con->searchMode){
		case 0:
			p_con->searchMode = 1;
			break;
		case 1:
			endSearch(p_con, keyLen);
			p_con->searchMode = 0;
			break;
		default:
			break;
		}
		keyLen = 0;
		p_con->blocked = 0;
		return;
	}

	if (p_con->blocked == 0){
        if (!(key & FLAG_EXT)) {
			if ((key&FLAG_CTRL_L)||(key&FLAG_CTRL_R)){
				output[0] = key;
				if (output[0] == 'z' || output[0] == 'Z'){
				// 撤销
					rollback(p_con);
				}
			}else{
				put_key(p_tty, key);
			}
        }
        else {
            switch(raw_code) {
            case ENTER:
				if (p_con->searchMode){
				// 如果是查找模式下按了Enter，需要调用搜索方法，并屏蔽ESC外的所有按键
					p_con->blocked = 1;
					search(p_con, searchText, keyLen);
				}else{
					put_key(p_tty, '\n');
				}
				break;
			case BACKSPACE:
				if (p_con->searchMode && keyLen>0){
					// 退格关键字
					put_key(p_tty, '\b');
					keyLen--;
				}else if (!p_con->searchMode){
					// 正常退格
					put_key(p_tty, '\b');
				}
				break;
			case TAB:
				put_key(p_tty, '\t');
				break;
			case UP:
                if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
					scroll_screen(p_con, SCR_DN);
                }
				break;
			case DOWN:
				if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
					scroll_screen(p_con, SCR_UP);
				}
				break;
			// 下面的代码是用来切换终端的，跟本次作业没有关系
			case F1:
			case F2:
			case F3:
			case F4:
			case F5:
			case F6:
			case F7:
			case F8:
			case F9:
			case F10:
			case F11:
			case F12:
				/* Alt + F1~F12 */
				if ((key & FLAG_ALT_L) || (key & FLAG_ALT_R)) {
					select_console(raw_code - F1);
				}
				break;
            default:
                break;
            }
        }
	}
}

/*======================================================================*
			      put_key
*======================================================================*/
PRIVATE void put_key(TTY* p_tty, u32 key)
{
	if (p_tty->inbuf_count < TTY_IN_BYTES) {
		*(p_tty->p_inbuf_head) = key;
		p_tty->p_inbuf_head++;
		if (p_tty->p_inbuf_head == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_head = p_tty->in_buf;
		}
		p_tty->inbuf_count++;
	}
}


/*======================================================================*
			      tty_do_read
 *======================================================================*/
PRIVATE void tty_do_read(TTY* p_tty)
{
	if (is_current_console(p_tty->p_console)) {
		keyboard_read(p_tty);
	}
}


/*======================================================================*
			      tty_do_write
 *======================================================================*/
PRIVATE void tty_do_write(TTY* p_tty)
{
	if (p_tty->inbuf_count) {
		char ch = *(p_tty->p_inbuf_tail);
		p_tty->p_inbuf_tail++;
		if (p_tty->p_inbuf_tail == p_tty->in_buf + TTY_IN_BYTES) {
			p_tty->p_inbuf_tail = p_tty->in_buf;
		}
		p_tty->inbuf_count--;
		if (p_tty->p_console->searchMode && ch != '\b'){
		// 处于搜索模式且不是输入退格符
			searchText[keyLen++] = ch;
		}
		out_char(p_tty->p_console, ch);
	}
}