/*
 * =====================================================================================
 *
 *       Filename:  hb_log.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Norton, yangshuang68@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include "hb_log.h"
#include "global.h"
#include "admin_tdef.h"

HBLog::HBLog(DTCTableDefinition *tbl) : _tabledef(tbl),
										_log_writer(0),
										_log_reader(0)
{
}

HBLog::~HBLog()
{
	DELETE(_log_writer);
	DELETE(_log_reader);
}

int HBLog::Init(const char *path, const char *prefix, uint64_t total, off_t max_size)
{
	_log_writer = new BinlogWriter;
	_log_reader = new BinlogReader;

	if (_log_writer->Init(path, prefix, total, max_size))
	{
		log_error("init log_writer failed");
		return -1;
	}

	if (_log_reader->Init(path, prefix))
	{
		log_error("init log_reader failed");
		return -2;
	}

	return 0;
}

int HBLog::write_update_log(TaskRequest &task)
{
	RawData *raw_data;
	NEW(RawData(&g_stSysMalloc, 1), raw_data);

	if (!raw_data)
	{
		log_error("raw_data is null");
		return -1;
	}

	HotBackTask &hotbacktask = task.get_hot_back_task();
	int type = hotbacktask.get_type();
	if (raw_data->Init(0, _tabledef->key_size(), (const char *)&type, 0, -1, -1, 0))
	{
		DELETE(raw_data);
		return -1;
	}
	DTCValue key;
	DTCValue value;
	if (0 == hotbacktask.get_packed_key_len())
	{
		log_error("packedkey len is  zero");
		return -1;
	}
	else
	{
		key.Set(hotbacktask.get_packed_key(), hotbacktask.get_packed_key_len());
	}

	if (0 == hotbacktask.get_value_len())
	{
		value.Set(0);
	}
	else
	{
		value.Set(hotbacktask.get_value(), hotbacktask.get_value_len());
	}

	RowValue row(_tabledef);
	row[0].u64 = type;
	row[1].u64 = hotbacktask.get_flag();
	row[2] = key;
	row[3] = value;
	log_debug(" tye is %d, flag %d", type, hotbacktask.get_flag());
	raw_data->insert_row(row, false, false);
	_log_writer->insert_header(type, 0, 1);
	_log_writer->append_body(raw_data->get_addr(), raw_data->data_size());
	DELETE(raw_data);

	log_debug(" packed key len:%d,key len:%d, key :%s", key.bin.len, *(unsigned char *)key.bin.ptr, key.bin.ptr + 1);
	return _log_writer->Commit();
}

int HBLog::write_lru_hb_log(TaskRequest &task)
{
	RawData *raw_data;
	NEW(RawData(&g_stSysMalloc, 1), raw_data);

	if (!raw_data)
	{
		log_error("raw_data is null");
		return -1;
	}

	HotBackTask &hotbacktask = task.get_hot_back_task();
	int type = hotbacktask.get_type();
	if (raw_data->Init(0, _tabledef->key_size(), (const char *)&type, 0, -1, -1, 0))
	{
		DELETE(raw_data);
		return -1;
	}
	DTCValue key;
	if (0 == hotbacktask.get_packed_key_len())
	{
		log_error("packedkey len is  zero");
		return -1;
	}
	else
	{
		key.Set(hotbacktask.get_packed_key(), hotbacktask.get_packed_key_len());
	}

	RowValue row(_tabledef);
	row[0].u64 = type;
	row[1].u64 = hotbacktask.get_flag();
	row[2] = key;
	row[3].Set(0);
	log_debug(" type is %d, flag %d", type, hotbacktask.get_flag());
	raw_data->insert_row(row, false, false);
	_log_writer->insert_header(BINLOG_LRU, 0, 1);
	_log_writer->append_body(raw_data->get_addr(), raw_data->data_size());
	DELETE(raw_data);

	log_debug(" write lru hotback log, packed key len:%d,key len:%d, key :%s", key.bin.len, *(unsigned char *)key.bin.ptr, key.bin.ptr + 1);
	return _log_writer->Commit();
}

int HBLog::Seek(const JournalID &v)
{
	return _log_reader->Seek(v);
}

/* 批量拉取更新key，返回更新key的个数 */
int HBLog::task_append_all_rows(TaskRequest &task, int limit)
{
	int count;
	for (count = 0; count < limit; ++count)
	{
		/* 没有待处理日志 */
		if (_log_reader->Read())
			break;

		RawData *raw_data;

		NEW(RawData(&g_stSysMalloc, 0), raw_data);

		if (!raw_data)
		{
			log_error("allocate rawdata mem failed");
			return -1;
		}

		if (raw_data->check_size(g_stSysMalloc.Handle(_log_reader->record_pointer()),
								0,
								_tabledef->key_size(),
								_log_reader->record_length(0)) < 0)
		{
			log_error("raw data broken: wrong size");
			DELETE(raw_data);
			return -1;
		}

		/* attach raw data read from one binlog */
		if (raw_data->Attach(g_stSysMalloc.Handle(_log_reader->record_pointer()), 0, _tabledef->key_size()))
		{
			log_error("attach rawdata mem failed");

			DELETE(raw_data);
			return -1;
		}

		RowValue r(_tabledef);
		r[0].u64 = *(unsigned *)raw_data->Key();

		unsigned char flag = 0;
		while (raw_data->decode_row(r, flag) == 0)
		{

			log_debug("type: " UINT64FMT ", flag: " UINT64FMT ", key:%s, value :%s",
					  r[0].u64, r[1].u64, r[2].bin.ptr, r[3].bin.ptr);
			log_debug("binlog-type: %d", _log_reader->binlog_type());

			task.append_row(&r);
		}

		DELETE(raw_data);
	}

	return count;
}

JournalID HBLog::get_reader_jid(void)
{
	return _log_reader->query_id();
}

JournalID HBLog::get_writer_jid(void)
{
	return _log_writer->query_id();
}
