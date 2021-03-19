LIB_PATH = ../../..
include ../Make.conf
VPATH :=../stat 

####################compile#################
CFLAGS += -D_GLIBCXX_USE_CXX11_ABI=0
CFLAGS += -I./ -I../common -I../api/c_api -I../stat -I../watchdog -I../helper 
LIBPATH := -L. -L../common -L../watchdog -L../stat -L../../../3rdlib/attr_api -L../api
DB%.o tmp.DB%.o:CFLAGS += $(MYSQLINC)
ifneq ($(findstring x86_64,$(PLATFORM)),)
BITS=64
else
BITS=32
endif

LIBDTCAPI := -L../api  -lpthread -Wl,-rpath,\$$ORIGIN/../lib/ -Wl,-rpath,\$$ORIGIN  -Wl,-rpath,\$$ORIGIN/../api/  -Wl,-rpath,\$$ORIGIN/../ -z origin

target = libdtcd.a dtcd
target_external = ../api/libdtc.a ../stat/libstat.a ../common/libcommon.a

$(filterout libdtcd.a,$(target)): libdtcd.a;

filelist := feature hash ng_info node_set node_index fence_unit buffer_bypass buffer_pool pt_malloc sys_malloc raw_data raw_data_process buffer_process buffer_flush buffer_unit empty_filter black_hole  logger task_pendlist lru_bit hb_log admin_process hb_feature container_dtcd col_expand t_tree tree_data tree_data_process expire_time main hb_process task_control
libdtcd_objs:= $(sort $(filelist:%=%.o))

#dtcd
dtcd: CFLAGS += -export-dynamic
dtcd: LDFLAGS += -Wl,--version-script,dtcd.export.lst
dtcd_objs:= main.o task_control.o stat_client.o expire_time.o hb_process.o 
dtcd_libs:= -lstat -lwatchdog  -ldtcd -lcommon -lattr_api_$(BITS) -lpthread -ldl $(Z_LIB) -rdynamic

#####################install############
target_install = dtcd
install_dir = ../../bin

include ../Make.rules
