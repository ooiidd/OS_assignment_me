#include <list.h>
#include <proc/sched.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <ssulib.h>
#include <interrupt.h>
#include <proc/sched.h>
#include <syscall.h>
#include <mem/palloc.h>

#define STACK_SIZE 512

struct list plist;				// All Process List
/*

struct list pr_pool1;			// Priority array, this will start with 'active' array.
struct list pr_pool2;
struct list active;				// Pointing active_pool
struct list expired;			// Pointing expired_pool
*/
struct list active[7][20];
struct list expired[7][20];
struct list rlist;				// Running Process List
struct list slist;				// Sleep Process List
struct list dlist;				// Deleted Process List

struct process procs[PROC_NUM_MAX];
struct process *cur_process;
struct process *idle_process;
int pid_num_max;
uint32_t process_stack_ofs;
static int lock_pid_simple; 
static int lately_pid;

bool more_prio(const struct list_elem *a, const struct list_elem *b, void *aux);
bool less_time_sleep(const struct list_elem *a, const struct list_elem *b, void *aux);
pid_t getValidPid(int *idx);
void proc_start(void);
void proc_end(void);

void kernel1_proc(void *aux);
void kernel2_proc(void *aux);
void kernel3_proc(void *aux);

/*
Hint : use 'printk' function to trace function call
*/
//초기화 합니다.
void init_proc()
{
	int i, j, p_row, p_col;
	process_stack_ofs = offsetof (struct process, stack);

	lock_pid_simple = 0;
	lately_pid = -1;

	list_init(&plist);
	list_init(&rlist);
	list_init(&slist);
	list_init(&dlist);
	
	//배열 초기화
	for(i=0;i<7;i++)
		for(j=0;j<20;j++)
			list_init(&active[i][j]);
	for(i=0;i<7;i++)
		for(j=0;j<20;j++)
			list_init(&expired[i][j]);	

	for (i = 0; i < PROC_NUM_MAX; i++)
	{
		procs[i].pid = i;
		procs[i].state = PROC_UNUSED;
		procs[i].parent = NULL;
	}

	pid_t pid = getValidPid(&i);
	cur_process = (int *)&procs[0];
	idle_process = (int *)&procs[0];

	cur_process->pid = pid;
	cur_process->parent = NULL;
	cur_process->state = PROC_RUN;

	cur_process -> nice = 0;
	cur_process -> rt_priority = 0;
	cur_process -> priority = 0;

	cur_process->stack = 0;
	cur_process->pd = (void*)read_cr3();
	cur_process -> elem_all.prev = NULL;
	cur_process -> elem_all.next = NULL;
	cur_process -> elem_stat.prev = NULL;
	cur_process -> elem_stat.next = NULL;

	list_push_back(&plist, &cur_process->elem_all);
	//list_push_back(&rlist, &cur_process->elem_stat);
}

pid_t getValidPid(int *idx) {
	pid_t pid = -1;
	int i;

	while(lock_pid_simple);

	lock_pid_simple++;

	for(i = 0; i < PROC_NUM_MAX; i++)
	{
		int tmp = i + lately_pid + 1;
		if(procs[tmp % PROC_NUM_MAX].state == PROC_UNUSED) { 
			pid = lately_pid + 1;
			*idx = tmp % PROC_NUM_MAX;
			break;
		}
	}

	if(pid != -1)
		lately_pid = pid;	

	lock_pid_simple = 0;

	return pid;
}
//프로세스를 생성합니다.
pid_t proc_create(proc_func func, struct proc_option *opt, void* aux)
{
	struct process *p;
	int idx, p_row, p_col;
	int i,j;

	enum intr_level old_level = intr_disable();

	pid_t pid = getValidPid(&idx);
	p = &procs[pid];
	p->pid = pid;
	p->state = PROC_RUN;

	if(opt != NULL) {
		p -> nice = opt->nice;	
		p -> rt_priority = opt->rt_priority;  
	}
	else {
		p -> nice = 20;
		p -> rt_priority = (unsigned char)45;
	}
	//priority 설정
	p -> priority = ((int)(p->nice))+((int)(p->rt_priority));
	p->time_used = 0;
	p->time_slice = 0;
	p->parent = cur_process;
	p->simple_lock = 0;
	p->child_pid = -1;
	p->pd = pd_create(p->pid);

	int *top = (int*)palloc_get_page();
	int stack = (int)top;
	top = (int*)stack + STACK_SIZE - 1;

	*(--top) = (int)aux;		//argument for func
	*(--top) = (int)proc_end;	//return address from func
	*(--top) = (int)func;		//return address from proc_start
	*(--top) = (int)proc_start; //return address from switch_process

	*(--top) = (int)((int*)stack + STACK_SIZE - 1); //ebp
	*(--top) = 1; //eax
	*(--top) = 2; //ebx
	*(--top) = 3; //ecx
	*(--top) = 4; //edx
	*(--top) = 5; //esi
	*(--top) = 6; //edi

	p -> stack = top;
	p -> elem_all.prev = NULL;
	p -> elem_all.next = NULL;
	p -> elem_stat.prev = NULL;
	p -> elem_stat.next = NULL;
	
	p_row = (p->priority)/20;
	p_col = (p->priority)%20;

	list_push_back(&plist, &p->elem_all);
	//list_push_back(&rlist, &p->elem_stat);
	list_push_back(&active[p_row][p_col], &p->elem_stat);//active배열에 현재 프로세스의 elem_stat을 넣어준다.

	intr_set_level (old_level);
	//printk("create %d\n",p->pid);
	return p->pid;
}

