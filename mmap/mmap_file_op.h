#ifndef LARGEFILE_MMAPFILE_OP_H
#define LARGEFILE_MMAPFILE_OP_H

#include"common.h"
#include"file_op.h"
#include"mmap_file.h"

namespace program
{
	namespace largefile
	{
		class MMapFileOperation:public FileOperation
		{
		public:
			MMapFileOperation(const std::string &filename,const int open_flags = O_CREAT|O_RDWR|O_LARGEFILE):
			FileOperation(filename,open_flags),map_file_(NULL),is_mapped_(false){}//因为父类没有默认构造函数，所以要显式调用
			
			~MMapFileOperation()
			{
				if(map_file_)
				{
					delete(map_file_);
					map_file_=NULL;
				}
				
			}
			//映射内存
			int mmap_file(const MMapSizeOption& mmap_size_option);
			int munmap_file();
			
			//重写读写
			int pread_file(char* buf,const int32_t size,const int64_t offset);
			int pwrite_file(const char* buf,const int32_t size,const int64_t offset);
			
			//拿到映射地址；写入磁盘
			void* get_map_data() const;
			int flush_file();
			
		private:
			MMapFile* map_file_;	//要操作的文件映射对象
			bool is_mapped_;		//是否已经映射，因为初始化时不一定初始化文件映射对象
		};
	}
}
#endif