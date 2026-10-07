# 实验复测记录

复测日期：2026-10-07（Asia/Shanghai）。使用隔离副本，未修改本地原仓库或实验实现。环境：Ubuntu、RISC-V GCC 13.3.0、QEMU 8.2.2。以下是本地评分脚本结果，不是课程提交成绩。

为避免 QEMU 调试端口冲突，各测试使用独立 GDBPORT。首次发生端口冲突的结果未采纳。部分实验的扣分来自缺失 `time.txt` 或问答文件，需与功能失败分别看待。

## cow

原始分支提交：`b83010e2b1fca7fa4b58ab1827b19b629f9904b9`。命令：`make grade GDBPORT=26105`；退出码：`2`。

```text
  simple: OK
  three: OK
  file: OK
  forkfork: OK
  usertests: copyin: OK
  usertests: copyout: OK
  usertests: all tests: OK
time: FAIL
Score: 129/130
```

[完整评分输出](verification/cow.log)。

## fs

原始分支提交：`475a25808ad180f4b9572a78828faf2733fbd05c`。命令：`make grade GDBPORT=26107`；退出码：`2`。

```text
running bigfile: OK (130.5s)
  symlinktest: symlinks: OK
  symlinktest: concurrent symlinks: OK
usertests: OK (178.6s)
time: FAIL
Score: 99/100
```

[完整评分输出](verification/fs.log)。

## lock

原始分支提交：`b45552db6eaece0c2d836b7e6e9b0151c47bc5dd`。命令：`make grade GDBPORT=26106`；退出码：`2`。

```text
  kalloctest: test1: OK
  kalloctest: test2: OK
  kalloctest: test3: FAIL
kalloctest: sbrkmuch: OK (9.4s)
  bcachetest: test0: OK
  bcachetest: test1: OK
  bcachetest: test2: OK
  bcachetest: test3: OK
usertests: OK (72.5s)
time: FAIL
Score: 99/110
```

[完整评分输出](verification/lock.log)。

## mmap

原始分支提交：`ae001ba66efb1df3da0da4c84872ebeeefd1c784`。命令：`make grade GDBPORT=26108`；退出码：`2`。

```text
  mmaptest: mmap basic: OK
  mmaptest: mmap private: OK
  mmaptest: mmap read-only: OK
  mmaptest: mmap read/write: OK
  mmaptest: mmap dirty: OK
  mmaptest: not-mapped unmap: OK
  mmaptest: lazy access: OK
  mmaptest: two files: OK
  mmaptest: fork_test: OK
  mmaptest: munmap_noaccess: OK
  mmaptest: read_only_write: OK
usertests: OK (74.3s)
time: FAIL
Score: 169/170
```

[完整评分输出](verification/mmap.log)。

`mmap` 现有测试全部通过，但原报告指出 `exec` 时缺少 MAP_SHARED 写回和 VMA 清理；通过现有评分脚本不代表该待办已修复。

## 锁竞争指标单独复核

其他 QEMU 测试结束后，单独执行 `MAKEFLAGS=GDBPORT=26130 python3 grade-lab-lock kalloctest`，退出码 `1`。`kalloctest test3` 仍失败；此处保留结果，不将提交信息中的 completed 等同于全部评分通过。

```text
  kalloctest: test1: OK
  kalloctest: test2: OK
  kalloctest: test3: FAIL
== Test kalloctest: sbrkmuch == kalloctest: sbrkmuch: OK (10.2s)
```

[单独复核日志](verification/lock-kalloctest-standalone.log)。
