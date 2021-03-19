/*
 * =====================================================================================
 *
 *       Filename:  singleton.h
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
#ifndef __SINGLETON_H__
#define __SINGLETON_H__
#include "lock.h"
#include "namespace.h"

DTC_BEGIN_NAMESPACE

template <class T>
struct CreateUsingNew
{
    static T *Create(void)
    {
        return new T;
    }

    static void Destroy(T *p)
    {
        delete p;
    }
};

template <class T, template <class> class CreationPolicy = CreateUsingNew>
class Singleton
{
public:
    static T *Instance(void);
    static void Destroy(void);

private:
    Singleton(void);
    Singleton(const Singleton &);
    Singleton &operator=(const Singleton &);

private:
    static T *_instance;
    static Mutex _mutex;
};

DTC_END_NAMESPACE

DTC_USING_NAMESPACE

//implement
template <class T, template <class> class CreationPolicy>
Mutex Singleton<T, CreationPolicy>::_mutex;

template <class T, template <class> class CreationPolicy>
T *Singleton<T, CreationPolicy>::_instance = 0;

template <class T, template <class> class CreationPolicy>
T *Singleton<T, CreationPolicy>::Instance(void)
{
    if (0 == _instance)
    {
        ScopedLock guard(_mutex);

        if (0 == _instance)
        {
            _instance = CreationPolicy<T>::Create();
        }
    }

    return _instance;
}

/* BUGFIX by ada*/
#if 0
template <class T, template <class> class CreationPolicy> 
void Singleton<T, CreationPolicy>::Destroy (void)
{
	return CreationPolicy<T>::Destroy (_instance);
}
#endif

template <class T, template <class> class CreationPolicy>
void Singleton<T, CreationPolicy>::Destroy(void)
{
    if (0 != _instance)
    {
        ScopedLock guard(_mutex);
        if (0 != _instance)
        {
            CreationPolicy<T>::Destroy(_instance);
            _instance = 0;
        }
    }

    return;
}

#endif //__SINGLETON_H__
