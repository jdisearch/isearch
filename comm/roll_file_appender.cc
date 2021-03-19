#include "roll_file_appender.h"
#include "log.h"
#include <string>
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 

const std::string ump_log_dir = "/export/home/tomcat/UMP-Monitor/logs";

namespace common {
	RollingFileAppender::RollingFileAppender() {
		_MaxBackupIndex = 3;
		_MaxFileSize = 50 * 1024 * 1024;
		_CurrentFileSize = 0;
	}

	bool RollingFileAppender::Initialize() {

		if (!FileExists(ump_log_dir)) {
			if (mkdir(ump_log_dir.c_str(), 0777) != 0) {
				log_error("can not mkdir %s ", ump_log_dir.c_str());
				return false;
			}
		}

		
		time_t now_time = time(NULL);
		struct tm* p = localtime(&now_time);
		char time_str[64];
		strftime(time_str, 64, "%Y%m%d%H%M%S", p);

		std::stringstream filename_ss;
		filename_ss << ump_log_dir << "/" << time_str << "000_" << getpid() << "_" << rand() % 100000 << "_tp.log";
		_FileName = filename_ss.str();

		_Fd = open(_FileName.c_str(), O_RDWR | O_APPEND | O_CREAT);
		if (_Fd < 0) {
			log_error("can not open file %s", _FileName.c_str());
			return false;
		}
		
		fcntl(_Fd, F_SETFD, FD_CLOEXEC);
		chmod(_FileName.c_str(), 0777);
		return true;
	}

	void RollingFileAppender::DoAppend(const std::string& report_item) {


		int l = write(_Fd, report_item.c_str(), report_item.length());
		if (l < 0) {
			return;
		}

		_CurrentFileSize += l;
		if (_CurrentFileSize >= _MaxFileSize) {
			fsync(_Fd);
			RollOver();
		}

	}

	void RollingFileAppender::RollOver() {

		for (int index = _MaxBackupIndex; index > 1; index--) {
			std::stringstream orignal_ss;
			std::stringstream rename_ss;
			orignal_ss << _FileName << "." << index - 1;
			if (FileExists(orignal_ss.str())) {
				rename_ss << _FileName << "." << index;
				if (rename(orignal_ss.str().c_str(), rename_ss.str().c_str()) == 0) {
					continue;
				}
				else {
					break;
				}
			}
		}

		if (_Fd > 0) {
			close(_Fd);
		}

		std::stringstream rename_ss;
		rename_ss << _FileName << "." << 1;
		rename(_FileName.c_str(), rename_ss.str().c_str());
		_Fd = open(_FileName.c_str(), O_RDWR | O_APPEND | O_CREAT);
		if (_Fd < 0) {
			log_error("can not open file %s", _FileName.c_str());
			return;
		}

		chmod(_FileName.c_str(), 0777);
		_CurrentFileSize = 0;
	}

	bool RollingFileAppender::FileExists(const std::string &filename)
	{
		return access(filename.c_str(), R_OK | W_OK) == 0;
	}


};
