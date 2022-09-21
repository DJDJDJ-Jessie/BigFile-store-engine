#ifndef LARGEFILE_INDEX_HANDLE_H
#define LARGEFILE_INDEX_HANDLE_H

#include"common.h"
#include"mmap_file_op.h"

namespace program
{
	namespace largefile
	{
		struct IndexHeader
		{
			//初始化索引文件信息
			IndexHeader()
			{
				memset(this,0,sizeof(IndexHeader));
			}
			BlockInfo block_info_;	//块信息
			int32_t bucket_size_;	//哈希桶大小
			int32_t data_file_size_;	//未使用数据起始的偏移
			int32_t index_file_offset_;	//索引文件当前偏移(索引文件大小)
			int32_t reuse_head_offset_;	//可重用的链表节点		
		};
		
		class IndexHandle
		{
		public:
			//拿到文件映射操作对象
			IndexHandle(const std::string& base_path,const uint32_t main_block_id);
			~IndexHandle();
			
			//创建索引文件需要：逻辑块的id、哈希桶的大小、内存映射的选项（因为创建时会执行内存映射）
			//将索引文件读到内存，然后同步到磁盘，然后进行内存映射
			int create(const uint32_t logic_block_id,const int32_t bucket_size, const MMapSizeOption mmap_size_option);
			
			 //加载索引文件到内存
            int load(const uint32_t logic_block_id, const int32_t _bucket_size, const MMapSizeOption mmap_size_option);
			
			//删除索引文件（解除映射，删除文件）
			int remove(const uint32_t logic_block_id);
			
			//同步磁盘
			int flush();
			
			//拿到哈希桶大小
			int32_t bucket_size() const
            {
                return (reinterpret_cast<IndexHeader*> (mmap_file_op_->get_map_data()))->bucket_size_;
            }
			//拿到映射到内存（起始地址）的索引文件的索引
			IndexHeader* index_header()
			{
				//强制类型转换
				return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data());
			}
			
			//拿到大文件块的地址
			BlockInfo* block_info()
			{
				return reinterpret_cast<BlockInfo*>(mmap_file_op_ ->get_map_data()); 
			}
			
			//拿到未使用数据的起始偏移(块数据偏移)
			int32_t get_block_data_offset() const
            {
				//索引头部中拿到（索引处理中拿不到的）
                return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->data_file_size_;
            }
			
			//拿到可重用链表位置
			int32_t reuse_head_offset() const
            {
				//索引头部中拿到（索引处理中拿不到的）
                return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->reuse_head_offset_;
            }
			
			//拿到哈希桶在块索引中的位置（文件保存后会直接映射在内存中）（先变成1字节然后转为4字节的）
            int32_t* bucket_slot()
            {
                return reinterpret_cast<int32_t*> (reinterpret_cast<char*> (mmap_file_op_->get_map_data()) + sizeof(IndexHeader));
            }
			
			//更新未使用数据起始的偏移
            void commit_block_data_offset(const int file_size)
            {
                reinterpret_cast<IndexHeader*> (mmap_file_op_->get_map_data())->data_file_size_ += file_size;
            }
		
			//更新块信息
			int update_block_info(const OpType op_type,const uint32_t modify_size);
			
		
			//拿到文件映射操作对象
            MMapFileOperation* get_file_op_()
            {
                return mmap_file_op_;
            }
			
			
			
			//哈希写到哈希文件中去，传递键值和哈希索引块
			int32_t write_segment_meta(const uint64_t key, MetaInfo& meta);

            int32_t read_segment_meta(const uint64_t key, MetaInfo& meta);

            int32_t delete_segment_meta(const uint64_t key);
		private:
			//哈希比较
			bool hash_compare(const uint64_t left_key, const uint64_t right_key)
            {
                return (left_key == right_key);
            }
			
			//哈希查找
            int32_t hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset);
			
			//增加哈希索引
            int32_t hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta);
			
		private:
			MMapFileOperation* mmap_file_op_;//文件映射操作对象
			bool is_load_;					//索引文件是否被加载
		};
	}
}
#endif
