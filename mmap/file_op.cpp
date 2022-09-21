#include "file_op.h"
#include "common.h"

static int debug = 1;

namespace program
{
	namespace largefile
	{
		FileOperation::FileOperation(const std::string &filename,const int open_flags):
		fd_(-1),open_flags_(open_flags)
		{
			//注意有分配内存
			file_name_ = strdup(filename.c_str());//字符串复制，相当于重新分配一块内存，把内容复制过去
		}
		
		//关闭文件句柄，清空分配的内存
		FileOperation::~FileOperation()
		{
			if(fd_>0)
			{
				::close(fd_);	//说明是全局的不属于当前作用域，直接调用库文件中的函数
			}
			if(NULL!=file_name_)
			{
				free(file_name_);
				file_name_ = NULL;
			}
			
		}
		
		//打开关闭------------------------------------------------------------------------
		int FileOperation::open_file()
		{
			//如果已经打开就先关掉再打开
			if(fd_>0)
			{
				close(fd_);
				fd_=-1;
			}
			fd_ = ::open(file_name_,open_flags_,OPEN_MODE);	//注意这一句和下一句的顺序不能颠倒
			if(fd_<0)
			{
				return -errno;
			}
			return fd_;
		}
		void FileOperation::close_file()
		{
			if(fd_<0)
			{
				return;
			}
			::close(fd_);
			fd_ = -1;
		}
		
		//保存删除------------------------------------------------------------------------
		int FileOperation::flush_file()
		{
			//O_SYNC立即同步到磁盘，成功才返回
			if(open_flags_ & O_SYNC)
			{
				return 0;
			}
			//偷偷打开文件
			int fd_ = check_file();
			if(fd_ < 0)
			{
				return fd_;
			}
			
			return fsync(fd_); //内存数据写回磁盘
		}
		
		
		int FileOperation::unlink_file()
		{
			close_file();
			return ::unlink(file_name_);
		}
		
		//读写------------------------------------------------------------------------
		int FileOperation::pread_file(char* buf, const int32_t nbytes,const int64_t offset)
		{
			//方法一：read函数参数不匹配，所以还需要lseek
			//方法二：直接用pread64函数（大文件），其不会自动移动文件指针(注意参数类型一定要匹配！！)
			//有offset有可能不能一次性读完要多次读取！并且因为可能会出现服务器暂时无响应的情况，要多尝试几次。
			int32_t left = nbytes;	//剩余字节数
			int64_t read_offset = offset;	//起始位置偏移量
			int32_t read_len = 0;	//已读的长度
			char* p_tmp = buf;		//读到的数据位置是buf的起始位置
			
			int i = 0;
			
			while(left > 0)
			{
				++i;
				if(i >= MAX_DISK_TIMES)
				{
					break;
				}
				
				//偷偷打开文件
				if(check_file() < 0)
				{
					return -errno;
				}
				
				//读取
				read_len = ::pread64(fd_, p_tmp, left, read_offset);
				
				//读的大小有问题或读完了
				if(read_len < 0)
				{
					read_len = -errno;	//出错的原因保存到read_len
					//这里为什么不用errno？因为errno是全局变量，多线程环境下，都可以对errno进行改变
					//EINTR系统中断（繁忙）即临时出错 或 系统要求重新尝试
					if(-read_len == EINTR || EAGAIN == -read_len)
					{
						continue;
					}
					//EBADF文件描述符已经坏了
					else if(EBADF == -read_len)
					{
						fd_ = -1;
						//return read_len;
						continue;
					}
					else
					{
						return read_len;
					}			
				}
				else if(0 == read_len)
				{
					break;
				}
				
				left -= read_len;
				p_tmp += read_len;
				read_offset += read_len;
			}		
			
			if(0 != left)
			{
				return EXIT_DISK_OPEN_INCOMPLETE;
			}
			
			return TFS_SUCCESS;
			//return pread64(fd_, buf, size_t nbytes, offset);
		}
		int FileOperation::pwrite_file(const char* buf,const int32_t nbytes,const int64_t offset)
		{
			int32_t left = nbytes;	//剩余字节数
			int64_t write_offset = offset;	//起始位置偏移量
			int32_t written_len = 0;	//已写的长度
			const char* p_tmp = buf;		//写到的数据位置是buf的起始位置
			
			int i = 0;
			
			//最多写五次
			while(left > 0)
			{
				++i;
				if(i >= MAX_DISK_TIMES)
				{
					break;
				}
				
				if(check_file() < 0)
				{
					return -errno;
				}
				
				//写
				written_len = ::pwrite64(fd_, p_tmp, left, write_offset);
				
				//写的大小有问题或读完了
				if(written_len < 0)
				{
					written_len = -errno;	//出错的原因保存到read_len
					//这里为什么不用errno？因为errno是全局变量，多线程环境下，都可以对errno进行改变
					//EINTR系统中断（繁忙）即临时出错 或 系统要求重新尝试
					if(-written_len == EINTR || EAGAIN == -written_len)
					{
						continue;
					}
					//EBADF文件描述符已经坏了
					else if(EBADF == -written_len)
					{
						fd_ = -1;
						continue;
					}
					else
					{
						return written_len;
					}			
				}
				
				left -= written_len;
				p_tmp += written_len;
				write_offset += written_len;
			}		
			
			if(0 != left)
			{
				return EXIT_DISK_OPEN_INCOMPLETE;
			}
			
			return TFS_SUCCESS;
			
		}
		
