/*
 * =====================================================================================
 *
 *       Filename:  roll_file_appender.h
 *
 *    Description:  roll_file_appender class definition.
 *
 *        Version:  1.0
 *        Created:  09/08/2018
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhulin, shzhulin3@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */


#ifndef __ROLL_FILE_APPENDER_H__
#define __ROLL_FILE_APPENDER_H__

#include <queue>
#include <string>
#include <unistd.h>

namespace common {

	class RollingFileAppender {
	public:
		RollingFileAppender();
		bool Initialize();
		void DoAppend(const std::string& report_item);
		~RollingFileAppender() {
			if (_Fd > 0)
				close(_Fd);
		}
	private:
		int _MaxFileSize;
		int _MaxBackupIndex;
		std::string _FileName;
		int _Fd;
		int _CurrentFileSize;

	private:
		void RollOver();
		inline bool FileExists(const std::string& filename);

	};
};



#endif
