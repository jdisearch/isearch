/**
*  智能指针类(智能指针不能相互引用, 否则内存泄漏). 
*  
*  所有需要智能指针支持的类都需要从该对象继承，
*  
*  内部采用引用计数Atomic实现，对象可以放在容器中；
*  
*  AutoPtrBase为计数器，AutoPtr为智能指针模板类
*
*  Jul 15, 2019
*     by qiuyu
*/

#ifndef __AUTO_PTR_H_
#define __AUTO_PTR_H_

#include "Atomic.h"
#include "log.h"

#include <algorithm>

/*
*  智能指针基类.
 * */
template<class  T>
class AutoPtrBase
{
public:

     /** 原子计数类型*/
    typedef T atomic_type;

    /**
     * @brief 复制.
     *
     * @return HandleBase&
     */
    AutoPtrBase& operator=(const AutoPtrBase&)
    {
        return *this;
    }

    /**
     * @brief 增加计数
     */
    void incRef() { mAtomic.inc_fast(); }

    /**
     * @brief 减少计数, 当计数==0时, 且需要删除数据时, 释放对象
     */
    void decRef()
    {
        if(mAtomic.dec_and_test() && !mNoDelete)
        {
            mNoDelete = true;
            delete this;
        }
    }

    /**
     * @brief 获取计数.
     *
     * @return int 计数值
     */
    int getRef() const        { return mAtomic.get(); }

    /**
     * @brief 设置不自动释放. 
     *  
     * @param b 是否自动删除,true or false
     */
    void setNoDelete(bool b)  { mNoDelete = b; }

protected:

    /**
     * @brief 构造函数
     */
    AutoPtrBase() : mAtomic(0), mNoDelete(false)
    {
    }

    /**
     * @brief 拷贝构造
     */
    AutoPtrBase(const AutoPtrBase&) : mAtomic(0), mNoDelete(false)
    {
    }

    /**
     * @brief 析够
     */
    virtual ~AutoPtrBase()
    {
    }

protected:

    /**
     * 计数
     */
    atomic_type   mAtomic;

    /**
     * 是否自动删除
     */
    bool        mNoDelete;
};

template<>
inline void AutoPtrBase<int>::incRef() 
{ 
    //__sync_fetch_and_add(&mAtomic,1);
    ++mAtomic; 
}

template<> 
inline void AutoPtrBase<int>::decRef()
{
    //int c = __sync_fetch_and_sub(&mAtomic, 1);
    //if(c == 1 && !mNoDelete)
    if(--mAtomic == 0 && !mNoDelete)
    {
        mNoDelete = true;
        delete this;
    }
}

template<> 
inline int AutoPtrBase<int>::getRef() const        
{ 
    //return __sync_fetch_and_sub(const_cast<volatile int*>(&mAtomic), 0);
    return mAtomic; 
} 

typedef AutoPtrBase<Atomic> HandleBase;

/**
 * @brief 智能指针模板类. 
 *  
 * 可以放在容器中,且线程安全的智能指针. 
 *  
 * 通过它定义智能指针，该智能指针通过引用计数实现， 
 *  
 * 可以放在容器中传递. 
 *  
 * template<typename T> T必须继承于HandleBase 
 */
template<typename T>
class AutoPtr
{
public:

    /**
     * 元素类型
     */
    typedef T element_type;

    /**
     * @brief 用原生指针初始化, 计数+1. 
     *  
     * @param p
     */
    AutoPtr(T* p = 0)
    {
        mRawPointer = p;

        if(mRawPointer)
        {
            mRawPointer->incRef();
        }
    }

    /**
     * @brief 用其他智能指针r的原生指针初始化, 计数+1. 
     *  
     * @param Y
     * @param r
     */
    /*用Y类型的智能指针对象r来构造当前对象
     * eg:
     *    AutoPtr<Y> yPtr;
     *    AutoPtr<T> tPtr(yPtr)*/
    template<typename Y>
    AutoPtr(const AutoPtr<Y>& r)
    {
        mRawPointer = r.mRawPointer;

        if(mRawPointer)
        {
            mRawPointer->incRef();
        }
    }

    /**
     * @brief 拷贝构造, 计数+1. 
     *  
     * @param r
     */
    AutoPtr(const AutoPtr& r)
    {
        mRawPointer = r.mRawPointer;

        if(mRawPointer)
        {
            mRawPointer->incRef();
        }
    }

