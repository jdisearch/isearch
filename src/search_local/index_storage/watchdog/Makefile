include ../Make.conf

target = libwatchdog.a

CFLAGS+=-I../common -I../stat
VPATH := ../stat
filelist := watchdog watchdog_daemon watchdog_fork watchdog_helper watchdog_listener watchdog_main watchdog_run watchdog_stattool watchdog_unit watchdog_logger fault gdb gdb_srv stat_alarm_reporter
libwatchdog_objs := $(filelist:%=%.o)

install_dir = ../../bin

include ../Make.rules

