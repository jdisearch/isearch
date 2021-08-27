#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "comm.h"

extern "C" {
#include "proc_title.h"
}

#include "sync_state_manager.h"

int main(int argc, char **argv)
{
	init_proc_title(argc, argv);
	CComm::parse_argv(argc, argv);

	log_info("Hello hot_backup.");
	SyncStateManager* pStateManager = new SyncStateManager();
	pStateManager->Start();

	return 0;
}
