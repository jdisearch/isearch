/*
 * =====================================================================================
 *
 *       Filename:  task_server.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

#include "task_base.h"
#include "decode.h"
#include "protocol.h"
#include "buffer_error.h"
#include "log.h"

int DTCTask::append_result(ResultSet *rs)
{
	if (rs == NULL)
	{
		set_total_rows(0);
		return 0;
	}
	rs->rewind();
	if (all_rows() && (count_only() || !in_range(rs->total_rows(), 0)))
	{
		set_total_rows(rs->total_rows());
	}
	else
	{
		for (int i = 0; i < rs->total_rows(); i++)
		{
			const RowValue *r = rs->fetch_row();
			if (r == NULL)
				return rs->error_num();
			if (compare_row(*r))
			{
				int rv = resultWriter->append_row(*r);
				if (rv < 0)
					return rv;
				if (all_rows() && result_full())
				{
					set_total_rows(rs->total_rows());
					break;
				}
			}
		}
	}
	return 0;
}

/* all use prepareResult interface */
int DTCTask::pass_all_result(ResultSet *rs)
{
	prepare_result(0, 0);

	rs->rewind();
	if (count_only())
	{
		set_total_rows(rs->total_rows());
	}
	else
	{
		for (int i = 0; i < rs->total_rows(); i++)
		{
			int rv;
			const RowValue *r = rs->fetch_row();
			if (r == NULL)
			{
				rv = rs->error_num();
				set_error(rv, "fetch_row()", NULL);
				return rv;
			}
			rv = resultWriter->append_row(*r);
			if (rv < 0)
			{
				set_error(rv, "append_row()", NULL);
				return rv;
			}
		}
	}
	resultWriter->set_total_rows((unsigned int)(resultInfo.total_rows()));

	return 0;
}

int DTCTask::merge_result(const DTCTask &task)
{
	/*首先根据子包击中情况统计父包的名种情况*/
	uint32_t uChildTaskHitFlag = task.resultInfo.hit_flag();
	if (HIT_SUCCESS == uChildTaskHitFlag)
	{
		resultInfo.incr_tech_hit_num();
		ResultPacket *pResultPacket = task.result_code() >= 0 ? task.get_result_packet() : NULL;
		if (pResultPacket && (pResultPacket->numRows || pResultPacket->totalRows))
		{
			resultInfo.incr_bussiness_hit_num();
		}
	}

	int ret = task.resultInfo.affected_rows();

	if (ret > 0)
	{
		resultInfo.set_affected_rows(ret + resultInfo.affected_rows());
	}

	if (task.resultWriter == NULL)
		return 0;

	if (resultWriter == NULL)
	{
		if ((ret = prepare_result_no_limit()) != 0)
		{
			return ret;
		}
	}
	return resultWriter->merge_no_limit(task.resultWriter);
}

/* the only entry to create resultWriter */
int DTCTask::prepare_result(int st, int ct)
{
	int err;

	if (resultWriterReseted && resultWriter)
		return 0;

	if (resultWriter)
	{
		err = resultWriter->Set(fieldList, st, ct);
		if (err < 0)
		{
			set_error(-EC_TASKPOOL, "ResultPacket()", NULL);
			return err;
		}
	}
	else
	{
		try
		{
			resultWriter = new ResultPacket(fieldList, st, ct);
		}
		catch (int err)
		{
			set_error(err, "ResultPacket()", NULL);
			return err;
		}
	}

	resultWriterReseted = 1;

	return 0;
}
