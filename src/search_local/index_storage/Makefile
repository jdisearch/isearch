include Make.conf

SUBDIRS := common api stat watchdog cache rocksdb_helper

make_all_stat:: make_all_common

make_all_cache:: make_all_common make_all_api make_all_stat make_all_watchdog

make_all_proxy:: make_all_common make_all_stat

include Make.rules

