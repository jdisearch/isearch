#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#include <getopt.h>
#include "comm.h"
#include "dtcapi.h"
#include "config.h"
#include "log.h"
#include "global.h"
#include "async_file.h"
#include "config_center_parser.h"
#include <string>

DTC::Server CComm::master;
DTC::Server CComm::slave;
CConfig CComm::config;
CRegistor CComm::registor;

const char *CComm::version = "0.2";
char *CComm::config_file = NULL;
int CComm::backend = 1;
int CComm::fixed = 0;
int CComm::purge = 0;
int CComm::normal = 1;
int CComm::skipfull = 0;

void CComm::show_usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s [OPTION]...\n"
		"Sync DTC/Bitmap master data to DTC/bitmap slave.\n\n" 
		"\t -n, --normal   normal running mode.\n"
		"\t -s, --skipfull  running inc mode without full sync.\n"
		"\t -f, --fixed    when something wrong, use this argument to fix error.\n"
		"\t -p, --purge    when switch slave, use this argument to purge dirty data.\n"
		"\t -b, --backend  runing in background.\n"
		"\t -v, --version  show version.\n"
		"\t -c, --config_file enter config file path.\n"
		"\t -h, --help     display this help and exit.\n\n", argv[0]);
	return;
}

void CComm::show_version(int argc, char **argv)
{
	fprintf(stderr, "%s(%s)\n", argv[0], version);
	return;
}

void CComm::parse_argv(int argc, char **argv)
{
	if (argc < 2) {
		show_usage(argc, argv);
		exit(-1);
	}

	int option_index = 0, c = 0;
	static struct option long_options[] = {
		{"normal", 0, 0, 'n'},
		{"skipfull", 0, 0, 's'},
		{"fixed", 0, 0, 'f'},
		{"purge", 0, 0, 'p'},
		{"backend", 0, 0, 'b'},
		{"version", 0, 0, 'v'},
		{"help", 0, 0, 'h'},
		{"config_file", 1, 0, 'c'},
		{0, 0, 0, 0},
	};

	while ((c =
		getopt_long(argc, argv, "nfpbvhsc:", long_options,
			    &option_index)) != -1) {
		switch (c) {
		case 'n':
			normal = 1;
			break;

		case 's':
			skipfull = 1;
			break;

		case 'f':
			fixed = 1;
			break;

		case 'p':
			purge = 1;
			break;

		case 'b':
			backend = 1;
			break;;

		case 'v':
			show_version(argc, argv);
			exit(0);
			break;

		case 'h':
			show_usage(argc, argv);
			exit(0);
			break;
		case 'c':
			if (optarg)
				{
					config_file = optarg;
				}
			break;
		case '?':
		default:
			show_usage(argc, argv);
			exit(-1);
		}
	}

	if (optind < argc) {
		show_usage(argc, argv);
		exit(-1);
	}

	if (fixed && purge) {
		show_usage(argc, argv);
		exit(-1);
	}

	return;
}

int CComm::ReInitDtcAgency(ParserBase* pParser)
{
	if (CComm::connect_ttc_server(1, pParser)) {
		return -1;
	}
	CComm::registor.SetMasterServer(&CComm::master);
	return CComm::log_server_info(pParser);
}

int CComm::connect_ttc_server(int ping_master, ParserBase* pParser)
{
	log_notice("try to ping master/slave server");

	int ret = 0;
	std::string master_addr = pParser->GetPeerShardMasterIp();
	std::string slave_addr = pParser->GetLocalDtcNet();
    log_debug("Master:%s.", master_addr.c_str());
	log_debug("slave:%s.", slave_addr.c_str());

	slave.IntKey();
	slave.SetTableName("*");
	ret = slave.SetAddress(slave_addr.c_str());
	slave.SetTimeout(30);

	if (-DTC::EC_BAD_HOST_STRING == ret
		|| ((ret = slave.Ping()) != 0 
		&& ret != -DTC::EC_TABLE_MISMATCH)) {
		log_error("ping slave[%s] failed, err: %d", slave_addr.c_str(), ret);
		return -1;
	}

	log_notice("ping slave[%s] success", slave_addr.c_str());

	if(ping_master) {
		ret =  master.SetAddress(master_addr.c_str());
		master.SetTimeout(30);
		master.CloneTabDef(slave);	/* 从备机复制表结构 */
		master.SetAutoUpdateTab(false);
		if (-DTC::EC_BAD_HOST_STRING == ret
			|| (ret = master.Ping()) != 0) {
			log_error("ping master[%s] failed, err:%d", master_addr.c_str(), ret);
			return -1;
		}

		log_notice("ping master[%s] success", master_addr.c_str());
	}

	return 0;
}

int CComm::load_config(const char *p)
{
	const char *f = strdup(p);

	if (config.ParseConfig(f, "SYSTEM")) {
		fprintf(stderr, "parse config %s failed\n", f);
		return -1;
	}

	return 0;
}

int CComm::check_hb_status()
{
	CAsyncFileChecker checker;

	if (checker.Check()) {
		log_error
		    ("check hb status, __NOT__ pass! errmsg: %s, try use --fixed parament to start",
		     checker.ErrorMessage());
		return -1;
	}

	log_notice("check hb status, passed");

	return 0;
}

int CComm::fixed_hb_env()
{
	/* FIXME: 简单删除，后续再考虑如何恢复 */
	if (system("cd ../bin/ && ./hb_fixed_env.sh hbp")) {
		log_error("invoke hb_fixed_env.sh hbp failed, %m");
		return -1;
	}

	log_notice("fixed hb env, passed");

	return 0;
}

int CComm::fixed_slave_env()
{
	if (system("cd ../bin/ && ./hb_fixed_env.sh slave")) {
		log_error("invoke hb_fixed_env.sh slave failed, %m");
		return -1;
	}

	log_notice("fixed slave env, passed");

	return 0;
}

/* 确保hbp唯一, 锁住hbp的控制文件目录 */
int CComm::uniq_lock(const char *p)
{
	if (access(p, F_OK | X_OK))
		mkdir(p, 0777);

	int fd = open(p, O_RDONLY);
	if (fd < 0)
		return -1;

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	return flock(fd, LOCK_EX | LOCK_NB);
}

int CComm::log_server_info(ParserBase* pParser , const char *p)
{
	if(access(p, F_OK|X_OK))
		mkdir(p, 0777);

	char info_name[256];
	snprintf(info_name, sizeof(info_name), "%s/%s", p, "server_address");

	FILE *fp = fopen(info_name, "w");
	if(!fp) {
		log_notice("log server info failed, %m");
		return -1;
	}

	std::string master_addr = pParser->GetPeerShardMasterIp();
	std::string slave_addr = pParser->GetLocalDtcNet();

	//int n;
	fwrite(master_addr.c_str(), strlen(master_addr.c_str()), 1, fp);
	fwrite("\n", 1, 1, fp);

	fwrite(slave_addr.c_str(), strlen(slave_addr.c_str()), 1, fp);
	fwrite("\n", 1, 1, fp);

	fclose(fp);
	return 0;
}

