org	0x7c00
[BITS 16]

START:   
mov		ax, 0xb800
mov		es, ax
mov		ax, 0x00
mov		bx, 0
mov		cx, 80*25*2
mov		si, 0;msg주소의 +몇번째 주소로 접근하기위해 0으로 초기화해준다.

PRINT:

mov		dl, byte[msg+si];msg주소에서 1바이트를 si만큼 이동하여 dl에 복사한다.
mov 	byte[es:bx], dl;세그먼트인 es레지스터 0xb800 에 오프셋 bx주소에 dl을 복사한다.
add		bx, 1;세그먼트 es레지스터의 오프셋을 1만큼 증가시켜준다 
add		si, 1;msg에 접근할 si를 1만큼 증가시켜준다.
mov		byte[es:bx], 0x07;char데이터인 첫1바이트와 글자속성인 두 번째 1바이트인 지금 설정하고 있는 0x07을 속성값으로 하여 한 글자를 만들어준다.
add		bx, 1; 한 글자를 만들었으므로 bx를 1만큼 증가시켜 es레지스터의 오프셋을 증가시켜준다.
cmp		dl, 0; msg db의 마지막에 0을 넣어줬으므로 문장의 끝을 확인하기 위해 dl과 비교한다. 
jne		PRINT; dl이 0이아니라면 아직 출력할 문자가 남았으므로 PRINT로 점프한다.

CLS:
mov		[es:bx], ax
add		bx, 1
loop 	CLS

msg db "Hello, Sunghoon's World", 0;문자열을 배열에 저장한다.
