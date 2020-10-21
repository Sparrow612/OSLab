%define	MAXL   	22
%define MUL_MAX	42
%define INF     46
%define LF      10
%define SPACE   32
%define P       43
%define N       45
%define ZERO    48

;2个数最多需要22位，剩下2位留给中间的空格和最后的换行符
section	.data
	msg:		db	"Please input x and y: ",0Ah
    ;.len:       equ $-msg;msg的长度
    mul_res:    times   MUL_MAX db  0
section	.bss
    buffer      resb    INF
	;store the numstr
	strx:		resb	MAXL
    stry:       resb    MAXL
    add_res:    resb    MAXL
	
section	.text
global main;使用gcc编译，因此函数名必须为main

main:
	mov		eax, msg
	call	sprint;输出提示信息
	
	call	readnums
	
	mov     eax, LF
    push    eax;栈底的空格，标志着出栈结束

    mov     eax, buffer
    mov     ebx, strx
    inc     ebx;下一个字符的地址

    mov     edx, strx;edx保存str的地址
    mov     byte[edx], P;默认是正数

;因为栈的原因，第二个数存在strx中，第一个则在stry中，不过他们的顺序不重要
cont:
    cmp     byte[eax], LF;匹配到换行符就停下
    je      .poploop

    .pushloop:
        movzx   ecx, byte[eax];无符号扩展
        push    ecx
        inc     eax
        jmp     cont
    .poploop:
        pop     ecx
        cmp     ecx, LF;这个LF是之前输入在栈底的LF，标志着结束
        je      judge;遇到栈底空格说明结束了

        cmp     ecx, N
        jne     .spacedetect;不是负号就看看是否为空格
        ;如果是负号，那说明第二个输入数是负数
        mov     byte[edx], N;
        jmp     .poploop
    .spacedetect:
        cmp     ecx, SPACE
        jne     .nextnum
        ;如果是空格，说明有一个数输入完成了
        mov     ebx, stry
        inc     ebx
        mov     edx, stry
        mov     byte[edx], P;same as line 43
        ;空格要跳过
        pop     ecx;因为是倒序，所以空格下一个弹出的必然是数字
    .nextnum:
        sub     cl, ZERO;cl, ecx的低位，把字符转为0～9的整数值
        mov     byte[ebx], cl
        inc     ebx
        jmp     .poploop

;读取输入
readnums:
	mov     edx, INF;nums of bytes to read
    mov     ecx, buffer;des
    mov     ebx, 0;stdin
    mov     eax, 3;sys_read       
    int     80h
	ret

judge:
;判断两数是同号还是异号，同加异减
    mov     al, byte[strx]
    add     al, byte[stry]
    cmp     al, 88;43+45, 说明是异号
    jne     doadd;同号做加法
    ;符号异，减
    cmp     byte[strx], N;
    je      .switch;保证正数在前
    mov     esi, strx
    mov     edi, stry
    jmp     dosub
.switch:
;交换两数传入顺序
    mov     esi, stry
    mov     edi, strx
    jmp     dosub

;add of two num with the same sign

doadd:
    mov     esi, strx
    mov     edi, stry
    mov     edx, add_res;结果的地址存在edx中
    call    add_driver
    jmp     domul;加法做完之后做乘法
;add(esi, edi, res=edx)
add_driver:
    mov     eax, 0
    mov     ebx, 0
    mov     ch, 0
    mov     byte[edx], P;结果默认是负
    cmp     byte[esi], P
    je      .addloop
    mov     byte[edx], N;两个数同号，发现一个负就说明结果是负
    .addloop:
        inc     esi
        inc     edi
        inc     edx;符号位已经确定了
        mov     bl, 0
        inc     ch;ch for ch in range(1,MAXL) MAXL-1 times
        cmp     ch, MAXL
        je      .finish;if ch==MAXL: jump
        mov     bl, byte[esi]
        mov     cl, byte[edi]
        add     bl, cl
        add     bl, al
        ;下面的部分处理进位
        mov     eax, ebx
        mov     ebx, 10
        push    edx;结果的地址
        mov		edx, 0;edx寄存器清0
        div     ebx
        ;divide eax by 10
        ;the result is saved in eax
        ;edx holds the remainder
        mov     ebx, edx;
        pop     edx
        mov     byte[edx], bl
        jmp     .addloop
    .finish:
        ret

