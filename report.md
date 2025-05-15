原 readi() 代码，由于读之前判定了当前 off 有没有超过 size，所以肯定不会创建新 block，进而不需要对文件系统进行任何更新，也就不需要 log_write()，因此没有把 readi() 套在 begin_op() 和 end_op() 里面

我的代码在 bmap() 中不论有没有修改 indirect block，都对其进行 log_write()，所以会报错 "log_write outside of trans"