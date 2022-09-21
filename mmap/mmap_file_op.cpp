#include"mmap_file_op.h"
#include"common.h"

static int debug = 1;

namespace program
{
	namespace largefile
	{
		int MMapFileOperation::mmap_file(const MMapSizeOption& mmap_size_option)
		{
			//最大映射大小有问题
			if(mmap_size_option.max_mmap_size < mmap_size_option.first_mmap_size)
			{
				return TFS_ERROR;
			}
			if(mmap_size_option.max_mmap_size <= 0)
			{
				return TFS_ERROR;
			}
			
			//偷偷打开文件失败
			int fd = check_file();
			if(fd < 0)
			{
				fprintf(stderr,"MMapFileOperation::mmap_file -- check_file failed");
				return TFS_ERROR;
			}
			
			//如果没有映射，那么就映射一下
			if(!is_mapped_)
			{
				//如果有映射对象，那么先删除后重新映射
				if(map_file_)
				{
					delete(map_file_);
				}
				map_file_ = new MMapFile(mmap_size_option,fd);
				is_mapped_ = map_file_->map_file(true);	//映射内存
			}
			//看映射是否成功
			if(is_mapped_)
			{
				return TFS_SUCCESS;
			}
			else
			{
				return TFS_ERROR;
			}
		}
		
		
		//解除映射
		int MMapFileOperation::munmap_file()
		{
			//如果之前映射成功才需要解除映射
			if(is_mapped_ && map_file_!=NULL)
			{
				delete(map_file_);	//删除时会析构~MMapFile()
				map_file_ = NULL;
			}	
		}

		void* MMapFileOperation::get_map_data() const
		{
			//映射了才可以拿到映射块的指针
			if(is_mapped_)
			{
				return map_file_->get_data();
			}
			
			return NULL;
		}
		
		//重写--读写
		int  MMapFileOperation::pread_file(char* buf,const int32_t size,const int64_t offset)
		{
			
			//情况1.内存已经映射，要读取的数据大于映射内存空间（内存空间不够）
			//注意顺序不要颠倒，尝试追加一次内存，如果还不够就算了（根据需求来，可以用while追加内存直到空间足够，不过要注意不要是死循环，比如永远满足不了）
			if(is_mapped_ && (offset + size) > map_file_->get_size())
			{
				if(debug) fprintf(stdout, "mmap file pread, size: %d, offset: %" __PRI64_PREFIX"d, map file size: %d. need remap\n",
				size, offset, map_file_->get_size());
				
				map_file_->remap_file();	//追加内存
			}
			//情况1.内存已经映射，要读取的数据小于映射内存空间（内存空间够）
			if(is_mapped_ && (offset + size) <= map_file_->get_size())
			{
				//copy到buf中
				memcpy(buf,(char*)map_file_->get_data()+offset,size);
				return TFS_SUCCESS;
			}
			
			//情况2.内存没有映射 或 要读取的数据映射不全
			return FileOperation::pread_file(buf,size,offset);
		}
		
		int  MMapFileOperation::pwrite_file(const char* buf,const int32_t size,const int64_t offset)
		{
			//情况1.内存已经映射，要写的数据小于映射内存空间（内存空间不够）
			if(is_mapped_ && (offset + size) > map_file_->get_size())
			{
				if(debug) fprintf(stdout, "mmap file pwrite, size: %d, offset: %" __PRI64_PREFIX"d, map file size: %d. need remap",
				size, offset, map_file_->get_size());
				
				map_file_->remap_file();	//追加内存
			}
			//情况1.内存已经映射，要写的数据小于映射内存空间（内存空间够）
			if(is_mapped_ && (offset + size) <= map_file_->get_size())
			{
				//注意这里是copy buf到内存去
				memcpy((char*)map_file_->get_data()+offset,buf,size);
				return TFS_SUCCESS;
			}
			
			//情况2.内存没有映射 或 要写入的数据映射不全
			return FileOperation::pwrite_file(buf,size,offset);
		}
		
		int MMapFileOperation::flush_file()
		{
			//映射了才需要同步
			if(is_mapped_)
			{
				if(map_file_->sync_file())//内存映射到文件(异步)是否成功
				{
					return TFS_SUCCESS;
				}
				else
				{
					return TFS_ERROR;
				}
			}
			
			//写入到磁盘
			return FileOperation::flush_file();
		}
	}
}