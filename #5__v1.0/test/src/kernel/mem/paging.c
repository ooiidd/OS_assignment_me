#include <device/io.h>
#include <mem/mm.h>
#include <mem/paging.h>
#include <device/console.h>
#include <proc/proc.h>
#include <interrupt.h>
#include <mem/palloc.h>
#include <ssulib.h>

uint32_t *PID0_PAGE_DIR;
uint32_t page_cnt;

intr_handler_func pf_handler;

uint32_t scale_up(uint32_t base, uint32_t size)
{
	uint32_t mask = ~(size-1);
	if(base & mask != 0)
		base = base & mask + size;
	return base;
}

uint32_t scale_down(uint32_t base, size_t size)
{
	uint32_t mask = ~(size-1);
	if(base & mask != 0)
		base = base & mask - size;
	return base;
}

void init_paging()
{
	uint32_t *page_dir = palloc_get_page();
	uint32_t *page_tbl = palloc_get_page();
	page_dir = VH_TO_RH(page_dir);
	page_tbl = VH_TO_RH(page_tbl);
	PID0_PAGE_DIR = page_dir;

	page_cnt = 1024;
	int NUM_PT, NUM_PE;
	uint32_t cr0_paging_set;
	int i;

	NUM_PE = mem_size() / PAGE_SIZE;
	NUM_PT = NUM_PE / 1024;
	printk("-PE=%d, PT=%d\n", NUM_PE, NUM_PT);
	printk("-page dir=%x page tbl=%x\n", page_dir, page_tbl);
	page_dir[0] = (uint32_t)page_tbl | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;
	
	NUM_PE = RKERNEL_HEAP_START / PAGE_SIZE;
	for ( i = 0; i < NUM_PE; i++ ) {
		page_tbl[i] = (PAGE_SIZE * i)
			| PAGE_FLAG_RW
			| PAGE_FLAG_PRESENT;
		//writable & present
	}


	//CR0레지스터 설정
	cr0_paging_set = read_cr0() | CR0_FLAG_PG;		// PG bit set

	//컨트롤 레지스터 저장
	write_cr3( (unsigned)page_dir );		// cr3 레지스터에 PDE 시작주소 저장
	write_cr0( cr0_paging_set );          // PG bit를 설정한 값을 cr0 레지스터에 저장

	reg_handler(14, pf_handler);
}

void memcpy_hard(void* from, void* to, uint32_t len)
{
	write_cr0( read_cr0() & ~CR0_FLAG_PG);
	memcpy(from, to, len);
	write_cr0( read_cr0() | CR0_FLAG_PG);
}

uint32_t* get_cur_pd()
{
	return (uint32_t*)read_cr3();
}

uint32_t pde_idx_addr(uint32_t* addr)
{
	uint32_t ret = ((uint32_t)addr & PAGE_MASK_PDE) >> PAGE_OFFSET_PDE;
	return ret;
}

uint32_t pte_idx_addr(uint32_t* addr)
{
	uint32_t ret = ((uint32_t)addr & PAGE_MASK_PTE) >> PAGE_OFFSET_PTE;
	return ret;
}

uint32_t* pt_pde(uint32_t pde)
{
	uint32_t * ret = (uint32_t*)(pde & PAGE_MASK_BASE);
	return ret;
}

uint32_t* pt_addr(uint32_t* addr)
{
	uint32_t idx = pde_idx_addr(addr);
	uint32_t* pd = get_cur_pd();
	return pt_pde(pd[idx]);
}

uint32_t* pg_addr(uint32_t* addr)
{
	uint32_t *pt = pt_addr(addr);
	uint32_t idx = pte_idx_addr(addr);
	uint32_t *ret = (uint32_t*)(pt[idx] & PAGE_MASK_BASE);
	return ret;
}

