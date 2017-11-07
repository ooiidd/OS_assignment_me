#include <interrupt.h>
#include <device/console.h>
#include <type.h>
#include <device/kbd.h>
#include <device/io.h>
#include <device/pit.h>
#include <stdarg.h>

#define HSCREEN 80
#define VSCREEN 25
#define SIZE_SCREEN HSCREEN * VSCREEN
#define NSCROLL 100
#define SIZE_SCROLL NSCROLL * HSCREEN
#define VIDIO_MEMORY 0xB8000

#define IO_BASE 0x3F8               /* I/O port base address for the first serial port. */
#define FIRST_SPORT (IO_BASE)
#define LINE_STATUS (IO_BASE + 5)   /* Line Status Register(read-only). */
#define THR_EMPTY 0x20              /* Transmitter Holding Reg Empty. */

char next_line[2]; //"\r\n";
#ifdef SCREEN_SCROLL

#define buf_e (buf_w + SIZE_NSCROLL)
#define SCROLL_END buf_s + SIZE_SCROLL

char buf_s[SIZE_SCROLL];
char *buf_w;	// = buf_s;
char *buf_p;	// = buf_s;

char* scr_end;// = buf_s + SIZE_SCROLL;

bool loop_buf = false;
bool a_s = TRUE;

#endif

void init_console(void)
{
	for(int i=0;i<25*80;i++)
		buf_s[i]=0;
	Glob_x = 0;
	Glob_y = 2;
	Glob_kbd_check=0;
	Glob_kbd = 0;

	next_line[0] = '\r';
	next_line[1] = '\r';

#ifdef SCREEN_SCROLL
	buf_w = buf_s;
	buf_p = buf_s;
	loop_buf = false;
	a_s = TRUE;
	scr_end = buf_s + SIZE_SCROLL;
#endif

}

void PrintCharToScreen(int x, int y, const char *pString) 
{
	Glob_x = x;
	Glob_y = y;
	int i = 0;
	while(pString[i] != 0)
	{
		PrintChar(Glob_x++, Glob_y, pString[i++]);
	}
}

void PrintChar(int x, int y, const char String) 
{
#ifdef SCREEN_SCROLL
	//엔터 입력시
	if (String == '\n') {
		//화면의 마지막을 넘어갈때 scroll함수로 들어간다.
		if((y+1) > VSCREEN) {
			scroll();
			y--;
		}
		Glob_x = 0;
		Glob_y = y+1;
		
		//엔터 입력이 키보드에서 들어왔다면 Glob_kbd변수를 0으로 초기화 하여 입력값이 없음을 알려준다.
		if (Glob_kbd_check==1){
			Glob_kbd=0;
		}
		return;
		
	}
	//백스페이스 입력이 들어올 시, 키보드 입력이 있었다면 여기로 들어온다.
	else if(Glob_kbd>0 && Glob_kbd_check==1 && String == '\b'){
		//x가 0이라면 줄을 바꿔줘야 하므로 
		if(x==0){
			char* b = &buf_w[y * HSCREEN + x - 1];//글자를 지울 위치
			if(b >= SCROLL_END){
				b-= SIZE_SCROLL;
			}
			*b=0;//글자를 지워준다.
			Glob_x=79;
			buf_w -= HSCREEN;//buf_w를 한줄 내려 준다.
			while(buf_w < buf_s)
				buf_w += SIZE_SCROLL;
		}
		else{
			char* b = &buf_w[y * HSCREEN + x - 1];//글자를 지울 위치
			if(b >= SCROLL_END){
				loop_buf=true;
				b-= SIZE_SCROLL;
			}
			*b=0;//글자를 지울 위치
			Glob_x-=2;	
		}
		Glob_kbd--;//한글자 지워졌으므로 Glob_kbd를 하나 빼준다.
		return;
	}
	//사용자가 입력하지 않은 문자를 지우려할때
	else if(String =='\b'){
		Glob_x-=1;//Glob_x++로 이 함수에 들어오므로 원위치 시켜준다.
		
		return;
	}
	else {
		if ((y >= VSCREEN) && (x >= 0)) {
			scroll();
			x = 0; y--;
		}
		char* b = &buf_w[y * HSCREEN + x];
		if(b >= SCROLL_END){
			loop_buf=true;
			b-= SIZE_SCROLL;
		}
		*b = String;

		if(Glob_x >= HSCREEN)
		{
			Glob_x = 0;
			Glob_y++;
		}   
		//사용자의 입력에 의한 문자가 들어왔을때, Glob_kbd를 1증가시켜준다.
		if (Glob_kbd_check==1){
			Glob_kbd++;
		}   
	}
#else
	CHAR *pScreen = (CHAR *)VIDIO_MEMORY;


	if (String == '\n') {
		if((y+1) > 24) {
			scroll();
			y--;
		}
		pScreen += ((y+1) * 80);
		Glob_x = 0;
		Glob_y = y+1;
		//tnwjd
		if (Glob_kbd_check==1){
			Glob_kbd=0;
		}
		return;
	}
	//tnwjd
	else if(Glob_kbd>0 && Glob_kbd_check==1 && String == '\b'){
		if(x==0){
			pScreen += (y*80) + x -1;
			pScreen[0].bAtt=0x07;
			pScreen[0].bCh =0;
			Glob_x=79;
			Glob_y=y-1;
		}
		else{
			pScreen += (y*80) + x -1;
			pScreen[0].bAtt=0x07;
			pScreen[0].bCh =0;
			Glob_x-=2;	
		}
		Glob_kbd--;
		return;
	}
	else if(String =='\b'){
		
		Glob_x-=1;	
		
		return;
	}
	else {
		  
		if ((y > 24) && (x >= 0)) {
			x = 0; y--;
		}                       

		pScreen += ( y * 80) + x;
		pScreen[0].bAtt = 0x07;
		pScreen[0].bCh = String;

		if ((y > 23) && (x >= 79)) {
			scroll();
		} 
		if(Glob_x > 79)
		{
			Glob_x = 0;
			Glob_y++;
		}  
		//tnwjd
		if (Glob_kbd_check==1){
			Glob_kbd++;
		}  
	}
#endif
}

