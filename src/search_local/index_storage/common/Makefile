include ../Make.conf

target = libcommon.a libcommon_nopic.a
CFLAGS+=-I../api/c_api -I. -I../watchdog -I../stat -I../../../3rdlib/attr_api $(ZINC)
LIBS = $(Z_LIB)
VPATH = ../watchdog
#SVN_VERSION = $(shell test -d .svn && (svn info ../ | grep "Last Changed Rev" | cut -d " " -f 4))
GIT_VERSION=$(shell git log | head -n 4 | grep "commit" | cut -d " " -f 2 | cut -c 1-7)
ifeq "$(GIT_VERSION)a" "a"
	GIT_REVISION = "(unknown)"
endif
$(shell sed -i '11s/"[^"]*"/"$(GIT_VERSION)"/' version.h)

filelist := md5 value decode encode field section table_def \
	packet_base packet_server packet_result \
	task_const task_base task_pkey task_server task_copy task_request task_api \
	poller timer_list config version system_lock log log_stat log_alert \
	shmem mem_check dbconfig dbconfig_tdef admin_tdef \
	daemon daemon_wait proc_title thread poll_thread poll_thread_group thread_cpu_stat \
	buffer localip \
	key_guard bitsop net_addr net_bind blacklist blacklist_unit \
	multi_request task_multiplexer \
	helper_client helper_collect helper_group \
	listener listener_pool decoder_base \
	client_sync client_async client_unit client_dgram  \
	plugin_request plugin_mgr plugin_dgram plugin_sync \
	plugin_worker plugin_unit plugin_listener_pool \
	plugin_global plugin_decoder plugin_timer dtc_global\
	compress client_resource_pool \
	new_hash dynamic_helper_collection consistent_hash_selector chash\
	file_backed_key_set markup_stl parse_cluster_config key_route \
	proxy_client proxy_client_unit proxy_listener proxy_listen_pool proxy_process proxy_receiver proxy_sender \
	proxy_multi_request plugin_proxy_mgr plugin_proxy_listener_pool watchdog_fork hotback_task table_def_manager

libcommon_objs := $(filelist:%=%_pic.o)
%_pic.o: CFLAGS  += -pthread -fPIC

libcommon_nopic_objs := $(filelist:%=%.o)
%.o: CFLAGS  += -pthread 

include ../Make.rules