    /**
     * @brief 析构
     */
    ~AutoPtr()
    {
        if(mRawPointer)
        {
            mRawPointer->decRef();
        }
    }

    /**
     * @brief 赋值, 普通指针. 
     *  
     * @param p 
     * @return AutoPtr&
     */
    AutoPtr& operator=(T* p)
    {
        if(mRawPointer != p)
        {
            if(p)
            {
                p->incRef();
            }

            T* ptr = mRawPointer;
            mRawPointer = p;

            if(ptr)
            {
                ptr->decRef();
            }
        }
        return *this;
    }

    /**
     * @brief 赋值, 其他类型智能指针. 
     *  
     * @param Y
     * @param r 
     * @return AutoPtr&
     */
    template<typename Y>
    AutoPtr& operator=(const AutoPtr<Y>& r)
    {
        if(mRawPointer != r.mRawPointer)
        {
            if(r.mRawPointer)
            {
                r.mRawPointer->incRef();
            }

            T* ptr = mRawPointer;
            mRawPointer = r.mRawPointer;

            if(ptr)
            {
                ptr->decRef();
            }
        }
        return *this;
    }

    /**
     * @brief 赋值, 该类型其他执政指针. 
     *  
     * @param r 
     * @return AutoPtr&
     */
    AutoPtr& operator=(const AutoPtr& r)
    {
        if(mRawPointer != r.mRawPointer)
        {
            if(r.mRawPointer)
            {
                r.mRawPointer->incRef();
            }

            T* ptr = mRawPointer;
            mRawPointer = r.mRawPointer;

            if(ptr)
            {
                ptr->decRef();
            }
        }
        return *this;
    }

    /**
     * @brief 将其他类型的智能指针换成当前类型的智能指针. 
     *  
     * @param Y
     * @param r 
     * @return AutoPtr
     */
    template<class Y>
    static AutoPtr dynamicCast(const AutoPtr<Y>& r)
    {
        return AutoPtr(dynamic_cast<T*>(r.mRawPointer));
    }

    /**
     * @brief 将其他原生类型的指针转换成当前类型的智能指针. 
     *  
     * @param Y
     * @param p 
     * @return AutoPtr
     */
    template<class Y>
    static AutoPtr dynamicCast(Y* p)
    {
        return AutoPtr(dynamic_cast<T*>(p));
    }

    /**
     * @brief 获取原生指针.
     *
     * @return T*
     */
    T* get() const
    {
        return mRawPointer;
    }

    /**
     * @brief 调用.
     *
     * @return T*
     */
    T* operator->() const
    {
        if(!mRawPointer)
        {
          log_error("internal error, raw pointer is null!");
        }

        return mRawPointer;
    }

    /**
     * @brief 引用.
     *
     * @return T&
     */
    T& operator*() const
    {
        if(!mRawPointer)
        {
          log_error("internal error, raw pointer is null!");
        }

        return *mRawPointer;
    }

    /**
     * @brief 是否有效.
     *
     * @return bool
     */
    operator bool() const
    {
        return mRawPointer ? true : false;
    }

    /**
     * @brief  交换指针. 
     *  
     * @param other
     */
    void swap(AutoPtr& other)
    {
        std::swap(mRawPointer, other.mRawPointer);
    }

public:
    T*          mRawPointer;
};

template<typename T, typename U>
inline bool operator==(const AutoPtr<T>& lhs, const AutoPtr<U>& rhs)
{
    T* l = lhs.get();
    U* r = rhs.get();
    if(l && r)
    {
        return *l == *r;
    }
    else
    {
        return !l && !r;
    }
}

template<typename T, typename U>
inline bool operator!=(const AutoPtr<T>& lhs, const AutoPtr<U>& rhs)
{
    T* l = lhs.get();
    U* r = rhs.get();
    if(l && r)
    {
        return *l != *r;
    }
    else
    {
        return l || r;
    }
}

/**
 * 小于判断, 用于放在map等容器中. 
 */
template<typename T, typename U>
inline bool operator<(const AutoPtr<T>& lhs, const AutoPtr<U>& rhs)
{
    T* l = lhs.get();
    U* r = rhs.get();
    if(l && r)
    {
        return *l < *r;
    }
    else
    {
        return !l && r;
    }
}
#endif // __AUTO_PTR_H_
