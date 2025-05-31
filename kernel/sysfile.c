//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  argint(n, &fd);
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;
  
  argaddr(1, &p);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  argaddr(1, &st);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    // now that success is guaranteed:
    dp->nlink++;  // for ".."
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  // something went wrong. de-allocate ip.
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  argint(1, &omode);
  if((n = argstr(0, path, MAXPATH)) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  argint(1, &major);
  argint(2, &minor);
  if((argstr(0, path, MAXPATH)) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  argaddr(1, &uargv);
  if(argstr(0, path, MAXPATH) < 0) {
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  argaddr(0, &fdarray);
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

// alloc a vma slot. return 0 for up to limit.
static struct vma*
allocvma()
{
  struct vma *v, *vmas = myproc()->vmas;
  for(v = vmas; v < &vmas[MAX_VMA]; v++) {
    if(v->start == 0)
      return v;
  }
  return 0;
}

// return MAP_FAILED on error
uint64
sys_mmap(void)
{
  struct proc *p = myproc();
  struct vma *v;

  size_t len;

  if((v = allocvma()) == 0)
    return MAP_FAILED;
    
  if(argfd(4, 0, &v->f) < 0){
    return MAP_FAILED;
  }
  if(!v->f->readable){ 
    // fd with O_WRONLY does not support mmap
    return MAP_FAILED;
  }
  argint(2, &v->prot);
  argint(3, &v->flags);
  if(v->prot & PROT_WRITE && !v->f->writable && v->flags != MAP_PRIVATE) {
    // cannot write
    return MAP_FAILED;
  }
  v->f = filedup(v->f);

  argulong(1, &len);
  v->start = PGROUNDUP(p->sz); // fill start, install vma
  v->end = v->start + len;
  p->sz = PGROUNDUP(v->end);
  
  arglong(5, &v->off);

  return v->start;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  size_t len;
  argulong(0, &addr);
  argulong(1, &len);
  return munmap((void*)addr, len);
}


// mmap page fault handler
// if the page fault is not caused by mmap, return 0
// else return 1
int
pagefault()
{
  uint64 scause = r_scause();
  uint64 addr = r_stval();
  struct vma *v;
  struct inode *ip;
  int load = scause == 0xdL;  // Load page fault
  int store = scause == 0xfL; // Store/AMO page fault
  char *mem;
  int perm;

  if(!load && !store)  
    return 0;
  
  if((v = whichvma(addr)) == 0)
    return 0; // addr does not belong to any vma
  
  if(store && !(v->prot & PROT_WRITE))
    return 0; // permission mismatch
  
  if((mem = (char*)kalloc()) == 0)
    return 0;
  memset(mem, 0, PGSIZE);
  
  addr = PGROUNDDOWN(addr);
  ip = v->f->ip;
  ilock(ip);
  if(readi(ip, 0, (uint64)mem, addr - v->start + v->off, PGSIZE) < 0){
    kfree(mem);
    iunlock(ip);
    return 0;
  }
  iunlock(ip);

  perm = PTE_U | PTE_R;
  if(v->prot & PROT_WRITE) perm |= PTE_W;
  
  mappages(myproc()->pagetable, addr, PGSIZE, (uint64)mem, perm);
  return 1;
}


// which vma is addr in?
// return 0 for not a vma address
struct vma *
whichvma(uint64 addr)
{
  struct vma *v, *vmas = myproc()->vmas;
  for(v = vmas; v < &vmas[MAX_VMA]; v++)
    if(v->start && addr >= v->start && addr < v->end)
      return v;
  return 0;
}

// close the file
// do not remove the mappings
void
clearvma(struct vma *v)
{
  if(v->start) {
    if(v->f)
      fileclose(v->f);
    v->f = 0;
    v->start = 0;
  }
}

#define max(a,b) (a) > (b) ? (a) : (b)
#define min(a,b) (a) < (b) ? (a) : (b)

int
munmap(void *addr, size_t len) {

  uint64 s, t, a;
  struct vma *v;
  pte_t *pte;
  pagetable_t pgtbl;
  struct inode *ip;
  s = (uint64)addr;
  if((v = whichvma(s)) == 0)
    return MAP_FAILED;
  t = s + len;

  if(s > v->start && t < v->end)
    return MAP_FAILED; // punched a hole
  
  if(s < v->start || t > v->end)
    return MAP_FAILED; // unmap something not mapped
  
  pgtbl = myproc()->pagetable;
  ip = v->f->ip;
  
  for(a = PGROUNDDOWN(s); a < t; a += PGSIZE) {
    if((pte = walk(pgtbl, a, 0)) == 0 || (*pte & PTE_V) == 0){
      // lazy allocation. ok.
      continue;
    }
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("munmap: not a leaf");

    uint64 l = max(a, s), r = min(a+PGSIZE, t);
    uint64 pa = PTE2PA(*pte);

    if(v->flags == MAP_SHARED && v->prot & PROT_WRITE) { // write back
      uint off = l - v->start + v->off;
      // modifications on mappings cannot increase the size of the underlying file
      int size = min(r - l, ip->size - off);
      // printf("# write %lu %lu %lu %lu %lu\n", v->start, v->end, a, l, r);
      if(size > 0) {
        begin_op();
        ilock(ip);
        int n = writei(ip, 0, pa + l - a, off, (uint)size);
        iunlock(ip);
        end_op();
        if(n < size)
          return MAP_FAILED;
      }
      
    }
    if((l==a || l==v->start) && (r==a+PGSIZE || r==v->end)) {
      // remove the mapping
      kfree((void*)pa);
      *pte = 0;
    }
  }

  if(s == v->start && t == v->end)
    clearvma(v);
  else if(s == v->start) {
    v->start += len;
    v->off += len;
  }
  else v->end -= len;
  return 0;
}
