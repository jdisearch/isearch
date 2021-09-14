///////////////////////////////////////////////////////////
//                                                       //
//   package the binary posix semaphore which shared     //
//   between threads, not support process communcation   //
///////////////////////////////////////////////////////////

#ifndef __SEM_H__
#define __SEM_H__

#include <time.h>
#include <semaphore.h>

//#include <stdint.h>

class Sem
{
private:
	sem_t	mOriSem;

public:
	Sem()
	{
		sem_init(&mOriSem, 0, 0);	
	}

	inline int semWait()
	{
		return sem_wait(&mOriSem);	
	}

	inline int semTryWait()
	{
		return sem_trywait(&mOriSem);
	}

	inline int semTimeWait(const uint64_t& micSeconds)
	{
		struct timespec timer;
    clock_gettime(CLOCK_REALTIME, &timer);
    
    uint64_t expiredSec = (micSeconds / 1000) + (timer.tv_nsec + (micSeconds % 1000) * 1000 * 1000) / (1000 * 1000 * 1000);
		timer.tv_sec += expiredSec;
		timer.tv_nsec = (timer.tv_nsec + (micSeconds % 1000) * 1000 * 1000) % (1000 * 1000 * 1000);

		return sem_timedwait(&mOriSem, &timer);
	}

	inline int semPost()
	{
		return sem_post(&mOriSem);
	}
};

#endif //__SEM_H__

