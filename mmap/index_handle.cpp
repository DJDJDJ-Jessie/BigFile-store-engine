#include"common.h"
#include"index_handle.h"
#include<sstream>

static int debug = 1;

namespace program
{
	namespace largefile
	{
		IndexHandle::IndexHandle(const std::string& base_path,const uint32_t main_block_id)
		{
			//进行路径信息的初始化
			std::stringstream tmp_stream;
			tmp_stream << base_path << INDEX_DIR_PREFIX << main_block_id;	//   /root/user/index/1
			
			std::string index_path;
			tmp_stream >> index_path;
			
			//用路径信息来进行文件映射(初始化句柄等)
			mmap_file_op_ = new MMapFileOperation(index_path, O_CREAT|O_RDWR|O_LARGEFILE);
			is_load_ = false;
		}
		
		IndexHandle::~IndexHandle()
		{
			if(mmap_file_op_)
			{
				delete mmap_file_op_;
				mmap_file_op_ = NULL;
			}		
		}
		
		//创建索引文件（映射到内存）
		int IndexHandle::create(const uint32_t logic_block_id,const int32_t bucket_size, const MMapSizeOption mmap_size_option)
		{
			int ret = TFS_SUCCESS;
			
			if(debug) printf("create index, block id:%u, bucket size:%d, max_mmap_size:%d ,first_mmap_size:%d , per_mmap_size:%d\n",
							logic_block_id, bucket_size, mmap_size_option.max_mmap_size, mmap_size_option.first_mmap_size,mmap_size_option.per_mmap_size);
				
			//如果索引文件已经加载
			if(is_load_)
			{
				return EXIT_INDEX_ALREADY_LOADED_ERROR;
			}
			
			//如果索引文件不存在
			int64_t file_size = mmap_file_op_->get_file_size();
			if(file_size < 0)
			{
				return TFS_ERROR;
			}
			else if(file_size == 0)
			{
				//创建索引头部，并分配文件哈希索引块内存
				IndexHeader i_header;
				i_header.block_info_.block_id_ = logic_block_id;
				i_header.block_info_.seq_no_ = 1;
				i_header.bucket_size_ = bucket_size;
				
				//索引文件当前偏移（文件大小） = 索引头部大小 + 文件哈希索引块大小
				i_header.index_file_offset_ = sizeof(IndexHeader) + bucket_size * sizeof(int32_t);
				
				
				//先将索引文件写入内存
				char* init_data = new char[i_header.index_file_offset_];
				memcpy(init_data, &i_header, sizeof(IndexHeader));	//索引头部写到init_data
				memset(init_data + sizeof(IndexHeader), 0, i_header.index_file_offset_ - sizeof(IndexHeader));//哈希索引块写到init_data中的头部后面
				
				ret = mmap_file_op_->pwrite_file(init_data,i_header.index_file_offset_,0);//写到内存
				
				delete[] init_data;	//写入内存后，记得要释放临时空间
				init_data = NULL;
				
				if(ret!=TFS_SUCCESS)
				{
					return ret;
				}
				
				//将内存中索引文件写入磁盘
				ret = mmap_file_op_->flush_file();
				
				if(ret!=TFS_SUCCESS)
				{
					return ret;
				}
				
			}
			else	//file size > 0, index already exist
			{
				return EXIT_META_UNEXPECTED_FOUND_ERROR;
			}
			
			//索引文件初始化成功后，映射到内存
			ret = mmap_file_op_-> mmap_file(mmap_size_option);
			if(ret != TFS_SUCCESS)
			{
				return ret;
			}
			
			is_load_ = true;
			
			//因为映射到内存中去了，所以不用这里的i_header
			if(debug) printf("init index success, block id:%u, data file size:%d, index file size:%d ,bucket file size:%d , free head offset:%d ,seqno:%d, size:%d, file count:%d, del size:%d,del file count:%d, version:%d\n",
								logic_block_id, index_header()->data_file_size_,
								index_header()->index_file_offset_,index_header()->bucket_size_,
								index_header()->reuse_head_offset_,block_info()->seq_no_,
								block_info()->size_,block_info()->file_count_,block_info()->del_size_,
								block_info()->del_file_count_,block_info()->version_);				
			
			return TFS_SUCCESS;
		}
		
		
		//加载索引文件（映射入内存）
		int IndexHandle::load(const uint32_t logic_block_id, const int32_t _bucket_size, const MMapSizeOption mmap_size_option)
		{
			//如果已经加载
			if (is_load_)
            {
                return EXIT_INDEX_ALREADY_LOADED_ERROR;
            }

			//如果索引文件大小不存在(根据拿到文件大小)
            int ret = TFS_SUCCESS;
            int file_size = mmap_file_op_->get_file_size();
            if (file_size < 0)
            {
                return file_size;
            }
            else if (file_size == 0) // empty file
            {
                return EXIT_INDEX_CORRUPT_ERROR;
            }

            //如果索引文件大小小于最大映射内存但是比首次映射内存要大，那么重新设置首次大小
            largefile::MMapSizeOption tmp_map_option = mmap_size_option;
            if (file_size > tmp_map_option.first_mmap_size&& file_size <= tmp_map_option.max_mmap_size)
            {
                tmp_map_option.first_mmap_size = file_size;
            }
			
			//将索引文件映射入内存
            ret = mmap_file_op_->mmap_file(tmp_map_option);
            if (TFS_SUCCESS != ret)
                return ret;

			if(debug) printf("IndexHandle::load. - bucket_size():%d, bucket_size_:%d, block_id:%d\n",bucket_size(),_bucket_size,block_info()->block_id_);
			
			
            //检查主块文件id和哈希桶大小是否合法 
            if (0 == bucket_size() || 0 == block_info()->block_id_)
            {
                fprintf(stderr,"Index corrupt error. blockid: %u, bucket size: %d\n", block_info()->block_id_, bucket_size());
                return EXIT_INDEX_CORRUPT_ERROR;
            }
			
			 //检查实际索引文件大小和拿到的文件大小是否一样
            int32_t index_file_offset_ = sizeof(IndexHeader) + bucket_size() * sizeof(int);
            if (file_size < index_file_offset_)   //映射内存不够
            {
                fprintf(stderr, "Index corrupt error. blockid: %u, bucket size: %d, file size: %d, index file size: %d\n",
                    block_info()->block_id_, bucket_size(), file_size, index_file_offset_);
                return EXIT_INDEX_CORRUPT_ERROR;
            }
			
			//检查哈希桶大小是否对的上
            if (_bucket_size != bucket_size())
            {
                fprintf(stderr, "Index configure error. old bucket size: %d, new bucket size: %d", bucket_size(), _bucket_size);
                return EXIT_BUCKET_CONFIGURE_ERROR;
            }

            //检查文件id，看是否冲突
            if (logic_block_id != block_info()->block_id_)
            {
                fprintf(stderr, "block id conflict. blockid: %u, index blockid: %u", logic_block_id, block_info()->block_id_);
                return EXIT_BLOCKID_CONFLICT_ERROR;
            }

			
			is_load_ = true;		
			//因为映射到内存中去了，所以不用这里的i_header
			if(debug) printf("init index success, block id:%u, data file size:%d, index file size:%d ,bucket file size:%d , free head offset:%d,seqno:%d, size:%d, file count:%d, del size:%d,del file count:%d, version:%d\n",
								logic_block_id, index_header()->data_file_size_,
								index_header()->index_file_offset_,index_header()->bucket_size_,
								index_header()->reuse_head_offset_,block_info()->seq_no_,
								block_info()->size_,block_info()->file_count_,block_info()->del_size_,
								block_info()->del_file_count_,block_info()->version_);				
			
			return TFS_SUCCESS;
		}
		
		
		
