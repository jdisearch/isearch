/*
 * =====================================================================================
 *
 *       Filename:  fault.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  linjinming, linjinming@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#include <signal.h>

#define ENV_FAULT_LOGGER_PID "__FAULT_LOGGER_PID"
class FaultHandler
{
private:
	static void Handler(int, siginfo_t *, void *);
	static FaultHandler Instance;
	static int DogPid;
	FaultHandler(void);

public:
	static void Initialize(int);
};
