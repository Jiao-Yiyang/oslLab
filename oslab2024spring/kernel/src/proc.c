#include "klib.h"
#include "cte.h"
#include "proc.h"

#define PROC_NUM 64

static __attribute__((used)) int next_pid = 1;

proc_t pcb[PROC_NUM];
static proc_t *curr = &pcb[0];

void init_proc() {
  pcb[0].status = RUNNING;
  pcb[0].pgdir = vm_curr();
  // Lab2-1, set status and pgdir
  // Lab2-4, init zombie_sem
  sem_init(&pcb[0].zombie_sem, 0);
  curr->cwd = iopen("/", TYPE_NONE);
  // Lab3-2, set cwd
}

proc_t *proc_alloc() {
  // Lab2-1: find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
  proc_t * to_alloc = NULL;
  for(int i=1;i<PROC_NUM;i++){
    if(pcb[i].status==UNUSED){
      to_alloc = &pcb[i];
      break;
    }
  }
  if(!to_alloc){
    return to_alloc;
  }
  to_alloc->pid = next_pid++;
  to_alloc->status = UNINIT;
  PD* pgdir = vm_alloc();
  to_alloc-> pgdir = pgdir;
  to_alloc->brk = 0;
  
  to_alloc->kstack = (kstack_t *)kalloc();
  to_alloc->ctx = &(to_alloc->kstack->ctx);

  to_alloc->cwd = NULL;
  to_alloc->parent = NULL;
  to_alloc->child_num = 0;
  sem_init(&to_alloc->zombie_sem,0);
  for (int i = 0; i <MAX_USEM; i++) {
      to_alloc->usems[i] = NULL;
    }
  for(int i = 0; i < MAX_UFILE; i++){
    to_alloc->files[i] = NULL;
  }
  return to_alloc;
  // init ALL attributes of the pcb
}

void proc_free(proc_t *proc) {
  // Lab2-1: free proc's pgdir and kstack and mark it UNUSED
  if(proc->status != RUNNING){
    //vm_teardown(proc->pgdir);
    //kfree(proc->kstack);
    proc->status = UNUSED;
  }
}

proc_t *proc_curr() {
  return curr;
}

void proc_run(proc_t *proc) {
  proc->status = RUNNING;
  curr = proc;
  set_cr3(proc->pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
  irq_iret(proc->ctx);
}

void proc_addready(proc_t *proc) {
  // Lab2-1: mark proc READY
  proc->status = READY;
}

void proc_yield() {
  // Lab2-1: mark curr proc READY, then int $0x81
  curr->status = READY;
  INT(0x81);
}

void proc_copycurr(proc_t *proc) {
  // Lab2-2: copy curr proc
  // Lab2-5: dup opened usems
  // Lab3-1: dup opened files
  // Lab3-2: dup cwd
  vm_copycurr(proc->pgdir);
  
  memcpy(&(proc->kstack->ctx), &(curr->kstack->ctx), sizeof(Context));
  proc -> ctx = &(proc->kstack->ctx);
  proc -> ctx->eax = 0;
  proc -> brk = curr->brk;
  curr -> child_num++;
  proc -> parent = curr;
  proc->cwd = idup(curr->cwd);
  for (int i = 0; i < MAX_USEM; i++) {
        if (curr->usems[i] != NULL) {
            proc->usems[i] = curr->usems[i];
            usem_dup(proc->usems[i]);
        }
  }
  for(int i = 0; i < MAX_UFILE; i++){
    if(curr->files[i] != NULL){
      proc->files[i] = curr->files[i];
      fdup(proc->files[i]);
    }
}
}
void proc_makezombie(proc_t *proc, int exitcode) {
  // Lab2-3: mark proc ZOMBIE and record exitcode, set children's parent to NULL
  // Lab2-5: close opened usem
  // Lab3-1: close opened files
  // Lab3-2: close cwd
  proc-> status = ZOMBIE;
  proc-> exit_code = exitcode;
  for(int i= 0;i<PROC_NUM;i++){
    if(pcb[i].parent == proc){
      pcb[i].parent =NULL;
    }
  }
  if (proc->parent != NULL) {
    sem_v(&proc->parent->zombie_sem);
  }
  for (int i = 0; i < MAX_USEM; i++) {
        if (proc->usems[i] != NULL) {
            usem_close(proc->usems[i]);
            proc->usems[i] = NULL;
        }
  }
  for(int i = 0; i < MAX_UFILE; i++){
    if(proc->files[i] != NULL){
      fclose(proc->files[i]);
      proc->files[i] = NULL;
    }
  }
  iclose(proc->cwd);
  proc->cwd = NULL;
  
}

proc_t *proc_findzombie(proc_t *proc) {
  // Lab2-3: find a ZOMBIE whose parent is proc, return NULL if none
  proc_t *zombie = NULL;
  for(int i= 0;i<PROC_NUM;i++){
    if(pcb[i].parent == proc && pcb[i].status == ZOMBIE){
      zombie = &pcb[i];
      break;
    }
  }
  return zombie;
}

void proc_block() {
  // Lab2-4: mark curr proc BLOCKED, then int $0x81
  curr->status = BLOCKED;
  INT(0x81);
}

int proc_allocusem(proc_t *proc) {
  // Lab2-5: find a free slot in proc->usems, return its index, or -1 if none
  for (int i = 0; i < 32; i++) {
    if (proc->usems[i] == NULL) {
      return i;
    }
  }
  return -1;
}

usem_t *proc_getusem(proc_t *proc, int sem_id) {
  // Lab2-5: return proc->usems[sem_id], or NULL if sem_id out of bound
  if (sem_id < 0 || sem_id >= 32) { 
    return NULL;
  }
    return proc->usems[sem_id];
}

int proc_allocfile(proc_t *proc) {
  // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
  for(int i = 0; i < 32; i++){
    if(proc->files[i] == NULL){
      return i;
    }
  }
  return -1;
}

file_t *proc_getfile(proc_t *proc, int fd) {
  // Lab3-1: return proc->files[fd], or NULL if fd out of bound
  return (fd < 0 || fd >= 32) ? NULL : proc->files[fd];
}

void schedule(Context *ctx) {
  // Lab2-1: save ctx to curr->ctx, then find a READY proc and run it
  curr->ctx = ctx;
  int next = (curr - pcb + 1) % PROC_NUM;
  for (;; next = (next + 1) % PROC_NUM) {
    if (pcb[next].status == READY) {
      proc_run(&pcb[next]);
      break;
    }
  }
}
