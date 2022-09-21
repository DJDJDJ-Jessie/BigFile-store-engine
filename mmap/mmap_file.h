#ifndef MMAP_FILE_H
#define MMAP_FILE_H

#include<unistd.h>
#include"common.h" 
namespace program
{
	namespace largefile
	{	
		class MMapFile
		{
			public:
			MMapFile();	
			explicit MMapFile(const int fd);	//避免隐式构造（避免一个参数的，带来歧义）
			MMapFile(const MMapSizeOption& mmap_size_option,const int fd);
			~MMapFile();
			
			//文件映射到内存-------------------------------------------------------------
			//进行内存映射一定要fd
			bool map_file(const bool write=false);//进行文件内存映射操作，同时设置访问权限
			void* get_data() const;//拿到内存映射成功的这部分数据
			int32_t get_size() const;//拿到映射数据的大小
			
			bool munmap_file();	//解除映射
			bool remap_file();	//重新映射
			
			//内存映射到文件，即内存内容同步到磁盘-------------------------------------------
			bool sync_file();	//同步文件
			
			private:
			bool ensure_file_size(const int32_t size);//磁盘扩容
			
			//内存映射的属性
			private:
			int32_t size_;
			int fd_;
			void* data_;
			struct MMapSizeOption mmapfile_size_option;
		};
	}
}


#endif