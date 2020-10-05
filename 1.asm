%define numMax 40
%define indexMax 4
%define countMax 4
%define multMax 80
section .data
string: db 'Please input x and y:',10
length: equ $-string
warning: db 'I want start here',10
length2: equ $-warning ;using for location in debug
equal:db 0
;carry: db 0 ;use register ch, abandon
end: db 0
temp: db 0,0,0,0
SECTION .bss

num1:resw 20
num2:resw 20
count:resw 20 ;too long, abandon
cnt1:resb 4 ;for add and mult
cnt2:resb 4 ;for mult
cnt3:resb 4 ;for mult
cntM:resb 4 ;save work
cnt4:resb 4 ;for mult
cnt5:resb 4  ;woc, it's too many
addResult:resw 20
multResult:resw 40 ;save two result
random2:resw 20
;1 and 2 to load,num 3 to count
index1:resb 4 ;to calc the location
index2:resb 4
print:resb 4 ;like index, but using in print
digit: resb 1 ; it should be buffer, but buffer is four bytes


section .text
global main:
    
main:
    mov eax, 4
    mov ebx, 1
    mov ecx, string
    mov edx, length
    int 80h

    mov dword[index1], 0
    mov dword[index2], 0
    
 getNum1:
    mov eax, 3
    mov ebx, 0
    mov ecx, digit
    mov edx, 1
    int 80h   ;get for digits
    

    cmp byte[digit], ' '
    je initGetNum2 ;blank will jump
    
    mov eax, num1
    add eax, dword[index1]
    mov cl, byte[digit]
    mov byte[eax], cl
    inc dword[index1]
    
   ; mov eax, num1
    ;call strPrintN;check is right
    
    jmp getNum1
    
    
initGetNum2:  
    mov byte[digit], 0
    mov byte[index2], 0
    
    
getNum2:   
    mov eax, 3
    mov ebx, 0
    mov ecx, digit
    mov edx, 1
    int 80h 
    
    cmp byte[digit], 0Ah
    je addition
    
    mov eax, num2
    add eax, dword[index2]
    mov cl, byte[digit]
    mov byte[eax], cl
    inc dword[index2]    
    
   ; mov eax, num2
    ;call strPrintN;check is right
      
    jmp getNum2
    
    
multiply:
    push    eax         
    mov     eax, 0Ah    
    push    eax         
    mov     eax, esp    
    call    strPrint      
    pop     eax         
    pop     eax;change a new line

    mov dword[cnt1],40
    mov dword[cnt2],40
    loopM1:
    cmp byte[cnt1], 0
    jl initCnt3
    
    mov edi, num1
    add edi, dword[cnt1]
    
    
    
    loopM2:
    mov esi, num2
    add esi, dword[cnt2]
    mov al, byte[edi]
    mov bl, byte[esi]
    sub al, '0'
    sub bl, '0'
    mul bl
    
    
    mov ebx, cntM
    mov word[ebx], ax
    
    mov ecx, dword[cnt1]
    add ecx, dword[cnt2]
    mov dword[cnt4],ecx;cnt4=i+j
    mov dword[cnt5],ecx
    inc dword[cnt5] ;cnt5=i+j+1
    
    push eax
    push ebx
    mov ebx,multResult
    add ebx,dword[cnt5]
    mov eax, dword[cntM]
    add al,byte[ebx]
    mov ebx,temp
    mov dword[temp],eax
    pop ebx
    pop eax
    
    push eax
    push ebx
    push edx

    mov eax,dword[temp]
    mov edx,10
    div dl
    mov ebx, multResult
    add ebx,dword[cnt4]
    add byte[ebx], al
    mov ebx,multResult
    add ebx, dword[cnt5]
    mov byte[ebx],ah
    pop edx
    pop ebx
    pop eax
    
    dec dword[cnt2]
    cmp dword[cnt2], 0
    jl outLoopCnt1
    jmp loopM2
    
    outLoopCnt1:
    mov dword[cnt2], 40
    dec dword[cnt1]
    jmp loopM1
    
    initCnt3:
    mov dword[cnt3], 0
    
    fill0Mult:
    cmp dword[cnt3], 80
    je count0Loop2
    mov eax, multResult
    add eax, dword[cnt3]
    mov cl, byte[eax]
    add cl, '0'
    mov byte[eax], cl
    inc dword[cnt3]  
    jmp fill0Mult
    
    
    
    count0Loop2:
    mov dword[cnt3], 0
     
    count0Loop2_1:
    
    cmp dword[cnt3], 80
    je printMulti
    mov eax, multResult
    add eax, dword[cnt3]
    inc dword[cnt3]
    mov cl, byte[eax]
    cmp cl, '0'
    je count0Loop2_1
    
    jmp printMulti    
 
addition:
    ;mov eax, num1
