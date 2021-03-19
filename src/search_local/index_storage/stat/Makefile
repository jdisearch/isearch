include ../Make.conf

#################compile ############
target = libstat.a stattool

CFLAGS+=-I../common -I../../../3rdlib/attr_api
ifneq ($(findstring x86_64,$(PLATFORM)),)
BITS=64
else
BITS=32
endif

filelist := stat_manager stat_dtc stat_thread stat_dtc_def stat_client stat_attn formater stat_manager_container_thread stat_alarm_reporter stat_tool
libstat_objs := $(filelist:%=%.o)
libstat_objs: CFLAGS += -pthread


stattool_src := stat_tool formater stat_manager stat_attn stat_client stat_dtc_def stat_alarm_reporter
stattool_objs := $(stattool_src:%=%.o) ../../../3rdlib/attr_api/libattr_api_$(BITS).a
stattool_libs := -L../common -lcommon -ldl -lpthread

##################install #############
target_install = stattool
install_dir = ../../bin

include ../Make.rules

