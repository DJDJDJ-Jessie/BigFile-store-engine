# BigFile-store-engine
implement a bigfile store engine from TFS which based on Hash structure written in C++ language
system:Linux Ubuntu

来自TFS的大文件（非结构化数据）的存储引擎，核心是内存映射和哈希链表。
实现了小文件的写入、读取、删除，这些操作都会设计到块和索引文件的变化
一个块由一个索引文件、一个主块文件（64M）、一个扩展文件（数据移除）组成