		int FileOperation::write_file(const char* buf,const int32_t nbytes)
		{
			//write方法是根据文件指针走的
			int32_t left = nbytes;		//剩余字节数
			int32_t written_len = 0;	//已写的长度
			const char* p_tmp = buf;			//写到的数据位置是buf的起始位置
			
			int i = 0;
			
			while(left > 0)
			{
				++i;
				if(i >= MAX_DISK_TIMES)
				{
					break;
				}
				
				if(check_file() < 0)
				{
					return -errno;
				}
				
				//写
				written_len = ::write(fd_, p_tmp, left);
				
				//写的大小有问题或读完了
				if(written_len < 0)
				{
					written_len = -errno;	//出错的原因保存到read_len
					//这里为什么不用errno？因为errno是全局变量，多线程环境下，都可以对errno进行改变
					//EINTR系统中断（繁忙）即临时出错 或 系统要求重新尝试
					if(-written_len == EINTR || EAGAIN == -written_len)
					{
						continue;
					}
					//EBADF文件描述符已经坏了
					else if(EBADF == -written_len)
					{
						fd_ = -1;
						continue;
					}
					else
					{
						return written_len;
					}			
				}
				
				left -= written_len;
				p_tmp += written_len;
			}		
			
			if(0 != left)
			{
				return EXIT_DISK_OPEN_INCOMPLETE;
			}
			
			return TFS_SUCCESS;
		}
		
		//偷偷打开文件
		int FileOperation::check_file()
		{
			if(fd_<0)
			{
				fd_ = open_file();
			}
			
			return fd_;
		}
		
		//拿到文件大小
		int64_t FileOperation::get_file_size()
		{
			//不打开文件(明面)也可以拿到文件大小
			int fd = check_file();
			
			if(fd_<0)
			{
				return fd_;
			}
			
			//拿到文件的状态
			struct stat buf;
			if(fstat(fd_,&buf) < 0)
			{
				fprintf(stderr,"file_op fstat error,error desc: %s\n",strerror(errno));
				return -1;
			}
			
			return buf.st_size;
		}
		//截取文件（缩小文件）
		int FileOperation::ftruncate_file(const int64_t length)
		{
			//偷偷打开文件
			int fd = check_file();
			
			if(fd_<0)
			{
				return fd_;
			}
			
			return ftruncate(fd_, length);
		}
		//文件内容定位
		int FileOperation::seek_file(const int64_t offset)
		{
			//偷偷打开文件
			int fd = check_file();
			
			if(fd_<0)
			{
				return fd_;
			}
			
			return lseek(fd_,offset,SEEK_SET);	//相对于文件头部移动
		}	
		
	}
}