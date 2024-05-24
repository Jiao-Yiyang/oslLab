#include "klib.h"
#include "vme.h"
#include "proc.h"

static TSS32 tss;

void init_gdt() {
  static SegDesc gdt[NR_SEG];
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_USER);
  gdt[SEG_TSS]   = SEG16(STS_T32A,     &tss,  sizeof(tss)-1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
}

typedef union free_page {
  union free_page *next;
  char buf[PGSIZE];
} page_t;

page_t *free_page_list;

void set_tss(uint32_t ss0, uint32_t esp0) {
  tss.ss0 = ss0;
  tss.esp0 = esp0;
}

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));



void init_page() {
  extern char end;
  panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)");
  static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
  static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
  static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
  static_assert(sizeof(PD) == PGSIZE, "PD must be one page");
  // Lab1-4: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
  // 初始化页表
for (int i = 0; i < PHY_MEM / PT_SIZE; ++i) {
  // 设置页目录项（PDE）
  kpd.pde[i].val = MAKE_PDE(&kpt[i], 3);
  for (int j = 0; j < NR_PTE; ++j) {
    // 设置页表项（PTE）
    kpt[i].pte[j].val = MAKE_PTE((i << DIR_SHIFT) | (j << TBL_SHIFT), 3);
  }
}
  kpt[0].pte[0].val = 0;
  set_cr3(&kpd);
  set_cr0(get_cr0() | CR0_PG);

  // Lab1-4: init free memory at [KER_MEM, PHY_MEM), a heap for kernel
    // 初始化空闲页链表
  free_page_list = (page_t *)KER_MEM;
  for (size_t addr = KER_MEM; addr < PHY_MEM; addr += PGSIZE) {
    page_t *page = (page_t *)addr;
    page->next = (page_t *)(addr + PGSIZE);
  }
  page_t *last_page = (page_t *)(PHY_MEM - PGSIZE);
  last_page-> next = NULL;
}

void *kalloc() {
  // Lab1-4: alloc a page from kernel heap, abort when heap empty
  // 从空闲页链表中分配一页
  if (free_page_list == NULL) {
    // 空闲页链表为空，没有可分配的页
    assert(0); // 直接终止程序
  }

  page_t *page = free_page_list;
  free_page_list = page->next;
  return page;
}

void kfree(void *ptr) {
  // Lab1-4: free a page to kernel heap
  // you can just do nothing :)
  //TODO();
  page_t *page = (page_t *)ptr;
  page->next = free_page_list;
  free_page_list = page;
}

PD *vm_alloc() {
  // 使用 kalloc 分配一页作为页目录
  PD *pd = (PD *)kalloc();
  if (pd == NULL) {
    // 处理分配失败的情况
    assert(0); // 直接终止程序
  }

  // 将前32个 PDE 映射到 kpt 的前32个页表上
  for (int i = 0; i < 32; ++i) {
    pd->pde[i].val = MAKE_PDE(&kpt[i], 3);
  }

  // 将剩余的 PDE 清零
  for (int i = 32; i < NR_PTE; ++i) {
    pd->pde[i].val = 0;
  }

  return pd;
}

void vm_teardown(PD *pgdir) {
  // Lab1-4: free all pages mapping above PHY_MEM in pgdir, then free itself
  // you can just do nothing :)
  //TODO();
}

PD *vm_curr() {
  return (PD*)PAGE_DOWN(get_cr3());
}

PTE *vm_walkpte(PD *pgdir, size_t va, int prot) {
  // Lab1-4: return the pointer of PTE which match va
  // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
  // if not exist (PDE of va is empty) and !(prot&1), return NULL
  // remember to let pde's prot |= prot, but not pte
  assert((prot & ~7) == 0);
  PDE *pde = &pgdir->pde[ADDR2DIR(va)];
  if ((pde->val&1)==0) {
    // 对应的 PDE 不存在
    if (prot== 0) {
      return NULL;
    }
    // 分配一个新的页表
    PT *pt = (PT *) kalloc();
    if (pt == NULL) {
      // 处理分配失败的情况
      assert(0); // 直接终止程序
    }
  
    // 清零页表
    memset(pt,0 , PGSIZE);
    // 设置 PDE 的映射和权限
    pde->val = MAKE_PDE(pt, prot);
  } 
  PT* PT_va = PDE2PT(*pde);
  return &PT_va->pte[ADDR2TBL(va)];
}

void *vm_walk(PD *pgdir, size_t va, int prot) {
  // Lab1-4: translate va to pa
  // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
  // if va is not mapped and !(prot&1), return NULL
  PTE *pte = vm_walkpte(pgdir, va, prot);
  if (pte == NULL || !(pte->val & 1)) {
    return NULL;
  }
  return PTE2PG(*pte);
}

void vm_map(PD *pgdir, size_t va, size_t len, int prot) {
  // Lab1-4: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
  // if have already mapped pages, just let pte->prot |= prot
  assert(prot & PTE_P);
  assert((prot & ~7) == 0);
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  assert(start >= PHY_MEM);
  assert(end >= start);
  for (size_t cur_va = start; cur_va < end; cur_va += PGSIZE) {
    PTE *pte = vm_walkpte(pgdir, cur_va, prot);
    assert(pte != NULL);

    if (!(pte->val & PTE_P)) {
      // Page not mapped, allocate and map it
      void *page = kalloc();
      assert(page != NULL);
      memset(page, 0, PGSIZE);

      uintptr_t pa = (uintptr_t)page;
      pte->val = MAKE_PTE(pa, prot);
    }
  }
}

void vm_unmap(PD *pgdir, size_t va, size_t len) {
  // Lab1-4: unmap and free [va, va+len) at pgdir
  // you can just do nothing :)
  //assert(ADDR2OFF(va) == 0);
  //assert(ADDR2OFF(len) == 0);
  //TODO();
}

bool pteIsValid(PTE * pte){
  return pte->val &1;
}

void vm_copycurr(PD *pgdir) {
  // Lab2-2: copy memory mapped in curr pd to pgdir
  for(size_t i = PHY_MEM;i<USR_MEM;i+=PGSIZE){
    PD * pd = vm_curr();
    PTE *pte = vm_walkpte(pd,i,0);
    if(pte!= NULL&& pteIsValid(pte)){
      vm_map(pgdir,i,PGSIZE,7);
      memcpy(vm_walk(pgdir,i,0),vm_walk(pd,i,0),PGSIZE);
    }
  }
}



void vm_pgfault(size_t va, int errcode) {
  printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
  panic("pgfault");
}