		//移除索引文件（解除映射，删除文件）
		int IndexHandle::remove(const uint32_t logic_block_id)
		{
			//如果已经加载
			if(is_load_)
			{
				if(logic_block_id!= block_info()->block_id_)
				{
					fprintf(stderr,"block id conflict.blockid:%d,index blockid:%d\n",logic_block_id,block_info()->block_id_);
					return EXIT_BLOCKID_CONFLICT_ERROR;
				}
			}
			
			//这里为什么可以这样调用,因为MapFileOperation* mmap_file_op_;//文件映射操作对象
			int ret = mmap_file_op_->munmap_file();
			if(TFS_SUCCESS!=ret)
			{
				return ret;
			}
			
			ret = mmap_file_op_->unlink_file();
			return ret;
		}
		
		//同步磁盘
		int IndexHandle::flush()
		{
			int ret = mmap_file_op_->flush_file();
			
			if(TFS_SUCCESS!=ret)
			{
				fprintf(stderr,"index flush fail. ret:%d error desc:%s\n",ret,strerror(errno));
			}
			
			return ret;
		}
		
		//哈希写到哈希文件中去，传递键值和哈希索引块
		int32_t IndexHandle::write_segment_meta(const uint64_t key, MetaInfo& meta)
		{
			int32_t current_offset = 0, previous_offset = 0;
			
			//思考？ key存在吗？ 存在如何处理、不存在如何处理？			
			//1.从文件哈希表中查找key是否存在 hash_find(key,current_offset,previous_offset)
			int ret = hash_find(key,current_offset,previous_offset);
			if(TFS_SUCCESS == ret)
			{
				return EXIT_META_UNEXPECTED_FOUND_ERROR;
			}
			else if(EXIT_META_NOT_FOUND_ERROR!=ret)
			{
				return ret;
			}
			
			//2.不存在就写入mata到文件哈希表中 hash_insert(meta,slot,previous_offset)
			//hash_insert(key,previous_offset,meta)
			ret = hash_insert(key,previous_offset,meta);
			return ret;
		}
		
