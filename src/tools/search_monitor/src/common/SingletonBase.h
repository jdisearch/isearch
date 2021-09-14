/**
 * july 16, 2019
 * created by qiuyu
 *
 */

#ifndef __SingletonBase_H_
#define __SingletonBase_H_

#include <cassert>
#include <cstdlib>

/////////////////////////////////////////////////
/** 
 * 单件实现类
 * 
 * 没有实现对单件生命周期的管理,使用示例代码如下:
 * 
 * class A : public SingletonBase<A, CreateStatic,  DefaultLifetime>
 * {
 *  public:
 * 
 *   A(){cout << "A" << endl;}
 *   ~A()
 *   {
 *     cout << "~A" << endl;
 *   }
 *
 *   void test(){cout << "test A" << endl;}
 * };
 * 
 * 对象的创建方式由CreatePolicy指定, 有如下方式:
 * 
 * CreateUsingNew: 在堆中采用new创建
 * 
 * CreateStatic: 在栈中采用static创建
 * 
 * 对象生命周期管理由LifetimePolicy指定, 有如下方式:
 * 
 * DefaultLifetime:缺省声明周期管理
 * 
 *如果单件对象已经析够, 但是还有调用, 会触发异常 
 * 
 * PhoneixLifetime:不死生命周期
 * 
 * 如果单件对象已经析够, 但是还有调用, 会再创建一个
 * 
 * NoDestroyLifetime:不析够
 * 
 * 对象创建后不会调用析够函数析够, 通常采用实例中的方式就可以了
 *
 */              
/////////////////////////////////////////////////

/**
 * @brief 定义CreatePolicy:定义对象创建策略
 */
template<typename T>
class CreateUsingNew
{
public:
    /**
     * @brief  创建.
     *  
     * @return T*
     */
    static T* create() 
    { 
        return new T; 
    }

    /**
     * @brief 释放. 
     *  
     * @param t
     */
    static void destroy(T *t) 
    { 
        delete t; 
    }
};

template<typename T>
class CreateStatic
{
public:
    /**
     * @brief   最大的空间
     */ 
    union MaxAlign 
    { 
        char t_[sizeof(T)]; 
        long double longDouble_; 
    }; 

    /**
     * @brief   创建. 
     * 
     * @return T*
     */
    static T* create() 
    { 
        static MaxAlign t; 
        return new(&t) T; // placement new语法 
    }

    /**
     * @brief   释放. 
     *  
     * @param t
     */
    static void destroy(T *t) 
    {
        t->~T();
    }
};

////////////////////////////////////////////////////////////////
/**
 * @brief 定义LifetimePolicy:定义对象的声明周期管理
 */
template<typename T>
class DefaultLifetime
{
public:
    static void deadReference()
    {
    }

    static void scheduleDestruction(T*, void (*pFun)())
    {
      // 注册程序退出前系统负责调用的用户层函数pFun(如果程序时crash退出，将不会调用)
        std::atexit(pFun);
    }
};

template<typename T>
class PhoneixLifetime
{
public:
    static void deadReference()
    {
        _bDestroyedOnce = true;
    }

    static void scheduleDestruction(T*, void (*pFun)())
    {
        if(!_bDestroyedOnce)
            std::atexit(pFun);
    }
private:
    static bool _bDestroyedOnce; 
};
template <class T> 
bool PhoneixLifetime<T>::_bDestroyedOnce = false; 

template <typename T> 
struct NoDestroyLifetime 
{ 
    static void scheduleDestruction(T*, void (*)()) 
    {
    } 

    static void deadReference() 
    {
    } 
}; 

//////////////////////////////////////////////////////////////////////
// Singleton
template
<
    typename T,
    template<class> class CreatePolicy   = CreateUsingNew, // 默认申请策略为CreateUsingNew
    template<class> class LifetimePolicy = DefaultLifetime
>
class SingletonBase 
{
public:
    typedef T  instance_type;
    typedef volatile T volatile_type;

    /**
     * @brief 获取实例
     * 
     * @return T*
     */
    static T *getInstance()
    {
        //加锁, 双check机制, 保证正确和效率(比在此处加锁效率高)
        if(!mInstance)
        {
            ThreadLock::Lock_t lock(mLock);
            if(!mInstance)
            {
                if(mDestroyed)
                {
                    LifetimePolicy<T>::deadReference();
                    mDestroyed = false;
                }
                mInstance = CreatePolicy<T>::create();
                LifetimePolicy<T>::scheduleDestruction((T*)mInstance, &destroySingleton);
            }
        }
        
        return (T*)mInstance;
    }
    
    virtual ~SingletonBase(){}; 

protected:

    static void destroySingleton()
    {
        assert(!mDestroyed);
        CreatePolicy<T>::destroy((T*)mInstance);
        mInstance = NULL;
        mDestroyed = true;
    }
protected:

    static ThreadLock    mLock;
    static volatile T*      mInstance;
    static bool             mDestroyed;

protected:
    SingletonBase(){}
    SingletonBase (const SingletonBase &); 
    SingletonBase &operator=(const SingletonBase &);
};

// 静态变量初始化
template <class T, template<class> class CreatePolicy, template<class> class LifetimePolicy> 
ThreadLock SingletonBase<T, CreatePolicy, LifetimePolicy>::mLock; 

template <class T, template<class> class CreatePolicy, template<class> class LifetimePolicy> 
bool SingletonBase<T, CreatePolicy, LifetimePolicy>::mDestroyed = false; 

template <class T, template<class> class CreatePolicy, template<class> class LifetimePolicy> 
volatile T* SingletonBase<T, CreatePolicy, LifetimePolicy>::mInstance = NULL; 

#endif
