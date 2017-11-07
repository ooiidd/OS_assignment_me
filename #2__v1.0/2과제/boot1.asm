org	0x9000  

[BITS 16]  

		cli		; Clear Interrupt Flag

		mov     ax, 0xb800 
        mov     es, ax
        mov     ax, 0x00
        mov     bx, 0
        mov     cx, 80*25*2 
CLS:
        mov     [es:bx], ax
        add     bx, 1
        loop    CLS 
 
Initialize_PIC:
		;ICW1 - 두 개의 PIC를 초기화 
		mov		al, 0x11
		out		0x20, al
		out		0xa0, al

		;ICW2 - 발생된 인터럽트 번호에 얼마를 더할지 결정
		mov		al, 0x20
		out		0x21, al
		mov		al, 0x28
		out		0xa1, al

		;ICW3 - 마스터/슬레이브 연결 핀 정보 전달
		mov		al, 0x04
		out		0x21, al
		mov		al, 0x02
		out		0xa1, al

		;ICW4 - 기타 옵션 
		mov		al, 0x01
		out		0x21, al
		out		0xa1, al

		mov		al, 0xFF
		;out		0x21, al
		out		0xa1, al

Initialize_Serial_port:
		xor		ax, ax
		xor		dx, dx
		mov		al, 0xe3
		int		0x14

READY_TO_PRINT:
		xor		si, si
		xor		bh, bh
PRINT_TO_SERIAL:
		mov		al, [msgRMode+si]
		mov		ah, 0x01
		int		0x14
		add		si, 1
		cmp		al, 0
		jne		PRINT_TO_SERIAL
PRINT_NEW_LINE:
		mov		al, 0x0a;0a
		mov		ah, 0x01
		int		0x14
		mov		al, 0x0d
		mov		ah, 0x01
		int		0x14

; OS assignment 2
; add your code here
; print current date to boch display

Print_date:

	mov	ah, 0x04;ah를 0x04로
	mov	al, 0x00
	int	0x1a ; 1a 인터럽트 호출

	xor	ax,0x0000;ax 초기화
	mov	es,ax	;es레지스터 초기화


	mov	ah, 0xF0;상위 1바이트만 남기기 위하여 ah를 0xF0으로 설정.
	and	ah,ch;ch와 and연산을 통해 ch의 상위 1바이트만 ah에 담기게 되고 하위 바이트는 0000이 된다.
	shr	ah,4;상위 1바이트를 하위 바이트로 옮기기위해 shr명령을 통해 옮겨준다.
	add	ah,0x30;숫자의 아스키 값으로 맞추기 위해서 0x30을 더해준다.
	mov	[es:0x0000], ah;es의 오프셋 0000부터 써줄 말을 담는다.

	mov	ah,0x0F;하위 1바이트만 남기기 위해 ah를 0x0F로 설정.
	and	ah,ch;ch와 and연산을 통해 ch의 하위 1바이트만 남겨서 ah에 담는다.
	add	ah,0x30; 숫자의 아스키 값으로 맞추기 위해서 0x30을 더해준다.
	mov	[es:0x0001], ah;es의 오프셋 0001에 다음 말을 담는다.

	mov	ah, 0xF0;cl에는 year이 있는데 위와 같은 방식으로 아스키값으로 바꾸어 es에 넣어준다.
	and	ah,cl
	shr	ah,4
	add	ah,0x30
	mov	[es:0x0002], ah

	mov	ah,0x0F
	and	ah,cl
	add	ah,0x30
	mov	[es:0x0003], ah


	mov	ah, 0xF0;dh에는 month가 있는데 위와 같은 방식으로 아스키값으로 바꾸어 es에 넣어준다.
	and	ah,dh
	shr	ah,4
	add	ah,0x30
	mov	[es:0x0004], ah

	mov	ah,0x0F
	and	ah,dh
	add	ah,0x30
	mov	[es:0x0005], ah


	mov	ah, 0xF0;dl에는 day가 있는데 위와 같은 방식으로 아스키값으로 바꾸어 es에 넣어준다.
	and	ah,dl
	shr	ah,4
	add	ah,0x30
	mov	[es:0x0006], ah

	mov	ah,0x0F
	and	ah,dl
	add	ah,0x30
	mov	[es:0x0007], ah

	mov	ax, 0x1300;10번 인터럽트에서 AX를 0x1300으로 설정해서 Display String 으로 사용한다.
	mov	bh,0x0;video page 를 0으로 설정하여 0페이지에 출력해 준다.
	mov	bl,0x0e;video attribute 노란색으로 글자를 바꾸기 위해 0x0e로 설정한다.
	mov	cx,0x8;스트링의 길이를 8로 설정한다. 년월일 8개면 충분하다.
	mov	dh,0x0;출력 행을 0열로 설정하여 맨위체 출력한다.
	mov	dl,36;출력 열을 36으로 설정하여 중간인 40에 8글자중 중간글자가 위치하게 하기위해 36으로 설정하였다.
	int	0x10;인터럽트 10번을 호출한다.

	mov	ax,0xb800;es를 다시 원상복구 시켜주기 위해 ax에 넣고 다시 es에 넣어준다.
	mov	es,ax