		//哈希查找
        int32_t IndexHandle::hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset)
		{
			int ret = TFS_SUCCESS;
			MetaInfo meta_info;
			
			current_offset = 0;
			previous_offset = 0;
			
			//1.确定key存放的桶slot的位置
			int32_t slot = static_cast<uint32_t>(key) % bucket_size();
			//2.读取桶首节点存储的第一个节点的偏移量，如果偏移量为0，直接返回EXIT_META_NOT_FOUND_ERROR
			//3.根据偏移量读取存储的metainfo
			//4.与key进行比较，相等则设置current_offset和previous_offset并返回success，否则继续执行
			//5.从metainfo中取得下一个节点在文件中的偏移量如果偏移量为0，直接返回EXIT_META_NOT_FOUND_ERROR，否则跳转继续执行3
			int32_t pos = bucket_slot()[slot];				
			for(;pos!=0;)
			{
				//读节点位置
				ret = mmap_file_op_->pread_file(reinterpret_cast<char*>(&meta_info),sizeof(MetaInfo),pos);
				if(TFS_SUCCESS!=ret)
				{
					return ret;
				}
				//比较读出来的是否和想要的key是一样的
				if(hash_compare(key,meta_info.get_key()))
				{
					current_offset = pos;
					return TFS_SUCCESS;
				}
				//更新偏移位置
				previous_offset = pos;
				pos = meta_info.get_next_meta_offset_();	
			}
			
			return EXIT_META_NOT_FOUND_ERROR;			
		}
		
