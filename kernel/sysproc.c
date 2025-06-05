#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


uint64 sys_sigalarm(void) {
  // printf("sys_sigalarm: 测试");
  struct proc *p = myproc();

  // 获取第一个参数，间隔时间
  int interval;
  argint(0, &interval);
  p -> interval = interval;

  // 获取第二个参数，handler地址
  uint64 handler_address;
  argaddr(1, &handler_address);  
  p -> handler_address = handler_address;

  p -> ticks = 0; // 每次调用ticks应该从0开始

  return 0;
}

uint64 sys_sigreturn(void) {
  // printf("sys_sigreturn: 测试");
  struct proc *p = myproc();
  p -> is_handler_running = 0; // 停止运行handler

  // 恢复寄存器状态
  memmove(p->trapframe, &p->sig_trapframe, sizeof(struct trapframe)); 
  return p -> trapframe -> a0; 
}