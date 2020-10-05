%define MAX 21
%define ADD_MAX 22
%define MUL_MAX 42
section .data
    msg: db "Please input x and y: ",0Ah
    .len: equ $-msg
    newline: db 0Ah
    .len: equ $-newline
section .bss
    num1: resb MAX
    num2: resb MAX
    len1: resb 4
    len2: resb 4
    readDigit: resb 1;每次读取一个字节
    count: resb 4;计数器作用用于加法
    count1: resb 4;计数器作用用于乘法
    count2: resb 4;计数器作用用于乘法
    count3: resb 4;计算器作用用于乘法结果输出
    flag1: resb 4
    flag2: resb 4
    add_result: resb ADD_MAX
    temp: resb 4
    temp1: resb 4
section .data
    mul_temp: db 0,0,0,0
    mul_temp_sum: db 0,0,0,0
    mul_result: db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
section .text
global main

main:
    mov dword[len1],0
    mov dword[len2],0
    
    mov eax,4
    mov ebx,1
    mov ecx,msg
    mov edx,msg.len
    int 0x80
    
readNum1:
    mov eax,3
    mov ebx,0
    mov ecx,readDigit
    mov edx,1
    int 0x80
    
    cmp byte[readDigit],32
    je readNum2
    
    mov eax,num1
    add eax,dword[len1]
    mov bl,byte[readDigit]
    mov byte[eax],bl
    inc dword[len1]
    jmp readNum1

readNum2:
    mov eax,3
    mov ebx,0
    mov ecx,readDigit
    mov edx,1
    int 0x80
    
    cmp byte[readDigit],0ah
    je init_count_x
    
    mov eax,num2
    add eax,dword[len2]
    mov bl,byte[readDigit]
    mov byte[eax],bl;
    inc dword[len2]
    jmp readNum2
;对x进行扩展
init_count_x:
    mov eax,dword[len1]
    mov dword[count],eax;把num1的长度len1赋值给count，作为计数器
;对x进行移动
move_x:
    cmp dword[count],0
    je extend_x_0
    
    mov eax,num1
    add eax,dword[count]
    dec eax;eax指向num1的最后一位的地址
    mov bl,byte[eax]
    add eax,MAX
    sub eax,dword[len1]
    mov byte[eax],bl
    dec dword[count]
    jmp move_x
;对x进行0扩展
extend_x_0:
    mov dword[count],0
extend_x_1:
    mov eax,MAX
    sub eax,dword[len1]
    cmp dword[count],eax
    je init_count_y
    mov eax,num1
    add eax,dword[count]
    mov byte[eax],'0'
    inc dword[count]
    jmp extend_x_1
;对y进行扩展
init_count_y:
    mov eax,dword[len2]
    mov dword[count],eax
;对y进行移动
move_y:
    cmp dword[count],0
    je extend_y_0
    
    mov eax,num2
    add eax,dword[count]
    dec eax
    mov bl,byte[eax]
    add eax,MAX
    sub eax,dword[len2]
    mov byte[eax],bl
    dec dword[count]
    jmp move_y
;对y进行扩展
extend_y_0:
    mov dword[count],0
extend_y_1:
    mov eax,MAX
    sub eax,dword[len2]
    
    cmp dword[count],eax
    je init_add
    mov eax,num2
    add eax,dword[count]
    mov byte[eax],'0'
    inc dword[count]
    jmp extend_y_1
;加法初始化
init_add:
    mov dword[count],MAX
    mov dh,0
;加法
add_1:
    cmp dword[count],0
    je result_carry
    mov eax,num1
    add eax,dword[count]
    dec eax
    
    mov ebx,num2
    add ebx,dword[count]
    dec ebx
    
    ;一位一位相加判断
    mov cl,byte[eax]
    sub cl,'0';将cl的值转为十进制
    mov dl,byte[ebx]
    sub dl,'0';将dl的值转为十进制
    add cl,dl
    add cl,dh
    cmp cl,10
    jnl carry
    mov dh,0
add_2:
    add cl,'0'
    mov eax,add_result
    add eax,dword[count]
    ;inc eax
    mov byte[eax],cl
    dec dword[count]
    jmp add_1
;加法进位
carry:
    mov dh,1
    sub cl,10
    jmp add_2
