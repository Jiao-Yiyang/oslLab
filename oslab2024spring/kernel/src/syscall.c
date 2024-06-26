#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx) {
  // TODO: Lab1-5 call specific syscall handle and set ctx register
  int sysnum = ctx->eax;
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS) {
    res = -1;
  } else {
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

int sys_write(int fd, const void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
   proc_t * curr = proc_curr();
  file_t* file = proc_getfile(curr, fd);
    if (file == NULL) {
      return -1;
    }
   ;
    return fwrite(file, buf, count);
}

int sys_read(int fd, void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  proc_t * curr = proc_curr();
  file_t* file = proc_getfile(curr, fd);
    if (file == NULL) {
      return -1;
    }
   ;
    return fread(file, buf, count);
}

int sys_brk(void *addr) {
  // TODO: Lab1-5
  proc_t * cur_pcb = proc_curr();
  //static size_t brk = 0; // use brk of proc instead of this in Lab2-1
  size_t new_brk = PAGE_UP(addr);
  if (cur_pcb->brk == 0) {
    cur_pcb->brk = new_brk;
  } else if (new_brk > cur_pcb->brk) {
    //TODO();
    vm_map(vm_curr(), cur_pcb->brk, new_brk - cur_pcb->brk, 7);
    cur_pcb->brk = new_brk;
  } else if (new_brk < cur_pcb->brk) {
    // can just do nothing
  }
  return 0;
}

void sys_sleep(int ticks) {
  // Lab1-7
  size_t start_tick = get_tick();
  while(start_tick + ticks > get_tick()){
    //sti();
    //hlt();
    //cli();
    proc_yield();
  }
}

int sys_exec(const char *path, char *const argv[]) {
  PD *new_pdir = vm_alloc();
  if (new_pdir == NULL) {
    return -1;
  }
  Context new_context;
  // 将用户程序加载到新的页目录中，并初始化上下文
  int load_result = load_user(new_pdir, &new_context, path, argv);
  if (load_result != 0) {
    return -1;
  }

  PD *old_pdir = vm_curr();
  proc_t * cur_pcb = proc_curr();
  cur_pcb->pgdir = new_pdir;
  // 切换到新的页目录
  set_cr3(new_pdir);
  vm_teardown(old_pdir);
  // 通过从中断返回进入新的用户程序
  irq_iret(&new_context);
  return 0;
}

int sys_getpid() {
  proc_t * cur_pcb = proc_curr();
 return cur_pcb->pid; // Lab2-1
}

void sys_yield() {
  proc_yield();
}

int sys_fork() {
  proc_t* pcb_ = proc_alloc();
  if(pcb_ == NULL){
    return -1;
  }
  proc_copycurr(pcb_);
  proc_addready(pcb_);
  return pcb_->pid;
}

void sys_exit(int status) {
  proc_makezombie(proc_curr(),status);
  INT(0x81);
  //TODO(); // Lab2-3
}

int sys_wait(int *status) {
  proc_t* cur_proc = proc_curr();
  if(cur_proc->child_num==0){
    return -1;
  }
  
  sem_p(&cur_proc->zombie_sem);
  while(!proc_findzombie(cur_proc)){
    proc_yield();
  }
  proc_t* zombie_child = proc_findzombie(cur_proc);
  cur_proc->child_num--;
  if(status){
    *status = zombie_child->exit_code;
  }
  int pid = zombie_child->pid;
  proc_free(zombie_child);

  return pid;
  //TODO(); // Lab2-3, Lab2-4
}

int sys_sem_open(int value) {
  proc_t * curr = proc_curr();
 int sem_id = proc_allocusem(curr);
    if (sem_id == -1) {
      return -1;
    }
    usem_t *usem = usem_alloc(value);
    if (usem == NULL) {
      return -1;
    }
    curr->usems[sem_id] = usem;
    return sem_id;
}

int sys_sem_p(int sem_id) {
   proc_t * curr = proc_curr();
    usem_t *usem = proc_getusem(curr, sem_id);
    if (usem == NULL) {
        return -1;
    }
    sem_p(&usem->sem);
    return 0;
}

int sys_sem_v(int sem_id) {
  proc_t * curr = proc_curr();
  usem_t *usem = proc_getusem(curr, sem_id);
    if (usem == NULL) {
      return -1;
    }
    sem_v(&usem->sem);
    return 0;
}

int sys_sem_close(int sem_id) {
  proc_t * curr = proc_curr();
usem_t *usem = proc_getusem(curr, sem_id);
    if (usem == NULL) {
      return -1;
    }
    usem_close(usem);
    curr->usems[sem_id] = NULL;
    return 0;
}

int sys_open(const char *path, int mode) {
   int fd = proc_allocfile(proc_curr());
    if (fd == -1) {
      return -1;
    }
    file_t *file = fopen(path, mode);
    if (file == NULL) {
      return -1;
    }
    proc_curr()->files[fd] = file;
    return fd;
}

int sys_close(int fd) {
  proc_t * curr = proc_curr();
  file_t* file = proc_getfile(curr, fd);
    if (file == NULL) {
      return -1;
    }
    fclose(file);
    curr->files[fd] = NULL;
    return 0;
}

int sys_dup(int fd) {
  proc_t * curr = proc_curr();
  file_t *file = proc_getfile(curr, fd);
  if(file == NULL){
    return -1;
  }
  
  int new_fd = proc_allocfile(curr);
  if(new_fd == -1){
    return -1;
  }
  
  curr->files[new_fd] = fdup(file);
  
  return new_fd;
}
uint32_t sys_lseek(int fd, uint32_t off, int whence) {
  proc_t * curr = proc_curr();
  file_t* file = proc_getfile(curr, fd);
  if(file == NULL){
    return -1;
  
  }
  return fseek(file, off, whence);
}

int sys_fstat(int fd, struct stat *st) {
  proc_t * curr = proc_curr();
  file_t* file = proc_getfile(curr, fd);
    if (file == NULL) {
      return -1;
    }
    if(file->type == TYPE_FILE||file->type == TYPE_DIR){
      st->type = itype(file->inode);
    st->size =  isize(file->inode);
    st->node =  ino(file->inode);
  } else if(file->type == TYPE_DEV){
    st->type = TYPE_DEV;
      st->size = 0;
      st->node = 0;
  }
    return 0;
}

int sys_chdir(const char *path) {
  proc_t * curr = proc_curr();
  inode_t *inode = iopen(path, TYPE_NONE);
  if (inode == NULL) {
    return -1;
  }
  if (itype(inode) != TYPE_DIR) {
    iclose(inode);
    return -1;
  }
  iclose(curr->cwd);
  curr->cwd = inode;
  return 0;
}

int sys_unlink(const char *path) {
  return iremove(path);
}

// optional syscall

void *sys_mmap() {
  TODO();
}

void sys_munmap(void *addr) {
  TODO();
}

int sys_clone(void (*entry)(void*), void *stack, void *arg) {
  TODO();
}

int sys_kill(int pid) {
  TODO();
}

int sys_cv_open() {
  TODO();
}

int sys_cv_wait(int cv_id, int sem_id) {
  TODO();
}

int sys_cv_sig(int cv_id) {
  TODO();
}

int sys_cv_sigall(int cv_id) {
  TODO();
}

int sys_cv_close(int cv_id) {
  TODO();
}

int sys_pipe(int fd[2]) {
  TODO();
}

int sys_link(const char *oldpath, const char *newpath) {
  TODO();
}

int sys_symlink(const char *oldpath, const char *newpath) {
  TODO();
}

void *syscall_handle[NR_SYS] = {
  [SYS_write] = sys_write,
  [SYS_read] = sys_read,
  [SYS_brk] = sys_brk,
  [SYS_sleep] = sys_sleep,
  [SYS_exec] = sys_exec,
  [SYS_getpid] = sys_getpid,
  [SYS_yield] = sys_yield,
  [SYS_fork] = sys_fork,
  [SYS_exit] = sys_exit,
  [SYS_wait] = sys_wait,
  [SYS_sem_open] = sys_sem_open,
  [SYS_sem_p] = sys_sem_p,
  [SYS_sem_v] = sys_sem_v,
  [SYS_sem_close] = sys_sem_close,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_dup] = sys_dup,
  [SYS_lseek] = sys_lseek,
  [SYS_fstat] = sys_fstat,
  [SYS_chdir] = sys_chdir,
  [SYS_unlink] = sys_unlink,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_clone] = sys_clone,
  [SYS_kill] = sys_kill,
  [SYS_cv_open] = sys_cv_open,
  [SYS_cv_wait] = sys_cv_wait,
  [SYS_cv_sig] = sys_cv_sig,
  [SYS_cv_sigall] = sys_cv_sigall,
  [SYS_cv_close] = sys_cv_close,
  [SYS_pipe] = sys_pipe,
  [SYS_link] = sys_link,
  [SYS_symlink] = sys_symlink};
