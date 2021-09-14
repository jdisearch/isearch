/**
 * 原子计数类. 
 * Jul 16, 2019
 *      By qiuyu  
 */

#ifndef __ATOMIC_H_
#define __ATOMIC_H_

#include <stdint.h>

__BEGIN_DECLS  // extern "C"{

#define ATOMIC_LOCK "lock ; "

// volatile关键字禁止编译器对修饰的变量进行优化
typedef struct { volatile int counter; } my_atomic_t;

#define atomic_read(v)        ((v)->counter)

#define atomic_set(v,i)       (((v)->counter) = (i))

__END_DECLS // }

/**
 * 原子操作类,对int做原子操作
 */
class Atomic
{
public:
    // 原子类型
    typedef int atomic_type;

    // 构造函数,初始化为0 
    Atomic(atomic_type at = 0)
    {
        set(at);
    }

    Atomic& operator++()
    {
        inc();
        return *this;
    }

    Atomic& operator--()
    {
        dec();
        return *this;
    }

    operator atomic_type() const
    {
        return get();
    }

    Atomic& operator+=(atomic_type n)
    {
        add(n);
        return *this;
    }

    Atomic& operator-=(atomic_type n)
    {
        sub(n);
        return *this;
    }

    Atomic& operator=(atomic_type n)
    {
        set(n);
        return *this;
    }

    atomic_type get() const           { return mCounterValue.counter; }

    // 添加
    atomic_type add(atomic_type i)    { return add_and_return(i); }

    // 减少
    atomic_type sub(atomic_type i)    { return add_and_return(-i); }

    // 自加1
    atomic_type inc()               { return add(1); }

    // 自减1
    atomic_type dec()               { return sub(1); }

    // 自加1
    void inc_fast()
    {
        __asm__ __volatile__(
            ATOMIC_LOCK "incl %0"
            :"=m" (mCounterValue.counter)
            :"m" (mCounterValue.counter));
    }

    /**
     * @brief 自减1
     * Atomically decrements @mCounterValue by 1 and returns true if the
     * result is 0, or false for all other
     */
    bool dec_and_test()
    {
        unsigned char c;

        __asm__ __volatile__(
            ATOMIC_LOCK "decl %0; sete %1"
            :"=m" (mCounterValue.counter), "=qm" (c)
            :"m" (mCounterValue.counter) : "memory");

        return c != 0;
    }

    // 设置值
    atomic_type set(atomic_type i)
    {
        mCounterValue.counter = i;

        return i;
    }

protected:
    // 增加并返回值
    int add_and_return(int i)
    {
        /* Modern 486+ processor */
        int __i = i;
        __asm__ __volatile__(
            ATOMIC_LOCK "xaddl %0, %1;"
            :"=r"(i)
            :"m"(mCounterValue.counter), "0"(i));
        return i + __i;
    }

protected:
    // 值
    my_atomic_t    mCounterValue;
};

#endif // __ATOMIC_H_
