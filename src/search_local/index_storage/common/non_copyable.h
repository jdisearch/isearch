/*
 * =====================================================================================
 *
 *       Filename:  non_copyable.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/08/2020 10:02:05 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  qiulu, choulu@jd.com
 *        Company:  JD.com, Inc.
 *
 * =====================================================================================
 */
#ifndef __DTC_NONCOPY_H__
#define __DTC_NONCOPY_H__

#include "namespace.h"

DTC_BEGIN_NAMESPACE

class noncopyable
{
protected:
    noncopyable(void) {}
    ~noncopyable(void) {}

private:
    noncopyable(const noncopyable &);
    const noncopyable &operator=(const noncopyable &);
};

DTC_END_NAMESPACE

#endif