;    call strPrintN
;    
;    mov eax,num2
;    call strPrintN
    
    
    prepareForX:
    mov eax,dword[index1]
    mov dword[cnt1],eax
    ;mov eax, dword[cnt1]
    ;call intPrint
       
    lowX:   
    cmp dword[cnt1], 0
    je fill0ForX
    
    mov eax,num1
    add eax,dword[cnt1]
    dec eax;num1 is pointer, num1+cnt1-1=the last digit of the number
    mov cl,byte[eax]
    add eax,40
    sub eax,dword[index1];load the last digit in num1
    mov byte[eax],cl;switch digit
    dec dword[cnt1]
    jmp lowX
    
    fill0ForX:
    mov dword[cnt1], 0;counter to 0,to fill all 0
    
    fillX0:
    mov eax,40
    sub eax, dword[index1];count 0 need to fill
    cmp dword[cnt1],eax
    je prepareForY
    
    mov eax,num1
    add eax,dword[cnt1]
    mov byte[eax], '0'
    inc dword[cnt1]
    jmp fillX0
    
    
    prepareForY:
    ;mov eax, num1
    ;call strPrintN
    mov eax,dword[index2]
    mov dword[cnt1],eax
    
    lowY:   
    cmp dword[cnt1], 0
    je fill0ForY
    
    mov eax,num2
    add eax,dword[cnt1]
    dec eax;num2 is pointer, num1+cnt1-1=the last digit of the number
    mov cl,byte[eax]
    add eax,40
    sub eax,dword[index2];load the last digit in num1
    mov byte[eax],cl;switch digit
    dec dword[cnt1]
    jmp lowY
    
    fill0ForY:
    mov dword[cnt1], 0;counter to 0,to fill all 0
    
    fillY0:
    mov eax,40
    sub eax, dword[index2];count 0 need to fill
    cmp dword[cnt1],eax
    je prepareForAdd
    
    mov eax,num2
    add eax,dword[cnt1]
    mov byte[eax], '0'
    inc dword[cnt1]
    jmp fillY0
    
    
    prepareForAdd:
    
    mov dword[cnt1],40
    mov ch,0
    
    startAdd:
    
    cmp dword[cnt1], 0
    je addCarry
    
    mov eax,num1
    add eax,dword[cnt1]
    dec eax
    mov ebx,num2
    add ebx,dword[cnt1]
    dec ebx
    
    
    
    mov cl,byte[eax]
    sub cl,'0'
    mov dl,byte[ebx]
    sub dl,'0'
    add cl,dl
    add cl,ch
    cmp cl,10
    jnl carry
    mov ch,0
    
    nextAdd:
    add cl,'0'
    mov eax,addResult
    add eax,dword[cnt1]
    mov byte[eax],cl  
    dec dword[cnt1]
    jmp startAdd
     
    carry:
    mov ch,1
    sub cl,10
    jmp nextAdd
    addCarry:
    add ch,'0'
    mov eax, addResult
    mov byte[eax], ch
    
    initCnt1:
    mov dword[cnt1], 0
    
    countLoop1:
    
    cmp dword[cnt1],40
    je printAdd
    
    
    mov eax,addResult
    add eax,dword[cnt1]
    inc dword[cnt1]
    
    mov cl,byte[eax]
    
    cmp cl, '0'
    je countLoop1
    jmp printAdd
    jmp multiply
    
fail:
    mov eax,dword[cnt1]
    call intPrint
    
isEqual:
    push ecx
    mov ecx, 0
   mov ax, 0
    mov bx, 0
    comp:
       mov ax, word[num2+ecx*2]
        mov bx, word[count+ecx*2]
        add ecx, 1
        cmp ecx,10
        ret
        cmp ax,bx
        je comp
     mov word[equal], 1
     ret
         
exit:
    printAdd:
    mov eax, addResult
    add eax, dword[cnt1]
      
    dec eax
    
    mov dword[print], eax
    mov eax, 4
    mov ebx, 1
    mov ecx,dword[print]
    mov edx,1
    int 80h  
    cmp dword[cnt1],41
    je multiply
    inc dword[cnt1]

    
    jmp printAdd
    
    printMulti:
    
    mov eax, multResult
    add eax, dword[cnt3]
    dec eax
    mov dword[print],eax
    mov eax, 4
    mov ebx, 1
    mov ecx, dword[print]
    mov edx, 1
    int 80h
    cmp dword[cnt3], 41
    je endl;
    inc dword[cnt3]
    jmp printMulti
    
    mov eax, 1
    mov ebx,0
    int 80h


endl:
    push    eax         
    mov     eax, 0Ah    
    push    eax         
    mov     eax, esp    
    call    strPrint      
    pop     eax         
    pop     eax
    
    mov eax, 1
    mov ebx,0
    int 80h
    
strLen:
    push    ebx
    mov     ebx, eax

nextchar:
    cmp     byte [eax], 0
    jz      finished
    inc     eax
    jmp     nextchar

finished:
    sub     eax, ebx
    pop     ebx
    ret


strPrint:
    push    edx
    push    ecx
    push    ebx
    push    eax
    call    strLen
    
    mov     edx, eax
    pop     eax
    
    mov     ecx, eax
    mov     ebx, 1
    mov     eax, 4
    int     80h
    
    pop     ebx
    pop     ecx
    pop     edx
    ret
strPrintN:
    call    strPrint
 
    push    eax         
    mov     eax, 0Ah    
    push    eax         
    mov     eax, esp    
    call    strPrint      
    pop     eax         
    pop     eax         
    ret             

intPrint:
    push    eax             
    push    ecx             
    push    edx             
    push    esi             
    mov     ecx, 0          
 
divideLoop:
    inc     ecx             
    mov     edx, 0          
    mov     esi, 10         
    idiv    esi             
    add     edx, 48         
    push    edx             
    cmp     eax, 0          
    jnz     divideLoop      
 
printLoop:
    dec     ecx             
    mov     eax, esp        
    call    strPrint          
    pop     eax             
    cmp     ecx, 0          
    jnz     printLoop       
 
    pop     esi             
    pop     edx             
    pop     ecx             
    pop     eax            
    ret
    
    
debugCache:
    push eax
    mov eax, dword[cnt1]
    call intPrint
    push    eax         
    mov     eax, 0Ah    
    push    eax         
    mov     eax, esp    
    call    strPrint      
    pop     eax         
    pop     eax;change a new line
    pop eax