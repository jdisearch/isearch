/*
 * =====================================================================================
 *
 *       Filename:  cachelist_unit.h
 *
 *    Description:  cachelist uint class definition.
 *
 *        Version:  1.0
 *        Created:  28/05/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */


#ifndef __CACHELIST_UNIT_H
#define __CACHELIST_UNIT_H

#include "namespace.h"
#include "timerlist.h"
#include "cachelist.h"
#include "log.h"

TTC_BEGIN_NAMESPACE

class CTimerObject;
class CCacheListUnit : public CCacheList, private CTimerObject
{
	public:
		CCacheListUnit(CTimerList *timer);
		virtual ~CCacheListUnit(void);

		int init_list(const unsigned max, const unsigned type, const unsigned expired=10*60/*10 mins*/)
		{
			return CCacheList::init_list(max, type, expired);
		}

		int add_list(const char *key, const char *value, const unsigned ksize, const unsigned vsize)
		{
			int ret = CCacheList::add_list(key, value, ksize, vsize);
			return ret;
		}

		int try_expired_list(void)
		{
			int ret = CCacheList::try_expired_list();

			return ret;
		}

		void start_list_expired_task(void)
		{
			log_info("start list-expired task");
			
			AttachTimer(timer);

			return;
		}

	public:
		virtual void TimerNotify(void);

	private:
		CTimerList * timer;
};

TTC_END_NAMESPACE

#endif
