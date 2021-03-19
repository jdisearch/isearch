LIB_PATH = ../../../..

include ../../Make.conf

SVN_REVISION = $(shell test -d .svn && (svn info | grep "Last Changed Rev" | cut -d " " -f 4))
ifeq "$(SVN_REVISION)a" "a"
	SVN_REVISION = "(unknown)"
endif

VPATH = ../../common
################compile#############
target = libdtc.a container_api.pic.o version.pic.o somain.pic.o libdtc.so libdtc.pic.a 

# CLIENTAPI macro use for scope test only
CFLAGS += -DCLIENTAPI
CFLAGS += -pthread -I../../common -I../../stat $(ZINC) 
LIBS = $(Z_LIB) -ldl -lpthread


filelist := dtc_req dtc_srv dtc_pool dtc_wrap dtc_wrapp \
	    poller timer_list key_list table_def \
	    log_client mem_check md5 value \
	    section decode encode field_api \
	    packet_base packet_client \
	    task_base task_const net_addr udp_pool compress buffer thread

# for auto ln -sf
libdtc_objs := $(patsubst %,%.o,$(filelist))
libdtc.so: LDFLAGS += -Wl,--version-script,dtc_api.lst -e _so_start container_api.pic.o version.pic.o somain.pic.o
libdtc_filename := libdtc-gcc-$(GCCVER)-r$(SVN_REVISION).so

###############install##############
target_install = libdtc.a libdtc.pic.a libdtc.so dtc_api.h 
install_dir = ../../../bin
%.a: install_dir = ../../../lib
%.h: install_dir = ../../../include

include ../../Make.rules

