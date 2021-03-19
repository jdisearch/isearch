/*
 * =====================================================================================
 *
 *       Filename:  watchdog_run.cc
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <signal.h>

#include "watchdog.h"
#include "watchdog_main.h"
#include "watchdog_stattool.h"
#include "watchdog_listener.h"
#include "watchdog_helper.h"
#include "watchdog_logger.h"
#include "dbconfig.h"
#include "log.h"
#include "daemon.h"

int start_watch_dog(int (*entry)(void *), void *args)
{
    int delay = 5;
    dbConfig->set_helper_path(getpid());
    WatchDog *wdog = NULL;
    WatchDogListener *srv = NULL;

    if (gConfig->get_int_val("cache", "DisableWatchDog", 0) == 0)
    {
        signal(SIGCHLD, SIG_DFL);

        delay = gConfig->get_int_val("cache", "WatchDogTime", 30);
        if (delay < 5)
        {
            delay = 5;
        }

        wdog = new WatchDog;
        srv = new WatchDogListener(wdog, delay);
        srv->attach_watch_dog();

        start_fault_logger(wdog);

        if (gConfig->get_int_val("cache", "StartStatReporter", 0) > 0)
        {
            WatchDogStatTool *st = new WatchDogStatTool(wdog, delay);
            if (st->Fork() < 0)
                return -1; // cann't fork reporter
            log_info("fork stat reporter");
        }
    }

    if (gConfig->get_int_val("cache", "DisableDataSource", 0) == 0)
    {
        int nh = 0;
        /* starting master helper */
        for (int g = 0; g < dbConfig->machineCnt; g++)
        {
            for (int r = 0; r < ROLES_PER_MACHINE; r++)
            {
                HELPERTYPE t = dbConfig->mach[g].helperType;

                log_debug("helper type = %d", t);

                //check helper type is dtc
                if (DTC_HELPER >= t)
                {
                    break;
                }

                int i, n = 0;
                for (i = 0; i < GROUPS_PER_ROLE && (r * GROUPS_PER_ROLE + i) < GROUPS_PER_MACHINE; i++)
                {
                    n += dbConfig->mach[g].gprocs[r * GROUPS_PER_ROLE + i];
                }

                if (n <= 0)
                {
                    continue;
                }

                WatchDogHelper *h = NULL;
                NEW(WatchDogHelper(wdog, delay, dbConfig->mach[g].role[r].path, g, r, n + 1, t), h);
                if (NULL == h)
                {
                    log_error("create WatchDogHelper object failed, msg:%m");
                    return -1;
                }

                if (h->Fork() < 0 || h->Verify() < 0)
                {
                    return -1;
                }

                nh++;
            }
        }

        log_info("fork %d helper groups", nh);
    }

    if (wdog)
    {
        const int recovery = gConfig->get_idx_val("cache", "ServerRecovery",
                                                ((const char *const[]){
                                                    "none",
                                                    "crash",
                                                    "crashdebug",
                                                    "killed",
                                                    "error",
                                                    "always",
                                                    NULL}),
                                                2);

        WatchDogEntry *dtc = new WatchDogEntry(wdog, entry, args, recovery);

        if (dtc->Fork() < 0)
            return -1;

        wdog->run_loop();
        exit(0);
    }

    return 0;
}
