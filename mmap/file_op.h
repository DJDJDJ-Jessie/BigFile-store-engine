#ifndef LARGE_FILE_OP_H
#define LARGE_FILE_OP_H

#include"common.h"

namespace program
{
	namespace largefile
	{
		class FileOperation
		{
		public:
			FileOperation(const std::string &filename,const int open_flags = O_RDWR|O_LARGEFILE);
			~FileOperation();
			
			//打开关闭
			int open_file();
			void close_file();
		
			//保存删除
			int flush_file();	//write方法操作系统会把文件写入缓存到内存，所以该方法将文件直接写到磁盘			
			int unlink_file();
			
			//读写
			virtual int pread_file(char* buf, const int32_t nbytes,const int64_t offset);	//大文件，所以64位比较好
			virtual int pwrite_file(const char* buf,const int32_t nbytes,const int64_t offset);	//seek
			
			int write_file(const char* buf,const int32_t nbytes);//在当前位置直接开始写
			
			//拿到文件大小
			int64_t get_file_size();
			//截取文件
			int ftruncate_file(const int64_t length);
			//文件内容定位
			int seek_file(const int64_t offset);
			
			int get_fd() const
			{
				return fd_;
			}
			
		protected:
			int check_file();
		
			int fd_;	//拿到文件描述句柄
			int open_flags_;//文件打开方式（读写、不存在则创建等）
			char* file_name_;	//文件名
			
			static const mode_t OPEN_MODE = 0644;	//文件打开方式用户权限
			static const int MAX_DISK_TIMES = 5;	//读取文件失败，尝试5次就不读了（可能负载过高也可能磁盘出问题等）
			
		};
	}
}
#endif