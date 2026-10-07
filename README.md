# xv6-labs-2024

我的 MIT 6.1810 2024 年实验代码，个人提交集中在 2025 年 3–5 月。为了组合不同年份的实验，我同时使用了 2021、2023 和 2024 年的仓库。

## 分支与进度

| 分支 | 内容与状态 |
| --- | --- |
| `cow` | 2025-03-23：copy-on-write；cowtest 和 usertests 通过 |
| `lock` | 2025-04-17–19：per-CPU allocator、buffer cache；保留锁竞争分析报告，复测详情见验证记录 |
| `fs` | 2025-04-28–05-19：large files、symbolic links；bigfile、symlinktest 和 usertests 通过；保留日志事务分析报告 |
| `mmap` | 2025-05-19–31：mmap / munmap、缺页处理；mmaptest 和 usertests 通过，但报告中的 exec 写回及 VMA 清理问题尚未修复 |
| `util` | 官方初始代码，没有本年份的个人实验实现 |

[对应年份课程网站](https://pdos.csail.mit.edu/6.1810/2024/)。

## 复测摘要（2026-10-07）

| 分支 | 评分 | 说明 |
| --- | --- | --- |
| `cow` | 129/130 | 功能和 usertests 通过，缺少 time.txt |
| `lock` | 99/110 | bcachetest / usertests 通过；kalloctest test3 竞争指标单独复测仍未达标，缺少 time.txt |
| `fs` | 99/100 | 功能和 usertests 通过，缺少 time.txt |
| `mmap` | 169/170 | 现有功能和 usertests 通过，缺少 time.txt；exec 待办仍保留 |

## 查看代码

默认分支 `codex/archive` 放置留档说明和当时的工作区快照。切换到实验分支查看各实验实现：

```bash
git clone https://github.com/Sylvialline/xv6-labs-2024.git
cd xv6-labs-2024
git switch mmap
```

实验依赖 RISC-V 工具链、QEMU、make 和 Python 3。实验分支通常使用 `make qemu` 运行、`make grade` 评分；切换不同实验后先执行 `make clean`。参考仓库和 xv6 基础仓库不保证包含每个实验的评分脚本。

## 相关留档

| 仓库 | 用途 |
| --- | --- |
| [xv6-labs-2021](https://github.com/Sylvialline/xv6-labs-2021) | Unix utilities |
| [xv6-labs-2023](https://github.com/Sylvialline/xv6-labs-2023) | syscall、pgtbl、traps、thread |
| [xv6-labs-2024](https://github.com/Sylvialline/xv6-labs-2024) | cow、lock、fs、mmap |
| [xv6-labs-2020](https://github.com/Sylvialline/xv6-labs-2020) | 2020 starter 和编译适配 |
| [xv6-labs-2024-tmp](https://github.com/Sylvialline/xv6-labs-2024-tmp) | 2024 临时副本和 mmap stash |
| [xv6-riscv](https://github.com/Sylvialline/xv6-riscv) | xv6 基础源码和调试配置 |
| [MIT6.S081-2020fall-reference](https://github.com/Sylvialline/MIT6.S081-2020fall-reference) | PKUFlyingPig 的参考解答 |

目前没有个人实现的 lazy allocation 或 network 实验提交；仓库中的官方起始分支及参考解答不计入个人实验完成进度。

## 留档说明

2026-10-07 整理自本地 `~/xv6/xv6-labs-2024`。原始提交的作者和日期保留；`upstream/*` 是当时本地保存的上游分支快照，`archive/reflog-*` 保存仅在 reflog 中残留的版本。原有 `README` 和许可证保留。

为满足 GitHub 文件大小限制，公开 Git 历史移除了生成文件 `fs.img.bk`，相关提交 SHA 因此变化。包含该文件及原始 SHA 的完整 Git 历史保存在 Release 备份附件中。

[完整备份和恢复说明](docs/ARCHIVE.md) · [2026-10-07 复测记录](docs/VERIFICATION.md)。本次留档未修复实验实现。
