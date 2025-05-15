### Big Files

原 readi() 代码，由于读之前判定了当前 off 有没有超过 size，所以肯定不会创建新 block，进而不需要对文件系统进行任何更新，也就不需要 log_write()，因此没有把 readi() 套在 begin_op() 和 end_op() 里面

我的代码在 bmap() 中不论有没有修改 indirect block，都对其进行 log_write()，所以会报错 "log_write outside of trans"

### Symbolic Links

做了一个小试验

```c
int
main(int argc, char *argv[])
{
  int fd = open("test.txt", O_CREATE | O_WRONLY);
  write(fd, "hello", 5);
  close(fd);
  fd = open("test.txt", O_RDONLY);
  char buf1[100], buf2[100];
  read(fd, buf1, 6);
  close(fd);
  fd = open("test.txt", O_WRONLY);
  write(fd, "world", 5);
  close(fd);
  fd = open("test.txt", O_RDONLY);
  read(fd, buf2, 12);
  printf("%s\n%s\n%s\n", buf1, buf2, buf2+6);
}
```

很明显因为 open 在打开文件时钦定 f->off = 0，所以不论读写都从头开始，要支持 append 需要另加代码