		//增加哈希索引
        int32_t IndexHandle::hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta)
		{
			int ret = TFS_SUCCESS;
            MetaInfo tmp_meta_info;
			int32_t current_offset = 0;
			//1.确定key存放的桶slot的位置
			int32_t slot = static_cast<uint32_t>(key) % bucket_size();
			
			//2.确定meta节点存储在文件中的偏移量
			if(reuse_head_offset()!=0)		//看可重用链表中有没有合适的
			{
				ret = mmap_file_op_->pread_file(reinterpret_cast<char*>(&tmp_meta_info),sizeof(MetaInfo),reuse_head_offset());
				if(TFS_SUCCESS!=ret)
				{
					return ret;
				}
				//剥离得到的节点
				current_offset = index_header()->reuse_head_offset_;
				if(debug) printf("reuse meta_info,current offset:%d\n",current_offset);
				index_header()->reuse_head_offset_ = tmp_meta_info.get_next_meta_offset_();//设置下一个可重用节点
			}
			else
			{
				current_offset = index_header()->index_file_offset_;
				index_header()->index_file_offset_+=sizeof(MetaInfo);
			}
				
			//3.将meta节点写入索引文件中
			meta.set_next_meta_offset_(0);
			
			ret = mmap_file_op_->pwrite_file(reinterpret_cast<const char*>(&meta),sizeof(MetaInfo),current_offset);
			if(TFS_SUCCESS!=ret)	//如果写入不成功
			{
				index_header()->index_file_offset_-=sizeof(MetaInfo);
				return ret;
			}
			
			//4.如果写入成功，那么meta节点插入到哈希链表中
			if(0!=previous_offset)	//当前一个节点已经存在
			{	
				//先读前一个节点到tmp_meta_info
				ret = mmap_file_op_->pread_file(reinterpret_cast<char*>(&tmp_meta_info),sizeof(MetaInfo),previous_offset);
				if(TFS_SUCCESS!=ret)
				{
					index_header()->index_file_offset_-=sizeof(MetaInfo);
					return ret;
				}
				//然后设置前一个节点tmp_meta_info的下一个偏移位置应该在current_offset
				tmp_meta_info.set_next_meta_offset_(current_offset);
				ret = mmap_file_op_->pwrite_file(reinterpret_cast<const char*>(&tmp_meta_info),sizeof(MetaInfo),previous_offset);
				if(TFS_SUCCESS!=ret)
				{
					index_header()->index_file_offset_-=sizeof(MetaInfo);
					return ret;
				}	
			}
			else	//当前一个节点不存在，说明哈希桶只有一个节点
			{
				bucket_slot()[slot] = current_offset;
			}
			return TFS_SUCCESS;
		}
		
		//从索引文件中读块信息
		int32_t IndexHandle::read_segment_meta(const uint64_t key, MetaInfo& meta)
		{
			int32_t current_offset = 0;
			int32_t previous_offset = 0;
			
			//1.确定key存放桶的位置
			//int32_t slot = static_cast<uint32_t>(key) % bucket_size();
			//2.利用哈希查找key
			int32_t ret = hash_find(key,current_offset,previous_offset);
			if(TFS_SUCCESS == ret)
			{
				ret = mmap_file_op_->pread_file(reinterpret_cast<char*>(&meta),sizeof(MetaInfo),current_offset);
				return ret;
			}
			else
			{
				return ret;
			}
		}

		//从索引文件中删除块信息
        int32_t IndexHandle::delete_segment_meta(const uint64_t key)
		{
			//1.首先看块到底在不在
			int32_t current_offset = 0;
			int32_t previous_offset = 0;
			//利用哈希查找key
			int32_t ret = hash_find(key,current_offset,previous_offset);
			
			if(ret != TFS_SUCCESS)
			{
				return ret;
			}
			
			MetaInfo meta_info;
			ret = mmap_file_op_->pread_file(reinterpret_cast<char*>(&meta_info),sizeof(MetaInfo),current_offset);
			if(TFS_SUCCESS != ret)
			{
				return ret;
			}
			
			//拿到下一个位置，让上一个节点指向下一个节点
			int32_t next_pos = meta_info.get_next_meta_offset_();
			//如果只有一个节点
			if(previous_offset == 0)
			{
				int32_t slot = static_cast<uint32_t>(key)%bucket_size();
				bucket_slot()[slot] = next_pos;	//直接更新槽位数据
			}
			//如果不止一个节点
			else
			{
				MetaInfo pre_meta_info;
				ret = mmap_file_op_->pread_file(reinterpret_cast<char*>(&pre_meta_info),sizeof(MetaInfo),previous_offset);
				if(TFS_SUCCESS!=ret)
				{
					return ret;
				}
				//设置下一个节点
				pre_meta_info.set_next_meta_offset_(next_pos);	//将前一个节点的数据覆盖，这个节点暂时成为僵尸节点
				
				ret = mmap_file_op_->pwrite_file(reinterpret_cast<char*>(&pre_meta_info),sizeof(MetaInfo),previous_offset);
				if(TFS_SUCCESS!=ret)
				{
					return ret;
				}	
			}
			//把删除节点加入可重用节点链表
			meta_info.set_next_meta_offset_(reuse_head_offset());
			ret = mmap_file_op_->pwrite_file(reinterpret_cast<const char*>(&meta_info),sizeof(MetaInfo),current_offset);
			if(TFS_SUCCESS!=ret)
			{
				return ret;
			}
			index_header()->reuse_head_offset_ = current_offset;
			if(debug) printf("delete_segment_meta -- reuse meta_info,current offset:%d\n",current_offset);
			//更新索引信息
			update_block_info(C_OP_DELETE,meta_info.get_size());
			return TFS_SUCCESS;
			
		}
		
		//更新块信息
		int IndexHandle::update_block_info(const OpType op_type,const uint32_t modify_size)
		{
			if(block_info()->block_id_ == 0)
			{
				return EXIT_BLOCKID_ZERO_ERROR;
			}
			
			if(op_type == C_OP_INSERT)
			{
				++block_info()->version_;
				++block_info()->file_count_;
				++block_info()->seq_no_;
				block_info()->size_ += modify_size;
			}
			else if(op_type == C_OP_DELETE)
			{
				++block_info()->version_;
				--block_info()->file_count_;
				++block_info()->seq_no_;
				block_info()->size_ -= modify_size;
				++block_info()->del_file_count_;
				block_info()->del_size_+=modify_size;		//已删除的文件大小
			}
			
			if(debug) printf("init index success, block id:%u,data file size:%d, index file size:%d ,bucket file size:%d , free head offset:%d,	seqno:%d, size:%d, file count:%d, del size:%d, del file count:%d, version:%d\n",
								block_info()->block_id_, index_header()->data_file_size_,
								index_header()->index_file_offset_,index_header()->bucket_size_,
								index_header()->reuse_head_offset_,block_info()->seq_no_,
								block_info()->size_,block_info()->file_count_,block_info()->del_size_,
								block_info()->del_file_count_,block_info()->version_);	
			
			return TFS_SUCCESS;
		}
		
	}
}
	