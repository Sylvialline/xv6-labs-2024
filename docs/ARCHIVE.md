# 完整历史备份

[Release](https://github.com/Sylvialline/xv6-labs-2024/releases/tag/archive-2026-10-07) 中的 [`xv6-labs-2024-original-2026-10-07.tar.gz`](https://github.com/Sylvialline/xv6-labs-2024/releases/download/archive-2026-10-07/xv6-labs-2024-original-2026-10-07.tar.gz) 保留原始 Git 历史和本地未提交内容。

原始远程来源：`git://g.csail.mit.edu/xv6-labs-2024`。

SHA-256：`fbccf51bcd0719cdd792738884d73f453d2296b9ae5ffd7aeb5cb648cc8a9aeb`。已核对 GitHub 返回的附件摘要。

附件包含：

- `original.bundle`：所有原始分支、远程分支、标签及 stash 可达的 Git 对象；额外保存 reflog 残留提交。
- `inventory.json`：留档时的 HEAD、分支、远程来源、状态和原始 ref / SHA 对照。
- `staged.patch`、`unstaged.patch`：已暂存和未暂存的修改。
- `untracked.tar.gz`：未跟踪文件。

被 `.gitignore` 忽略且未纳入 Git 的编译产物未上传；已提交的生成文件完整保留在 bundle 中。

## 恢复

在新目录中解压附件后，可以完整恢复原始 refs：

```bash
git clone --mirror original.bundle restored.git
git clone restored.git restored
cd restored
git switch mmap
```

如需恢复当时未提交的内容，在 `restored` 目录按顺序应用非空补丁，并解压未跟踪文件（附件解压于上一级目录）：

```bash
if test -s ../staged.patch; then git apply --index ../staged.patch; fi
if test -s ../unstaged.patch; then git apply ../unstaged.patch; fi
tar -xzf ../untracked.tar.gz
```

原始 stash 位于镜像的 `refs/stash`；`archive/stash-0` 分支用于浏览同一提交。stash 的第二个父提交为 index 快照；存在第三个父提交时，其保存未跟踪文件。可以从 bundle 恢复这些原始对象。

## 原始引用与公开分支

| 原始引用 | 原始 SHA | 公开分支 / 标签 |
| --- | --- | --- |
| `refs/heads/cow` | `b83010e2b1fca7fa4b58ab1827b19b629f9904b9` | `cow` |
| `refs/heads/fs` | `475a25808ad180f4b9572a78828faf2733fbd05c` | `fs` |
| `refs/heads/lock` | `b45552db6eaece0c2d836b7e6e9b0151c47bc5dd` | `lock` |
| `refs/heads/mmap` | `ae001ba66efb1df3da0da4c84872ebeeefd1c784` | `mmap` |
| `refs/heads/util` | `79e8024d61c3dd92ccf064fc67bb99cb999f70ab` | `util` |
| `refs/remotes/origin/HEAD` | `79e8024d61c3dd92ccf064fc67bb99cb999f70ab` | `上游 HEAD 指向，详见 inventory.json` |
| `refs/remotes/origin/cow` | `19fa72d7dccf193c7e09789db708443736c2954c` | `upstream/cow` |
| `refs/remotes/origin/fs` | `408565dc36c7595de3e68284a3789c081901b88f` | `upstream/fs` |
| `refs/remotes/origin/lock` | `fc27289d78a415ed4e4264a80b88c8496db70eef` | `upstream/lock` |
| `refs/remotes/origin/mmap` | `6dd1443e74895a3953d837106392a9ad538fbab2` | `upstream/mmap` |
| `refs/remotes/origin/net` | `0b94a6a45fc79ad87112d16622bb5f2f130c893b` | `upstream/net` |
| `refs/remotes/origin/pgtbl` | `db30f8d9f46732d926995fb97a2ea0362f0654ee` | `upstream/pgtbl` |
| `refs/remotes/origin/riscv` | `de247db5e6384b138f270e0a7c745989b5a9c23b` | `upstream/riscv` |
| `refs/remotes/origin/syscall` | `d5b09ff4571e07f6d996fdf7ce1533afc19a6978` | `upstream/syscall` |
| `refs/remotes/origin/traps` | `814d126edf03b5498dbc7e0796190b938837a851` | `upstream/traps` |
| `refs/remotes/origin/util` | `79e8024d61c3dd92ccf064fc67bb99cb999f70ab` | `upstream/util` |

仅在 reflog 中残留的原始提交：`6d05aa65df65295b80affb889f0f13bf96fe0c0e`。

公开历史仅移除 `fs.img.bk`；其他文件的内容已逐分支对照原始树验证一致。[commit-map.txt](commit-map.txt) 给出原始 SHA 到公开 SHA 的映射。本地原仓库没有改写。
