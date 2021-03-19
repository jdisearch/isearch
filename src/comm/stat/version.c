#include <stdlib.h>
#include <time.h>
#include "version.h"

const char compdatestr[] = __DATE__;
const char comptimestr[] = __TIME__;
const char version[]     = TTC_VERSION;
const char version_detail[]     = TTC_VERSION_DETAIL;
long compile_time;

	
__attribute__((constructor))
void init_comptime(void)
{
	struct tm tm;
	switch(compdatestr[0])
	{
	case 'A': tm.tm_mon = compdatestr[1]=='p' ? 3 /*Apr*/: 7/*Aug*/; break;
	case 'D': tm.tm_mon=11; break; //Dec
	case 'F': tm.tm_mon=1;  break;//Feb
	case 'J': tm.tm_mon = compdatestr[1]=='a'? 0 /*Jan*/ : compdatestr[2]=='n' ? 5/*Jun*/ : 6 /*Jul*/; break;
	case 'M': tm.tm_mon = compdatestr[2]=='r'?2/*Mar*/:4/*May*/; break;
	case 'N': tm.tm_mon=10;/*Nov*/ break;
	case 'S': tm.tm_mon=8;/*Sep*/ break;
	case 'O': tm.tm_mon=9;/*Oct*/ break;
	default: return;
	}

	tm.tm_mday = strtol(compdatestr+4, 0, 10);
	tm.tm_year = strtol(compdatestr+7, 0, 10) - 1900;
	tm.tm_hour = strtol(comptimestr+0, 0, 10);
	tm.tm_min = strtol(comptimestr+3, 0, 10);
	tm.tm_sec = strtol(comptimestr+6, 0, 10);
	compile_time = mktime(&tm);
}