result_carry:
    add dh,'0'
    mov eax,add_result
    mov byte[eax],dh
;初始化count计数器
init_count:
    mov dword[count],0
;消除0
judge_zero:
    cmp dword[count],MAX
    je judge_last_iszero
    mov eax,add_result
    add eax,dword[count]
    inc dword[count]
    mov cl,byte[eax]
    cmp cl,'0'
    je judge_zero
    jmp print_add_result
print_add_result:
    mov eax,add_result
    add eax,dword[count]
    dec eax
    mov dword[temp],eax
    mov eax,4
    mov ebx,1
    mov ecx,dword[temp]
    mov edx,1
    int 0x80
    cmp dword[count],ADD_MAX
    je init_mul
    inc dword[count]
    jmp print_add_result
judge_last_iszero:
    mov eax,add_result
    add eax,dword[count]
    mov dword[temp],eax
    mov eax,4
    mov ebx,1
    mov ecx,dword[temp]
    mov edx,1
    int 0x80
    jmp init_mul
;乘法开始
init_mul:
    ;初始化两个计数器，用于两重循环
    mov dword[count1],20
    mov dword[count2],20
;外层循环
loop_mul_1:
    cmp byte[count1],0
    jl init_count3
    mov edi,num1
    add edi,dword[count1]
;内层循环
loop_mul_2:
    mov esi,num2
    add esi,dword[count2]
    
    mov al,byte[edi]
    mov bl,byte[esi]
    sub al,'0'
    sub bl,'0'
    mul bl;ax暂存每一位的乘积结果，实际只对al有效
    
    mov ebx,mul_temp
    mov word[ebx],ax
    
    mov ecx,dword[count1]
    add ecx,dword[count2]
    mov dword[flag1],ecx;flag1 = count1 + count2
    mov dword[flag2],ecx
    inc dword[flag2];flag2 = count1 + count2 + 1
    
    push eax
    push ebx
    mov ebx,mul_result
    add ebx,dword[flag2]
    mov eax,dword[mul_temp]
    add al,byte[ebx]
    mov ebx,mul_temp_sum
    mov dword[mul_temp_sum],eax
    pop ebx
    pop eax
    
    push eax
    push ebx
    push edx
    mov eax,dword[mul_temp_sum]
    mov edx,10
    div dl
    mov ebx,mul_result
    add ebx,dword[flag1]
    add byte[ebx],al
    mov ebx,mul_result
    add ebx,dword[flag2]
    mov byte[ebx],ah
    pop edx
    pop ebx
    pop eax
    
    dec dword[count2]
    cmp dword[count2],0
    jl dec_count1
    jmp loop_mul_2
dec_count1:
    mov dword[count2],20
    dec byte[count1]
    jmp loop_mul_1
;初始化计数器count3
init_count3:
    call printLF
    mov dword[count3],0
add_char_zero:
    cmp dword[count3],MUL_MAX
    je init_count3_1
    mov eax,mul_result
    add eax,dword[count3]
    mov cl,byte[eax]
    add cl,'0'
    mov byte[eax],cl
    inc dword[count3]
    jmp add_char_zero
;再次初始化一下count3
init_count3_1:
    mov dword[count3],0
judge_mul_zero:
    cmp dword[count3],41
    je judge_mul_last_zero
    mov eax,mul_result
    add eax,dword[count3]
    inc dword[count3]
    mov cl,byte[eax]
    cmp cl,'0'
    je judge_mul_zero
    jmp print_mul_result
print_mul_result:
    mov eax,mul_result
    add eax,dword[count3]
    dec eax
    mov dword[temp1],eax
    mov eax,4
    mov ebx,1
    mov ecx,dword[temp1]
    mov edx,1
    int 0x80
    cmp dword[count3],MUL_MAX
    je print_lf
    inc dword[count3]
    jmp print_mul_result
judge_mul_last_zero:
    mov eax,mul_result
    add eax,dword[count3]
    mov dword[temp1],eax
    mov eax,4
    mov ebx,1
    mov ecx,dword[temp1]
    mov edx,1
    int 0x80
print_lf:
    call printLF
    mov eax,1
    mov ebx,0
    int 0x80
printLF:
    mov eax,4
    mov ebx,1
    mov ecx,newline
    mov edx,newline.len
    int 80h
    ret

    


