#include <list.h>
#include <proc/sched.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <proc/switch.h>
#include <interrupt.h>

extern struct list plist;
extern struct list rlist;
extern struct list active[7][20];
extern struct list expired[7][20];
extern struct process *idle_process;
extern struct process procs[PROC_NUM_MAX];

bool more_prio(const struct list_elem *a, const struct list_elem *b,void *aux);
struct process* get_next_proc(struct list *rlist_target);
int scheduling;									// interrupt.c

// Browse the 'active' array
struct process* sched_find_set(void) {
	struct process * result = NULL;

	for(int i=0;i<7;i++)
		for(int j=0;j<20;j++){
			if(!list_empty(&active[i][j])){
				result = get_next_proc(&active[i][j]);	
				if(result != NULL){
					//printk("find %d : %d,%d \n",result->pid,i,j);
					return result;
				}
			}
		}
	
	return result;
}
//���డ���� ���μ����� �ִٸ� return���ݴϴ�.
struct process* get_next_proc(struct list *rlist_target) {
	struct list_elem *e=NULL;

	for(e = list_begin (rlist_target); e != list_end (rlist_target);
		e = list_next (e))
	{
		struct process* p = list_entry(e, struct process, elem_stat);

		if(p->state == PROC_RUN){
			//printk("??%d %d\n",p->pid,p->priority);
			return p;
		}
	}
	return NULL;
}

void schedule(void)
{	

	struct process *cur=NULL;
	struct process *next=NULL;
	struct process *tmp=NULL;
	struct list_elem *e=NULL;
	int left_print=1,p_row=0,p_col=0,next_pid;//���� 1���� ������� ','�� ���� ������ ���� left_print���

	//0�� ���μ����� �ƴ϶�� �� if���� ������ return
	if(cur_process->pid !=0){
		//time_slice�� 60�̶�� �ش� ���μ����� expired�� �Ű��ش�.
		if(cur_process->time_slice == 60 ){
			list_remove(&cur_process->elem_stat);
			p_row = ((int)(cur_process->priority))/20;
			p_col = ((int)(cur_process->priority))%20;
			list_push_back(&expired[p_row][p_col],&cur_process->elem_stat);
		}
		cur = cur_process;
		next = idle_process;
		cur_process = next;
		
		//���ؽ�Ʈ ����ġ�ϱ����� ���ͷ�Ʈ�� ���ش�.
		enum intr_level old_level = intr_disable();
		switch_process(cur,next);
		//���ؽ�Ʈ ����ġ ������ ���ͷ�Ʈ�� �ٽ� ���ش�.
		intr_set_level (old_level);
		return;
	}
	
	//������ ���μ����� �ִٸ� ���ش�.
	proc_wake();
	//next�� ������ ���� ���μ����� ��´�
	next = sched_find_set();

	int processCount=0;
	//active�� �ƹ� �͵� ���� �����̸� NULL�� ���� �ϹǷ� expired�迭���� active�迭�� �����Ѵ�.
	if(next == NULL){
		for(int i=0;i<7;i++)
			for(int j=0;j<20;j++){
				if(!list_empty(&expired[i][j])){
					struct list_elem *e;
					for(e = list_begin (&expired[i][j]); e != list_end (&expired[i][j]);e = list_next (e))
					{
						struct process* p = list_entry(e, struct process, elem_stat);
						if(p->pid > 256)
							break;
						list_remove(&cur_process->elem_stat);
						list_push_back(&active[i][j],&p->elem_stat);
						break;
					}
				}
			}
		//expired�迭�� �ʱ�ȭ �Ѵ�.
		for(int i=0;i<7;i++)
			for(int j=0;j<20;j++)
				list_init(&expired[i][j]);
		next = sched_find_set();
	}
	//���� ������ ���μ����� ������ݴϴ�.
	for(e=list_begin(&plist);e != list_end(&plist); e = list_next(e)){
		tmp = list_entry (e, struct process, elem_all);
		if(next->pid ==0){
			next = idle_process;
			continue;
		}
		if(tmp->pid == 0)
			continue;
		if(tmp->state != PROC_RUN)
			continue;
		if(left_print == 1)
		{
			printk("#= %2d p= %3d c= %3d u= %3d",tmp->pid,tmp->priority,tmp->time_slice,tmp->time_used);
			left_print=0;
		}
		else
		{
			printk(", #= %2d p= %3d c= %3d u= %3d",tmp->pid,tmp->priority,tmp->time_slice,tmp->time_used);
		}
	}
	if(next->pid != 0){
		printk("\n");
		printk("Selected : %d \n", next->pid);
	}




	cur = cur_process;
	cur_process = next;
	cur_process -> time_slice = 0;
	
	//���ͷ�Ʈ�� ���ݴϴ�.
	enum intr_level old_level = intr_disable();
	switch_process(cur, next);
	//���ؽ�Ʈ ����ġ �Ŀ� �ٽ� ���ͷ�Ʈ�� ���ݴϴ�.
	intr_set_level (old_level);
}