void clrScreen(void) 
{
	CHAR *pScreen = (CHAR *) VIDIO_MEMORY;
	int i;

	for (i = 0; i < 80*25; i++) {
		(*pScreen).bAtt = 0x07;
		(*pScreen++).bCh = ' ';
	}   
	Glob_x = 0;
	Glob_y = 0;
}

void scroll(void) 
{
#ifdef SCREEN_SCROLL
	buf_w += HSCREEN;//buf_w를 한줄 내려 준다.
	while(buf_w >= SCROLL_END)
		buf_w -= SIZE_SCROLL;

	//다음에 써줄 라인을 초기화 한다.
	int i;
	char* b = &buf_w[80*24];
	if( b >= SCROLL_END){
		b -=SIZE_SCROLL;
	}
	for(i = 0; i < HSCREEN; i++)
		b[i] = 0;
#else
	CHAR *pScreen = (CHAR *) VIDIO_MEMORY;
	CHAR *pScrBuf = (CHAR *) (VIDIO_MEMORY + 2*80);
	int i;
	for (i = 0; i < 80*24; i++) {
		    (*pScreen).bAtt = (*pScrBuf).bAtt;
		        (*pScreen++).bCh = (*pScrBuf++).bCh;
	}   
	for (i = 0; i < 80; i++) {
		    (*pScreen).bAtt = 0x07;
		        (*pScreen++).bCh = ' ';
	} 
#endif
	Glob_y--;

}

#ifdef SERIAL_STDOUT
void printCharToSerial(const char *pString)
{
	int i;
	enum intr_level old_level = intr_disable();
	for(;*pString != NULL; pString++)
	{
		if(*pString != '\n'){
			while((inb(LINE_STATUS) & THR_EMPTY) == 0)
				continue;
			outb(FIRST_SPORT, *pString);

		}
		else{
			for(i=0; i<2; i++){
				while((inb(LINE_STATUS) & THR_EMPTY) == 0)
					continue;
				outb(FIRST_SPORT, next_line[i]);
			}
		}
	}
	intr_set_level (old_level);
}
#endif


