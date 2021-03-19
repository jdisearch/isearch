#ifndef __UNIX_LOCK_H
#define __UNIX_LOCK_H

#include <sys/cdefs.h>

__BEGIN_DECLS
extern int UnixSocketLock(const char *fmt,...);
__END_DECLS

#endif
