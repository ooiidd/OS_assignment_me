#include <device/kbd.h>
#include <type.h>
#include <device/console.h>
#include <interrupt.h>
#include <device/io.h>
#include <ssulib.h>

static Key_Status KStat;
static char kbd_buf[BUFSIZ];
int buf_head, buf_tail;

static BYTE Kbd_Map[4][KBDMAPSIZE] = {
	{ /*  default */
		0x00, 0x00, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
		'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0x00, 'a', 's',
		'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0x00, '\\', 'z', 'x', 'c', 'v',
		'b', 'n', 'm', ',', '.', '/', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 128, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 129, 0x00, 0x00, 0x00, 0x00
	},
	{ /* capslock  */
		0x00, 0x00, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', 0x00, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', 0x00, '\\', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 128, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 129, 0x00, 0x00, 0x00, 0x00
	},
	{ /* Shift */
		0x00, 0x00, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', 0x00,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0x00, 0x00, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0x00, '|', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 128, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 129, 0x00, 0x00, 0x00, 0x00
	},
	{ /* Capslock & Shift */
		0x00, 0x00, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', 0x00,
		'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', 0x00, 0x00, 'a', 's',
		'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '\"', '~', 0x00, '|', 'z', 'x', 'c', 'v',
		'b', 'n', 'm', '<', '>', '?', 0x00, 0x00, 0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 128, '-', 0x00, 0x00, 0x00, '+', 0x00,
		0x00, 129, 0x00, 0x00, 0x00, 0x00
	}
};

void init_kbd(void)
{
	KStat.ShiftFlag = 0;
	KStat.CapslockFlag = 0;
	KStat.NumlockFLag = 0;
	KStat.ScrolllockFlag = 0;
	KStat.ExtentedFlag = 0;
	KStat.PauseFlag = 0;

	buf_head = 0;
	buf_tail = 0;

	reg_handler(33, kbd_handler);
}

void UpdateKeyStat(BYTE Scancode)
{
	if(Scancode & 0x80)
	{
		if(Scancode == 0xB6 || Scancode == 0xAA)
		{
			KStat.ShiftFlag = FALSE;
		}
	}
	else
	{
		if(Scancode == 0x3A && KStat.CapslockFlag)
		{
			KStat.CapslockFlag = FALSE;
		}
		else if(Scancode == 0x3A)
			KStat.CapslockFlag = TRUE;
		else if(Scancode == 0x36 || Scancode == 0x2A)
		{
			KStat.ShiftFlag = TRUE;
		}
	}

	if(Scancode == 0xE0)
	{
		KStat.ExtentedFlag = TRUE;
	}
	else if(KStat.ExtentedFlag == TRUE && Scancode != 0xE0)
	{
		KStat.ExtentedFlag = FALSE;
	}
}

BOOL ConvertScancodeToASCII(BYTE Scancode, BYTE *Asciicode)
{
	if(KStat.PauseFlag > 0)
	{
		KStat.PauseFlag--;
		return FALSE;
	}
	if(Scancode == 0xE1)
	{
		*Asciicode = 0x00;
		KStat.PauseFlag = 2;
		return FALSE;
	}
	else if(Scancode == 0xE0)
	{
		*Asciicode = 0x00;
		return FALSE;
	}
	if(!(Scancode & 0x80))
	{
		if(KStat.ShiftFlag & KStat.CapslockFlag)
		{
			*Asciicode = Kbd_Map[3][Scancode & 0x7F];
		}
		else if(KStat.ShiftFlag)
		{
			*Asciicode = Kbd_Map[2][Scancode & 0x7F];
		}
		else if(KStat.CapslockFlag)
		{
			*Asciicode = Kbd_Map[1][Scancode & 0x7F];
		}
		else
		{
			*Asciicode = Kbd_Map[0][Scancode];
		}

		return TRUE;
	}
	return FALSE;
}

bool isFull()
{
	return (buf_head+1) % BUFSIZ == buf_tail;
}

bool isEmpty()
{
	return buf_head == buf_tail;
}


void kbd_handler(struct intr_frame *iframe)
{
	BYTE asciicode;
	BYTE data = inb(0x60);

	UpdateKeyStat(data);
	if(ConvertScancodeToASCII(data, &asciicode))
	{
		if( !isFull() && asciicode != 0)
		{
			//printk("%c", asciicode);
			kbd_buf[buf_tail] = asciicode;
			buf_tail = (buf_tail + 1) % BUFSIZ;
		}


#ifdef SCREEN_SCROLL
		switch(asciicode)
		{
			case 128:
				scroll_screen(-1);
				buf_tail = (buf_tail-1) % BUFSIZ;
				kbd_buf[buf_tail] = 0x00;
				break;
			case 129 :
				scroll_screen(+1);
				buf_tail = (buf_tail-1) % BUFSIZ;
				kbd_buf[buf_tail] = 0x00;
				break;
			default:
				set_fallow();
				break;
		}
#else
		//tnwjd
		switch(asciicode)
		{
			case 128: // page up
				buf_tail = (buf_tail-1) % BUFSIZ;
				kbd_buf[buf_tail] = 0x00;
				break;
			case 129: // page down
				buf_tail = (buf_tail-1) % BUFSIZ;
				kbd_buf[buf_tail] = 0x00;
				break;
			default:
				break;
		}
#endif
	}
}

char kbd_read_char()
{
	if( isEmpty())
		return -1;

	char ret;
	ret = kbd_buf[buf_head];
	buf_head = (buf_head + 1)%BUFSIZ;
	//이 곳에서 출력
	printk_by_kbd("%c", ret);
	return ret;
}
