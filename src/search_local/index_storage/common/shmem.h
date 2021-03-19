/*
 * =====================================================================================
 *
 *       Filename:  sheme.h
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
#define IPC_PERM 0644

class SharedMemory
{
private:
	int m_key;
	int m_id;
	unsigned long m_size;
	void *m_ptr;
	int lockfd;

public:
	SharedMemory();
	~SharedMemory();

	unsigned long Open(int key);
	unsigned long Create(int key, unsigned long size);
	unsigned long Size(void) const { return m_size; }
	void *Ptr(void) const { return m_ptr; }
	void *Attach(int ro = 0);
	void Detach(void);
	int Lock(void);
	void Unlock(void);

	/* 删除共享内存 */
	int Delete(void);
};