void* getEIP()
{
    return __builtin_return_address(0);
}

void  proc_start(void)
{
	//printk("proc_start %d \n",cur_process->pid);
	intr_enable ();
	return;
}

void proc_free(void)
{
	uint32_t pt = *(uint32_t*)cur_process->pd;
	cur_process->parent->child_pid = cur_process->pid;
	cur_process->parent->simple_lock = 0;

	cur_process->state = PROC_ZOMBIE;
	list_push_back(&dlist, &cur_process->elem_stat);

	palloc_free_page(cur_process->stack);
	palloc_free_page((void*)pt);
	palloc_free_page(cur_process->pd);

	list_remove(&cur_process->elem_stat);
	list_remove(&cur_process->elem_all);
}

//프로세스를 종료합니다.
void proc_end(void)
{
	//printk("proc_end %d \n",cur_process->pid);
	proc_free();
	schedule();
	printk("never reach\n");
	return;
}
//proc_sleep에서 넣어준 slist를 탐방하며 현재시간에 깨워줄 프로세스가 있다면 깨워줍니다.
void proc_wake(void)
{
	struct process* p;
	int p_row, p_col;
	int old_level;
	unsigned long long t = get_ticks();

    while(!list_empty(&slist))
	{
		p = list_entry(list_front(&slist), struct process, elem_stat);
		if(p->time_sleep > t)
			break;
		
		//printk("proc_wake %d \n",p->pid);
		list_remove(&p->elem_stat);
		p_row = (p->priority)/20;
		p_col = (p->priority)%20;
		//행열에 맞는 active에 다시 넣어줍니다.
		list_push_back(&active[p_row][p_col], &p->elem_stat);
		p->state = PROC_RUN;
	}
}
//깨워줄 시간을 결정하고 slist에 넣어줍니다.
void proc_sleep(unsigned ticks)
{
	unsigned long cur_ticks = get_ticks();
	
	//깨워줄 시간을 넣어줍니다. 현재tick + 60tick
	cur_process->time_sleep =  cur_ticks+ticks;
	cur_process->state = PROC_STOP;
	cur_process->time_slice = 0;

	printk("Proc %d I/O at %d\n",cur_process->pid,cur_process->time_used);
	list_remove(&cur_process->elem_stat);
	list_insert_ordered(&slist, &cur_process->elem_stat,
			less_time_sleep, NULL);
	schedule();
}

void proc_block(void)
{
	//printk("proc_block\n");
	cur_process->state = PROC_BLOCK;
	schedule();	
}

void proc_unblock(struct process* proc)
{
	//printk("unblock\n");
	enum intr_level old_level;
	int p_row, p_col;
	
	list_push_back(&rlist, &proc->elem_stat);
	proc->state = PROC_RUN;
}     

bool less_time_sleep(const struct list_elem *a, const struct list_elem *b,void *aux)
{
	struct process *p1 = list_entry(a, struct process, elem_stat);
	struct process *p2 = list_entry(b, struct process, elem_stat);

	return p1->time_sleep < p2->time_sleep;
}

