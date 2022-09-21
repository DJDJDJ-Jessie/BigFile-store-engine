#ifndef _COMMON_H
#define _COMMON_H

#include<iostream>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>	//msync
#include<string>
#include<string.h>
#include<stdint.h>		//int32_t
#include<errno.h>		//errno  stderr strerror
#include<stdio.h>		//fprintf
#include<unistd.h>
#include<stdlib.h>		//free
#include<inttypes.h>	//__PRI64_PREFIX
#include<assert.h>

//加一个标识
namespace program
{
	namespace largefile
	{
		const int32_t EXIT_DISK_OPEN_INCOMPLETE = -8012;//read or write length is less than required
		const int32_t TFS_SUCCESS = 0;		
		const int32_t TFS_ERROR = -1;		
		const int32_t EXIT_INDEX_ALREADY_LOADED_ERROR = -8013;	//index is loaded when create or load.		
		const int32_t EXIT_META_UNEXPECTED_FOUND_ERROR = -8014;	//meta found in index when insert.
		const int32_t EXIT_INDEX_CORRUPT_ERROR = -8015;	//index file is corrupt.
		const int32_t EXIT_BLOCKID_CONFLICT_ERROR = -8016;	//block id conflict.
		const int32_t EXIT_BUCKET_CONFIGURE_ERROR = -8017;	//bucket size error.
		const int32_t EXIT_META_NOT_FOUND_ERROR = -8018;	//META_NOT_FOUND.
		const int32_t EXIT_BLOCKID_ZERO_ERROR = -8019;	//BLOCKID_ZERO.
		
		
		//基础路径信息
		static const std::string MAINBLOCK_DIR_PREFIX = "/mainblock/";
		static const std::string INDEX_DIR_PREFIX = "/index/";
		static const mode_t DIR_MODE = 0755;
		
		//定义索引处理的一些类型（更新块信息这里需要用到）
		enum OpType
		{
			C_OP_INSERT = 1,
			C_OP_DELETE
		};
		
		//设定三个映射类型的大小
		struct MMapSizeOption
		{
			int32_t max_mmap_size;
			int32_t first_mmap_size;
			int32_t per_mmap_size;
		};
		
		//大文件块结构
		struct BlockInfo
		{
			uint32_t block_id_;
			int32_t version_;
			int32_t file_count_;
			int32_t size_;
			int32_t del_file_count_;
			int32_t del_size_;
			uint32_t seq_no_;
			
			BlockInfo()
			{
				memset(this,0,sizeof(BlockInfo));	//所有成员清零
			}
			
			//块是否相等
			inline bool operator==(const BlockInfo& rhs) const
			{
				return block_id_ == rhs.block_id_&&version_ == rhs.version_&&file_count_ == rhs.file_count_
				&&size_ == rhs.size_&&del_file_count_ == rhs.del_file_count_&&del_size_ == rhs.del_size_
				&&seq_no_ == rhs.seq_no_;
			}
		};
	
	
		//文件哈希索引块
		struct MetaInfo
		{
			MetaInfo() { init(); }
			MetaInfo(const uint64_t file_id,const int32_t in_offset,const int32_t file_size,const int32_t next_meta_offset)
			{
				file_id_ = file_id;
				location_.inner_offset_ = in_offset;
				location_.size_ = file_size;
				next_meta_offset_ = next_meta_offset;
			}
			//拷贝构造
			MetaInfo(const MetaInfo& meta_info)
			{
				memcpy(this,&meta_info,sizeof(MetaInfo));
			}
			
			//赋值拷贝
			MetaInfo& operator=(const MetaInfo& meta_info)
			{
				if(this == &meta_info)
				{
					return *this;
				}
				
				file_id_ = meta_info.file_id_;
				location_.inner_offset_ = meta_info.location_.inner_offset_;
				location_.size_ = meta_info.location_.size_;
				next_meta_offset_ = meta_info.next_meta_offset_;
			}
			
			//克隆
			MetaInfo& clone(const MetaInfo& meta_info)
			{
				assert(this!=&meta_info);
				
				file_id_ = meta_info.file_id_;
				location_.inner_offset_ = meta_info.location_.inner_offset_;
				location_.size_ = meta_info.location_.size_;
				next_meta_offset_ = meta_info.next_meta_offset_;
				
				return *this;
			}			
			
			//小文件是否相等
			bool operator==(const MetaInfo& rhs) const
			{
				return file_id_ == rhs.file_id_&&location_.inner_offset_ == rhs.location_.inner_offset_
				&&location_.size_ == rhs.location_.size_&&next_meta_offset_ == rhs.next_meta_offset_;
			}
			
			uint64_t get_key() const
			{
				return file_id_;
			}			
			void set_key(const uint64_t key)
			{
				file_id_ = key;
			}
			
			uint64_t get_file_id() const
			{
				return file_id_;
			}
			void set_file_id(const uint64_t file_id)
			{
				file_id_ = file_id;
			}
			
			int32_t get_offset() const
			{
				return location_.inner_offset_;
			}
			
			void set_offset(const int32_t offset)
			{
				location_.inner_offset_ = offset;
			}
			
			int32_t get_size() const
			{
				return location_.size_;
			}
			
			void set_size(const int32_t file_size)
			{
				location_.size_ = file_size;
			}
			
			int32_t get_next_meta_offset_() const
			{
				return next_meta_offset_;
			}
			
			void set_next_meta_offset_(const int32_t offset)
			{
				next_meta_offset_ = offset;
			}
			
		private:
			//小文件的一些信息
			uint64_t file_id_;
			
			//小文件在大文件中的位置和大小
			struct 
			{
				int32_t inner_offset_;
				int32_t size_;
			}location_;
			
			int32_t next_meta_offset_;
		private:
			void init()
			{
				memset(this,0,sizeof(MetaInfo));	//所有成员清零
			}	
		};
	
	
	}
}

#endif	/*_COMMON_H*/