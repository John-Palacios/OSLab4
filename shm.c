#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id;
    char *frame;
    int refcnt;
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

int shm_open(int id, char **pointer) {
  int i;
  void * r; 
  int fst = 0;
  struct proc *curproc;
  curproc = myproc();

  // account for concurancy
  acquire(&(shm_table.lock));
  
  // Searching for shared page with given id
  for(i = 0; i < 64; i++){
    if(shm_table.shm_pages[i].id != 0){fst++;}
    if(shm_table.shm_pages[i].id == id){
      shm_table.shm_pages[i].refcnt++;
      // map process VA to physical page address
      if(mappages(curproc->pgdir, (void *)PGROUNDUP(curproc->sz), PGSIZE, V2P(shm_table.shm_pages[i].frame), PTE_W|PTE_U) < 0){
        panic("shm_open");
      }
      // return this va to the calling process
      *pointer = (char *)curproc->sz;
      curproc->sz += PGSIZE;
      release(&(shm_table.lock));
      return 0;
    }
  }
  
  // id not found
  if(fst > 63){panic("shm_open::table overflow");}
  shm_table.shm_pages[fst].id = id;
  // Allocate physical memory for shared page
  if( (r = kalloc()) == 0 ){ panic("Shm_open::allocation error");}
  memset(r, 0, PGSIZE);
  shm_table.shm_pages[fst].refcnt++;
  shm_table.shm_pages[fst].frame = (void *)r;
  if(mappages(curproc->pgdir, (void *)PGROUNDUP(curproc->sz), PGSIZE, V2P(shm_table.shm_pages[fst].frame), PTE_W|PTE_U) < 0){
    panic("shm_open");
  }
  // return virtual address to the calling process.
  *pointer = (char *)curproc->sz;
  curproc->sz += PGSIZE;
  release(&(shm_table.lock));
  return 0;
}


int shm_close(int id) {
int i;

acquire(&(shm_table.lock));
for(i = 0; i < 64; i++){
    if(shm_table.shm_pages[i].id == id){
      shm_table.shm_pages[i].refcnt--;
      if(shm_table.shm_pages[i].refcnt == 0){
        // clear shared memory page.
        shm_table.shm_pages[i].id = 0;
        shm_table.shm_pages[i].frame = 0;
      }
    }
}
release(&(shm_table.lock));

return 0;
}