bool more_prio(const struct list_elem *a, const struct list_elem *b,void *aux)
{
	struct process *p1 = list_entry(a, struct process, elem_stat);
	struct process *p2 = list_entry(b, struct process, elem_stat);
	
	return p1->priority > p2->priority;
}

//프로세스1의 세부 작업
void kernel1_proc(void* aux)
{
	int sleepNum=0;
	while(1)
	{
		//I/O작업
		if(cur_process -> time_used==80 && sleepNum==0){
			enum intr_level old_level = intr_disable();
			proc_sleep(60);
			sleepNum++;
			intr_set_level (old_level);
		}
		//time used체크
		if(cur_process -> time_used > 200)
			break;
	}
	proc_end();
}
//프로세스2의 세부작업
void kernel2_proc(void* aux)
{
	int sleepNum=0;
	while(1)
	{
		//I/O작업
		if(cur_process -> time_used == 110 && sleepNum==0){
			enum intr_level old_level = intr_disable();
			proc_sleep(60);
			sleepNum++;
			intr_set_level (old_level);
		}
		//time used체크
		if(cur_process -> time_used >120)
			break;
	}
	proc_end();
}
//프로세스 3의 세부작업
void kernel3_proc(void* aux)
{
	int sleepNum=0;
	while(1)
	{
		//I/O작업
		if(cur_process -> time_used == 20 && sleepNum==0){
			enum intr_level old_level = intr_disable();
			proc_sleep(60);
			sleepNum++;
			intr_set_level (old_level);
		}
		//time used체크
		if(cur_process -> time_used >300)
			break;
	}
	proc_end();
}

/*
Let's say PLIST_COL is 5 and PLIST_ROW is 4, then the location of process with priority 7 is :
7/5 = 1
7%5 = 2
   0  1  2  3  4
0 [ ][ ][ ][ ][ ]
1 [ ][ ][*][ ][ ]
2 [ ][ ][ ][ ][ ]
3 [ ][ ][ ][ ][ ]

이번 과제의 priority값은 0~139이므로 col을 14로 설정하면, row 값의 최대는 9까지만 나오므로 row가 10이고 col이 14인 배열을
만들어 준다면 priority가 다르면 각각 다른 행렬에 들어갈 수 있게 된다. prior[10][14].
*/

// idle process, pid = 0
void idle(void* aux)
{
	//printk("idle start\n");
	struct proc_option p_opt1 = { .nice = 20, .rt_priority = 70};
	struct proc_option p_opt2 = { .nice = 20, .rt_priority = 50};
	//struct proc_option p_opt3 = { .nice = 15, .rt_priority = 65};

	proc_create(kernel1_proc, &p_opt1, NULL);
	proc_create(kernel2_proc, &p_opt2, NULL);
	//proc_create(kernel3_proc, &p_opt3, NULL);
	while(1) {  
		//printk("idle while\n");
		schedule();    
	}
}

void proc_print_data()
{
	int a, b, c, d, bp, si, di, sp;

	//eax ebx ecx edx
	__asm__ __volatile("mov %%eax ,%0": "=m"(a));

	__asm__ __volatile("mov %ebx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(b));
	
	__asm__ __volatile("mov %ecx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(c));
	
	__asm__ __volatile("mov %edx ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(d));
	
	//ebp esi edi esp
	__asm__ __volatile("mov %ebp ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(bp));

	__asm__ __volatile("mov %esi ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(si));

	__asm__ __volatile("mov %edi ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(di));

	__asm__ __volatile("mov %esp ,%eax");
	__asm__ __volatile("mov %%eax ,%0": "=m"(sp));

	printk(	"\neax %o ebx %o ecx %o edx %o"\
			"\nebp %o esi %o edi %o esp %o\n"\
			, a, b, c, d, bp, si, di, sp);
}

void hexDump (void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    if (len == 0) {
        printk("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printk("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0)
                printk ("  %s\n", buff);

            printk ("  %04x ", i);
        }

        printk (" %02x", pc[i]);

        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    while ((i % 16) != 0) {
        printk ("   ");
        i++;
    }

    printk ("  %s\n", buff);
}



