#include"mmap_file.h"
#include<stdio.h>

static int debug = 1;	//如果日志信息太多会影响性能，所以设置开关


namespace program
{
	namespace largefile
	{
		//初始化与析构----------------------------------------------------
		MMapFile::MMapFile():
		size_(0),fd_(-1),data_(NULL)
		{
		}
		
		MMapFile::MMapFile(const int fd):
		size_(0),fd_(fd),data_(NULL)
		{
		}
		
		MMapFile::MMapFile(const MMapSizeOption& mmap_size_option,const int fd):
		size_(0),fd_(fd),data_(NULL)
		{
			mmapfile_size_option.max_mmap_size = mmap_size_option.max_mmap_size;
			mmapfile_size_option.first_mmap_size = mmap_size_option.first_mmap_size;
			mmapfile_size_option.per_mmap_size = mmap_size_option.per_mmap_size;
		}
		
		//解除映射，重置属性
		MMapFile::~MMapFile()
		{
			//如果还有数据
			if(data_)
			{
				if(debug) printf("mmap_file destruct,fd:%d,data:%p,maped_size:%d\n",fd_,data_,size_);
				msync(data_,size_,MS_SYNC);	//内存与磁盘同步
				munmap(data_,size_);		//解除映射
				
				size_ = 0;
				data_ = NULL;
				fd_ = -1;
				
				mmapfile_size_option.max_mmap_size = 0;
				mmapfile_size_option.first_mmap_size = 0;
				mmapfile_size_option.per_mmap_size = 0;
			}
		}
		
		//内存映射到文件(异步)是否成功--------------------------------------------------
		bool MMapFile::sync_file()
		{
			if(NULL!=data_&&size_>0)
			{
				return msync(data_,size_,MS_ASYNC)==0;	//同步成功否
			}
			return true;	//没有数据需要同步，也算同步成功
		}
		
		//文件映射到内存----------------------------------------------------------------
		bool MMapFile::map_file(const bool write)//进行文件内存映射操作，同时设置访问权限
		{
			int flags = PROT_READ;
			if(write)
			{
				flags |=PROT_WRITE;
			}
			if((fd_ < 0) && (0 == mmapfile_size_option.max_mmap_size))
			{
				return false;
			}
			//对于内存映射的大小设定，这里涉及到硬盘文件与内存大小的同步
			if(size_ < mmapfile_size_option.max_mmap_size)
			{
				size_ = mmapfile_size_option.first_mmap_size;
			}
			else
			{
				size_ = mmapfile_size_option.max_mmap_size;
			}
			
			//磁盘扩容
			if(ensure_file_size(size_) == 0)
			{
				fprintf(stderr,"ensure file size failed in map_file,size:%d",size_);
				return false;
			}
			
			//如果成功映射，映射到的内存首地址返回给指针
			data_ = mmap(0,size_,flags,MAP_SHARED,fd_,0);
			if(MAP_FAILED == data_)
			{
				fprintf(stderr,"map file failed: %s\n",strerror(errno));	//失败会返回错误编号errno，通过stderror拿到错误编号
				
				size_= 0;
				fd_= -1;
				data_ = NULL;
				return false;
			}
			
			if(debug)	printf("mmap file successed,fd:%d maped size: %d,data:%p\n",fd_,size_,data_);
			return true;
		}
		//拿到内存映射成功的这部分数据
		void* MMapFile::get_data() const		
		{
			return data_;
		}
		//拿到映射数据的大小
		int32_t MMapFile::get_size() const	
		{
			return size_;
		}
		//解除映射
		bool MMapFile::munmap_file()			
		{
			if(munmap(data_,size_) == 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		//重新映射(扩大/缩小现有内存映射)
		bool MMapFile::remap_file()			
		{
			//防御性编程
			if(fd_ < 0 || data_ == NULL)
			{
				fprintf(stderr,"mremap not yet\n");
				return false;
			}
			if(size_ == mmapfile_size_option.max_mmap_size)
			{
				fprintf(stderr,"already mapped max size:%d,now size:%d\n",size_,mmapfile_size_option.max_mmap_size);
				return false;
			}
			
			int32_t newsize = size_ + mmapfile_size_option.per_mmap_size;
			//如果内存空间达到最大值，那么就不能再大了，这里涉及到硬盘文件与内存大小的同步
			if(size_ > mmapfile_size_option.max_mmap_size)
			{
				newsize = mmapfile_size_option.max_mmap_size;
			}
			if(ensure_file_size(newsize) == 0)
			{
				fprintf(stderr,"ensure file size failed in map_file,size:%d",size_);
				return false;
			}
			
			if(debug) printf("mremap start fd:%d,now size: %d,new size:%d, old data:%p\n",fd_,size_,newsize,data_);
			
			//重新映射，指向一个新的地址，进行扩容，看flags，反之就是在原来的地方扩容，扩不了就是失败
			void* new_map_data = mremap(data_,size_,newsize,MREMAP_MAYMOVE);
			
			//if(MAP_FAILED == mremap(data_,size_,newsize,MREMAP_MAYMOVE))
			if(MAP_FAILED == new_map_data)
			{
				fprintf(stderr,"mremap failed,fd:%d,new size:%d,error desc: %s\n",fd_,newsize,strerror(errno));
				return false;
			}
			else
			{
				if(debug) 
					printf("mremap success. fd:%d,now size: %d,new size:%d, old data:%p, new data:%p\n",fd_,size_,newsize,data_,new_map_data);
	
			}
			
			data_ = new_map_data;
			size_ = newsize;
			
			return true;
		}		
		//磁盘扩容，涉及到fstat
		bool MMapFile::ensure_file_size(const int32_t size)
		{
			struct stat s;	//文件的状态
			//拿到文件的状态
			if(fstat(fd_,&s) < 0)
			{
				fprintf(stderr,"fstat error,error desc: %s\n",strerror(errno));
				return false;
			}
			//获取文件大小,如果磁盘文件小于内存，则磁盘扩容
			if(s.st_size < size)
			{
				//调整文件的大小
				if(ftruncate(fd_,size) < 0)
				{
					fprintf(stderr,"fstat error,size:%ld,error desc: %s\n",s.st_size,strerror(errno));
					return false;
				}
			}
			return true;
		}
	}
}