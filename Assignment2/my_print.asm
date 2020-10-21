section .data
	red db  27, '[1;31m'
    ;27 means \033;[1;31m表示红色高亮
	    .len    equ $ - red
	default db 27, '[0m'
    ;恢复默认
	    .len    equ $ - default

section .text
	global my_print
	global back
	global change_to_red

	;my_print(char* c, int length);
	;利用栈传递参数
	my_print:
		mov	edx,[esp+8]
		mov	ecx,[esp+4]						
		mov	ebx,1
		mov	eax,4 
		int	80h
		ret
		
	
	back:
		mov eax, 4
		mov ebx, 1
		mov ecx, default
		mov edx, default.len
		int 80h
		ret

	change_to_red:
		mov eax, 4
		mov ebx, 1
		mov ecx, red
		mov edx, red.len
		int 80h
		ret