/* $Id: noncopyable.h 602 2009-01-08 02:27:44Z jackda $ */
#ifndef __TTC_NONCOPY_H__
#define __TTC_NONCOPY_H__

#include "namespace.h"

TTC_BEGIN_NAMESPACE

class noncopyable
{
    protected:
        noncopyable(void){}
        ~noncopyable(void){} 
    private:
        noncopyable(const noncopyable&);
        const noncopyable& operator= (const noncopyable&);
};

TTC_END_NAMESPACE

#endif
