#### bcache 遇到的问题
bcache.lock 用于保护寻找 unused cache(即 refcnt == 0 的buf) 的过程
list.lock 用于保护每一个链表的结构

##### 一个会造成死锁的路径
- bget 需要找特定 blockno 的 buf，在对应 list 里找。因此要先 aquire 对应 list.lock
- 没找到，为了避免对一个block重复分配buf的情况，必须让找不到buf和分配未用buf的过程atomic
- 所以在不释放list.lock的情况下继续aqcuire bcache.lock，遍历所有buf找到refcnt==0的某个并evict
- 对每一个遍历的buf，为了避免在发现refcnt==0后又被该buf对应的list给refcnt++，并维护删除buf时链表结构正确性，也必须要acquire这个buf对应的list.lock，这就死锁了
- list1.lock -> bcache.lock -> list2.lock

##### 解决方案
- 明确 bcache.lock 的意义: serializing cache miss handling, 将 critical section 设置为“将某个 refcnt 为 0 的 buf 从原本的 list 中删去并加入新 list”的过程
- 这样，如果在 bget 中要求的 blockno 没有在对应的 list 中找到，那么 bget 可以 safely drop the lock，然后 aqcuire bache.lock，准备 handle cache miss。这样做可以避免死锁。
- 如果同时有多个 bget 没找到 blockno，第一个 aqcuire 到 bcache.lock 的线程会完成 evicting，之后的线程会在进入 bcache.lock 的 critical section 后发现对应 list 里面已经有 blockno 了，可以直接 return。

##### cache miss handler
- 为了完成 eviction, handler 需要能访问所有 buf 的 refcnt 和 blockno, 以及要能修改所有链表的结构。
- 为了避免 handler 找到 refcnt==0 的 buf 后又被 list 修改(refcnt++), 对于确定好的 sacrifice buffer 必须一直持有其对应 list 的锁直到 list 删去这个 buf
- 一种具体实现是
  + 从 buf 数组枚举所有 buf
  + 获取其 blockno 后 acquire 对应的锁
    - 不先知道 blockno 的内容的话无法知道要 acquire 哪个锁。但为什么可以这么做？
    - 一个简单的解释是，我们先获得 A 锁，再获得了 B 锁，然后才修改一个变量 c
    - 即 c 只在 A 和 B 共同的 c.s. 内被写
    - 所以只要我们获取了 A 或者 B 锁中的任何一个，就可以安全地读 c 的内容，而不发生任何 race
    - 对应到具体情况来说就是，我们在先后获得了 bcache.lock 和 list.lock 之后进行驱逐，只有在驱逐的时候我们会改 blockno
    - 我们在 bget 开头 check cache miss 时只获取了 list.lock, 这个时候读 blockno 是安全的
    - 然后我们在 handle cache miss 时获取了 bcache.lock, 这个时候读 blockno 也是安全的
  + 判断是否可作为 scrifice，是的话执行 evicting, 放掉 list.lock
  + 获取 bget 想要的 blockno 对应的锁，将刚获取到的 buf 加入
  + 释放所有锁
- 另一种实现是
  + 直接枚举所有的 bucket list, 遍历链表来获得所有非空闲的 cache
  + 这样做 acquire 的次数会更少，但是每次 acquire 的时长似乎会更长，不知会不会影响效率？
- brelse 中对应的修改: 最后需要 refcnt--, 只需 acquire list.lock
- 可以顺便实现 LRU 策略
  + 维护一个全局时间 bcache.time 和 per-buf 的时间戳 stamp, 每次 refcnt 减至 0 时执行 buf.stamp = ++bcache.time.
  + 寻找 sacrifice 时，比较当前 buf 的 stamp 和当前找到的最小 stamp 比较，从而获得全局最小 stamp 的 buf
  + 在遍历完所有 buf 之前，我们不知道当前找到的 buf 是不是最好的，因此需要一直持有它的锁，直到发现一个更好的，或者遍历完所有 buf 并完成 eviction.
  + 这样做存在一个问题，为了保护 time 应该需要再 acquire bcache.lock, 这就使得 brelse 没能成功完成将原本的 bcache.lock 分散到各个 list.lock 上面去的任务，可能会导致针对 bcache.lock 的 lock contention 更激烈
  + 一种比较狡猾的解决方案是使用 __sync_fetch_and_add() 将 bcache.lock 的自加原子化。
  + 如果我们仅仅只是为了一个变量的自增不出错就去不厌其烦地获得锁，感觉还是太过浪费，一些比较小的同步需求可以尝试通过硬件提供的同步机制解决