int printk(const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsprintk(buf, fmt, args);
	va_end(args);
	
#ifdef SERIAL_STDOUT
	printCharToSerial(buf);
#endif
	PrintCharToScreen(Glob_x, Glob_y, buf);
	set_cursor();//커서를 설정해 준다.
	
	return len;
}
//키보드에서 누르면 printk가 아닌 아래 함수로 들어온다.
//printk와 동작은 비슷하고 Glob_kbd_check변수만 바꿔준다.
int printk_by_kbd(const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsprintk(buf, fmt, args);
	va_end(args);
	Glob_kbd_check=1;//이 함수를 1로 바꿈으로써 키보드 입력이 있었음을 알려준다.
	
#ifdef SERIAL_STDOUT
	printCharToSerial(buf);
#endif
	PrintCharToScreen(Glob_x, Glob_y, buf);
	Glob_kbd_check=0;//PrintCharToScreen함수가 끝난 후에는 다시 이 변수를 0으로 바꿔서 오류가 없게 한다.
	set_cursor();//커서를 재설정해 준다.
	return len;
}

//커서를 설정하는 함수.
void set_cursor(void)
{
	unsigned short position;
	//a_s가 true 거나 buf_p가 buf_w와 같다면 커서가 보여야 하므로 커서 위치를 설정해준다.
	if(a_s||buf_p==buf_w){
		position = (Glob_y*80)+Glob_x;
	}
	else{//그렇지 않으면 커서를 없애줘야한다. 커서 y좌표를 충분히 내려서 안보이게 한다.
		position = ((Glob_y+25)*80)+Glob_x;
	}
	//어셈블리어 out 명령으로 레지스터 0x03d4와 0x03d5를 이용하여 위에서 설정한 position값으로 커서를 이동시킨다.
	outb(0x03d4,0x0f);
	outb(0x03d5,(unsigned char)position&0xff);

	outb(0x03d4,0x0e);
	outb(0x03d5,(unsigned char)((position>>8)&0xff));
}
#ifdef SCREEN_SCROLL
void scroll_screen(int offset)
{
	if(a_s == TRUE && offset > 0 && buf_p == buf_w)
		return;

	a_s = FALSE;
	char * scroll_minus_p;

	if((int)buf_p-80*25<(int)buf_s)//scroll_minus_p에 buf_p에서부터 위로 25줄 올라간 위치를 저장한다.
		scroll_minus_p = (char*)((int)buf_p-80*25+SIZE_SCROLL);
	else
		scroll_minus_p = (char*)((int)buf_p-80*25);


	if(offset==1 && (int)buf_p==(int)buf_w)//page_down키가 눌렸을 때, 더이상 못내려가게 한다.
		return;
	if(loop_buf==false && offset==-1)//page_up키가 눌렸을 때, buf_p가 buf_s까지 올라갔다면, 더이상 못올라가게 한다. loop_buf가 false이므로 아직 한바퀴도 돌지 않았기 때문.
	{
		if(buf_p==buf_s)
			return;
	}
	if(loop_buf==true && offset==-1 && scroll_minus_p==buf_w){//한바퀴 돈 후로는 page_up키를 누르면 buf_p에서 25줄 위로 올라가서 그것이 buf_w과 같다면 더이상 올라가지 못하게 한다.
		return;
	}
	
	buf_p = (char*)((int)buf_p + (HSCREEN * offset));

	
	if(buf_p >= SCROLL_END)
		buf_p = (char*)((int)buf_p - SIZE_SCROLL);
	else if(buf_p < buf_s)
		buf_p = (char*)((int)buf_p + SIZE_SCROLL);
	
	set_cursor();//커서를 설정한다.
}

void set_fallow(void)
{
	a_s = TRUE;
}



void refreshScreen(void)
{
	CHAR *p_s= (CHAR *) VIDIO_MEMORY;
	int i;

	if(a_s)
		buf_p = buf_w;

	char* b = buf_p;

	for(i = 0; i < SIZE_SCREEN; i++, b++, p_s++)
	{
		if(b >= SCROLL_END)
			b -= SIZE_SCROLL;
		p_s->bAtt = 0x07;
		p_s->bCh = *b;
	}
}
#endif