Activate_A20Gate:
		mov		ax,	0x2401
		int		0x15

;Detecting_Memory:
;		mov		ax, 0xe801
;		int		0x15

PROTECTED:
        xor		ax, ax
        mov		ds, ax              

		call	SETUP_GDT

        mov		eax, cr0  
        or		eax, 1	  
        mov		cr0, eax  

		jmp		$+2
		nop
		nop
		jmp		CODEDESCRIPTOR:ENTRY32

SETUP_GDT:
		lgdt	[GDT_DESC]
		ret

[BITS 32]  

ENTRY32:
		mov		ax, 0x10
		mov		ds, ax
		mov		es, ax
		mov		fs, ax
		mov		gs, ax

		mov		ss, ax
  		mov		esp, 0xFFFE
		mov		ebp, 0xFFFE	

		mov		edi, 80*2
		lea		esi, [msgPMode]
		call	PRINT

		;IDT TABLE
	    cld
		mov		ax,	IDTDESCRIPTOR
		mov		es, ax
		xor		eax, eax
		xor		ecx, ecx
		mov		ax, 256
		mov		edi, 0
 
IDT_LOOP:
		lea		esi, [IDT_IGNORE]
		mov		cx, 8
		rep		movsb
		dec		ax
		jnz		IDT_LOOP

		lidt	[IDTR]

		sti
		jmp	CODEDESCRIPTOR:0x10000

PRINT:
		push	eax
		push	ebx
		push	edx
		push	es
		mov		ax, VIDEODESCRIPTOR
		mov		es, ax
PRINT_LOOP:
		or		al, al
		jz		PRINT_END
		mov		al, byte[esi]
		mov		byte [es:edi], al
		inc		edi
		mov		byte [es:edi], 0x07

OUT_TO_SERIAL:
		mov		bl, al
		mov		dx, 0x3fd
CHECK_LINE_STATUS:
		in		al, dx
		and		al, 0x20
		cmp		al, 0
		jz		CHECK_LINE_STATUS
		mov		dx, 0x3f8
		mov		al, bl
		out		dx, al

		inc		esi
		inc		edi
		jmp		PRINT_LOOP
PRINT_END:
LINE_FEED:
		mov		dx, 0x3fd
		in		al, dx
		and		al, 0x20
		cmp		al, 0
		jz		LINE_FEED
		mov		dx, 0x3f8
		mov		al, 0x0a
		out		dx, al
CARRIAGE_RETURN:
		mov		dx, 0x3fd
		in		al, dx
		and		al, 0x20
		cmp		al, 0
		jz		CARRIAGE_RETURN
		mov		dx, 0x3f8
		mov		al, 0x0d
		out		dx, al

		pop		es
		pop		edx
		pop		ebx
		pop		eax
		ret

GDT_DESC:
        dw GDT_END - GDT - 1    
        dd GDT                 
GDT:
		NULLDESCRIPTOR equ 0x00
			dw 0 
			dw 0 
			db 0 
			db 0 
			db 0 
			db 0
		CODEDESCRIPTOR  equ 0x08
			dw 0xffff             
			dw 0x0000              
			db 0x00                
			db 0x9a                    
			db 0xcf                
			db 0x00                
		DATADESCRIPTOR  equ 0x10
			dw 0xffff              
			dw 0x0000              
			db 0x00                
			db 0x92                
			db 0xcf                
			db 0x00                
		VIDEODESCRIPTOR equ 0x18
			dw 0xffff              
			dw 0x8000              
			db 0x0b                
			db 0x92                
			db 0x40                    
			;db 0xcf                    
			db 0x00                 
		IDTDESCRIPTOR	equ 0x20
			dw 0xffff
			dw 0x0000
			db 0x02
			db 0x92
			db 0xcf
			db 0x00
GDT_END:
IDTR:
		dw 256*8-1
		dd 0x00020000
IDT_IGNORE:
		dw ISR_IGNORE
		dw CODEDESCRIPTOR
		db 0
		db 0x8E
		dw 0x0000
ISR_IGNORE:
		push	gs
		push	fs
		push	es
		push	ds
		pushad
		pushfd
		cli
		nop
		sti
		popfd
		popad
		pop		ds
		pop		es
		pop		fs
		pop		gs
		iret

msgRMode db "Real Mode", 0
msgPMode db "Protected Mode", 0

 
times 	2048-($-$$) db 0x00