/*
    page table 복사
*/
/*page table 로 사용할 page를 새로 할당 받은 후 인자로 받은 destination
directory의 인자로 받은 idx번째 항목에 추가
*/
void  pt_copy(uint32_t *pd, uint32_t *dest_pd, uint32_t idx)
{
	uint32_t *pt = pt_pde(pd[idx]);
	pt = RH_TO_VH(pt);
	uint32_t i;

	//page table로 사용할 page를 새로 할당
	uint32_t *new_tbl = palloc_get_page();
	dest_pd[idx] = ((uint32_t)new_tbl |  (pd[idx] & 0xFDF)) - VKERNEL_HEAP_START + RKERNEL_HEAP_START;
	
	for(i = 0; i<1024; i++)
	{
		if(pt[i] & PAGE_FLAG_PRESENT)
		{
			//present비트가 설정되어 있다면 새 테이블에 복사를 합니
			new_tbl[i] = (uint32_t)pt[i];
		}
	}
}

/*
    page directory 복사. 
pd_copy() 함수는 source page directory에서 항목들을 검사해 PRESENT 비트가 설정되어 있는 항목들을 찾아 pt_copy() 함수를 호출 
*/
void pd_copy(uint32_t* from, uint32_t* to)
{
	uint32_t i;

	for(i = 0; i < 1024; i++)
	{
		if(from[i] & PAGE_FLAG_PRESENT){
			pt_copy(from, to, i);
		}
	}
}

uint32_t* pd_create (pid_t pid)
{
	uint32_t *pd = palloc_get_page();
	pd_copy(RH_TO_VH(PID0_PAGE_DIR),pd);

	pd = VH_TO_RH(pd);
	//할당 받아 page directory로 사용하는 page를 실제 메모리 주소로 변환하여 리턴 
	return pd;
}

void pf_handler(struct intr_frame *iframe)
{
	void *fault_addr;

	asm ("movl %%cr2, %0" : "=r" (fault_addr));

	printk("Page fault : %X\n",fault_addr);
#ifdef SCREEN_SCROLL
	refreshScreen();
#endif
	//cr0 레지스터를 피지컬 주소도 사용할 수 있도록 변경
	write_cr0( read_cr0() & ~CR0_FLAG_PG);
	uint32_t *page_dir;	//페이지 디렉토리 선언
	uint32_t *page_tbl;	//페이지 테이블 선

/*
페이지 폴트가 난 주소의 상위 10비트를 pde_idx로, 그다음 10비트를 pte_idx에 저장하기위해 사용
페이지 테이블이 새로 생성되었을 경우 자신을 참조할 수 있도록 해주는 table_add_idx 변수도 사용.
*/
	uint32_t pde_idx,pte_idx,table_add_idx;	
	//페이지 디렉토리 시작 주소를 가져온다
	page_dir = get_cur_pd();
	//page_dir = RH_TO_VH(page_dir);
	//find index
	pde_idx = pde_idx_addr(fault_addr);
	pte_idx = pte_idx_addr(fault_addr);
	//page table miss
	if(page_dir[pde_idx] == 0){
		page_tbl = palloc_get_page();
		table_add_idx = pte_idx_addr( (uint32_t)page_tbl );
		
		page_tbl = VH_TO_RH(page_tbl);	

		//자기 자신위치 초기화
		page_tbl[table_add_idx] = (uint32_t)page_tbl | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;
		//페이지 디렉토리 설정
		page_dir[pde_idx] = (uint32_t)page_tbl | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;
	}else{//페이지 테이블이 있을 경우
		//해당 페이지 테이블의 시작 주소를 페이지 디렉토리로부터 가져옴
		page_tbl = page_dir[pde_idx] & 0xfffff000 ;
	}
	//해당 페이지 테이블의 pte_idx인덱스 설정
	page_tbl[pte_idx] = (uint32_t) VH_TO_RH(fault_addr) | PAGE_FLAG_RW | PAGE_FLAG_PRESENT;
	
	
	//cr0 레지스터를 다시 가상주소를 사용하도록 변경
	write_cr0( read_cr0() | CR0_FLAG_PG);
}
