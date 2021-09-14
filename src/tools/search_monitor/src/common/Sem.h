///////////////////////////////////////////////////////////
//                                                       //
//   package the binary posix semaphore which shared     //
//   between threads, not support process communcation   //
///////////////////////////////////////////////////////////

#ifndef __SEM_H_
#define __SEM_H_

#include "AutoPtr.h"

#include <semaphore.h>
#include <stdint.h>

class Sem : public HandleBase
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
		timer.tv_sec = micSeconds / 1000;
		timer.tv_nsec = (micSeconds - (micSeconds / 1000) * 1000) * 1000 * 1000;

		return sem_timedwait(&mOriSem, &timer);
	}

	inline int semPost()
	{
		return sem_post(&mOriSem);
	}
};

typedef AutoPtr<Sem>  SemPtr;

#endif //__SEM_H__