;sub: esi-ebi, the one in esi in positive
;the final sign is to be judged
dosub:	
    call    sub_driver
    jmp     domul
sub_driver:
    mov     edx, 0;计数器
    mov     byte[add_res], P;结果默认为正
    .subloop:
        inc     esi
        inc     edi
        
        mov     bl, 0
        inc     edx
        cmp     edx, MAXL;1<=ch<MAXL
        je      .modify;每一位都减完进入下一环节
        
        mov     bl, byte[esi]
        mov     cl, byte[edi]
        sub     bl, cl

        mov     byte[add_res+edx], bl
        jmp     .subloop

    .modify:
        mov     edx, 0
    .modifyloop:
        inc     edx
        cmp     edx, 21;MAXL-1,1<=index<=20
        je      .backwave

        mov     al, 0
    .inner:
        push    eax
		push	edx
		mov		edx, 0
        mov     dl, 10
        mul     dl
		pop		edx
        add     al, byte[add_res+edx]
        cmp     al, 0
        
        jl      .cont
        mov     byte[add_res+edx], al
        pop     eax
        sub     byte[add_res+edx+1], al
        jmp     .modifyloop 
    .cont:
        pop     eax
        inc     al
        jmp     .inner
    .backwave:
        ;如果首位是正，说明结果为正，直接返回
        ;首位为负，需要不断向后（高位到低位）修正结果，方法名也源于此
        cmp     byte[add_res+MAXL-1], 0
        jge     .done;减法完成
        mov     byte[add_res], N;没跳转的都是负号
        mov     edx, MAXL-1
    ;现在，我们要把高位的-1往低位推
    .negative_proc:
        cmp     edx, 1;for(i=MAXL-1;i>1;i--)
        je      .first_proc        
        cmp     byte[add_res+edx-1], 0
        jl      .abs
        sub     byte[add_res+edx-1], 10
        add     byte[add_res+edx], 1
    ;化负为正
    .abs:
        mov     al, byte[add_res+edx]
        mov     byte[add_res+edx], 0
        sub     byte[add_res+edx], al
        
        dec     edx
        jmp     .negative_proc
    .first_proc:
    ;上述过程可能会把第一位（个位）修正为10（-10取绝对值）需要手动处理
        mov     al, byte[add_res+1]
        mov     byte[add_res+1], 0
        sub     byte[add_res+1], al
        mov     ebx, 1
    .last_modify:
        cmp     byte[add_res+ebx], 10
        jl      .done
        mov     byte[add_res+ebx], 0
        add     byte[add_res+ebx+1], 1 
        inc     ebx
        jmp     .last_modify
    .done:
        ret

