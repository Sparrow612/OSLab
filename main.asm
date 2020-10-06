%define	MAXL	22;10^20 with a sign bit
%define BUFL	23;'\0' takes up a bit
%define MUL_MAX	42;the max num of bits of mutiplication

section	.data
	msg			db	"Please input x and y: ",0Ah
section	.bss
	;store the numstr
	buffer:		resb	BUFL
	;the big nums
	x:			resb	MAXL
	y:			resb	MAXL
	result:		resb	MAXL
section	.text
global main


main:
	mov		eax, msg
	call	sprint
	
	call	readnum
	
	mov		eax, buffer
	call	sprint
	
	
	

readnum:
	mov     edx, BUFL;nums of bytes to read
    mov     ecx, buffer;des
    mov     ebx, 0;stdin
    mov     eax, 3;sys_read       
    int     80h
	ret


;loadnum:
	;the des address is stored in eax
;	push	ebx
;	mov		ebx, 0;offset
;	call	readchar
;	cmp		[cb], 
	
	

;the len of a string
;use ebx and eax
;the address of char is stored in eax
;result stored in eax
strlen:
    push	ebx
    mov		ebx, eax
nextchar:
    cmp     byte [eax], 0
    jz    	count_fin
    inc     eax
    jmp     nextchar
count_fin:
    sub     eax, ebx
    pop     ebx
    ret
	
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
	;get the length
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
	
	
	