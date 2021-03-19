#include  <cachelist_unit.h>

TTC_USING_NAMESPACE

CCacheListUnit::CCacheListUnit(CTimerList * t) :
	timer(t) 
{
}

CCacheListUnit::~CCacheListUnit()
{
}

void CCacheListUnit::TimerNotify(void)
{
	log_debug("sched cachelist-expired task");

	/* expire all expired eslot */
	try_expired_list();

	CCacheList::dump_all_blslot();
	
	/* set timer agagin */
	AttachTimer(timer);
	
	return;
}