domul:
    mov     byte[mul_res], P
    mov     ecx, 0
    mov     eax, 0
    add     al, byte[strx]
    add     al, byte[stry]
    cmp     al, 88;43+45, 说明是异号
    jne     .mul_driver;同号,结果为正
    mov     byte[mul_res], N;否则需要把符号改为负
    .mul_driver:
    ;类似一个双层for循环
    ;双指针法
    ;ch:第二个乘子的当前位，0～20, 或者叫乘法的偏移位
    ;cl:第一个乘子的当前位，0～20
        cmp     ch, 20;0~20
        jg      .finish
    .mulloop:
        movzx   edx, cl
        mov     al, byte[stry+edx+1]
        movzx   ebx, ch
        ;mul src src:1byte ax=al*src
        mul     byte[strx+ebx+1];res in eax
        push    ecx
        add     cl, ch
        movzx   edx, cl
        pop     ecx     
        add     byte[mul_res+edx+1], al
        inc     cl

        cmp     cl, 20
        jle     .mul_driver
        inc     ch
        mov     cl, 0
        jmp     .mul_driver
    .finish:
    ;处理进位
        mov     ebx, 1
        ;符号位在前面处理完毕了
    .format:
        cmp     ebx, 40;1~40位
        jg      return
        mov     ecx, 10
        movzx   eax, byte[mul_res+ebx]
        mov     edx, 0;edx寄存器清0
        div     ecx;line 136 for ref
        mov     byte[mul_res+ebx], dl
        add     byte[mul_res+ebx+1], al
        inc     ebx
        jmp     .format       
return:
;输出加法和乘法的结果
    mov     eax, 21;offset
    mov     edx, add_res
    call    printres

    mov     eax, 41
    mov     edx, mul_res
    call    printres

    call    quit

printres:
;获取边界（去掉开头的0）
    cmp     byte[edx+eax], 0
    jne     .print
    dec     eax
    cmp     eax, 1;1是最后一个数字所在地址（偏移）
    jl      .printzero
    jmp     printres
    .print:
    ;比较符号位
        cmp     byte[edx], P
        je      .printbody
        ;输出负号
        push    eax
        mov     eax, N
        push    eax
        mov     eax, esp
        call    cprint
        pop     eax
        pop     eax
    .printbody:
    ;这里还需要完善
        cmp     eax, 0
        je      .done;<0说明全部输入完
        add     byte[edx+eax], ZERO
        push    eax;eax后面有用
        movzx   ecx, byte[edx+eax]
        push    ecx
        mov     eax, esp
        call    cprint
        pop     ecx
        pop     eax
        dec     eax
        jmp     .printbody
    .printzero:
        push    eax
        mov     eax, ZERO
        push    eax
        mov     eax, esp
        call    cprint
        call    printLF
        pop     eax
        pop     eax
        ret
    .done:
        call    printLF
        ret;因为无法确定输出的是加还是减，所以这里需要返回

	
;===========================工具函数===================================
;get the len of a string
;use ebx and eax
;the address of char is stored in eax
;the result stored in eax
strlen:
    push	ebx
    mov		ebx, eax
    .nextchar:
        cmp     byte[eax], 0
        jz    	.count_fin
        inc     eax
        jmp     .nextchar
    .count_fin:
        sub     eax, ebx
        pop     ebx
        ret


;string print
sprint:
	;src addr of the string is stored in eax
    push    edx
    push    ecx
    push    ebx
    push    eax
    call    strlen
 
    mov     edx, eax;length
	
    pop     eax
 
    mov     ecx, eax;src string
    mov     ebx, 1;stdout
    mov     eax, 4;sys_write
    int     80h
 
    pop     ebx
    pop     ecx
    pop     edx
    ret


;char print
;the address of char is stored in eax
cprint:
	;save into stack
    push    edx
    push    ecx
    push    ebx

    mov     edx, 1
    mov     ecx, eax
    mov     ebx, 1
    mov     eax, 4;system_write
    int     80h
 	;return
    pop     ebx
    pop     ecx
    pop     edx
    ret
	
;print linefeed
printLF:
	push    eax         ; push eax onto the stack to preserve it while we use the eax register in this function
    mov     eax, 0Ah    ; move 0Ah into eax
    push    eax         ; push the linefeed onto the stack so we can get the address
    mov     eax, esp    ; move the address of the current stack pointer into eax for sprint
    call    cprint      ; call cprint
    pop     eax         ; remove the linefeed char
    pop     eax         ; restore the original value of eax before our function was called
    ret                 	
;exit()
quit:
	mov		ebx, 0
	mov 	eax, 1
	int		80h
	ret