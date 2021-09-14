#include "TransactionExecutor.h"

#include <iostream>
#include <vector>
#include <bitset>
using namespace std;

const int threadNum = 10;

class MyThread : public Thread
{
  int _threadIdx;
  const ThreadMutex* _mutex;
  ThreadCond* _cond;

public:
  MyThread(int idx, const ThreadMutex* mutex, ThreadCond* cond)
  : 
  _threadIdx(idx),
  _mutex(mutex),
  _cond(cond)
  {
  }

  void run()
  {
    // wait for the condition
    _mutex->lock();
    _cond->wait<ThreadMutex>(*_mutex);
    cout << "wake up from signal, thread:" << _threadIdx << endl;
    _mutex->unlock();
  }
};

int main()
{
  /*
   * Summary:
   *    the test result proving that, one mutex can bind to serval
   *    condtions
   * */
  ThreadMutex *mutex = new ThreadMutex();
  
  std::vector<ThreadCond*> conds;
  for (int i = 0; i < threadNum; i++)
  {
    conds.push_back(new ThreadCond());
  }

  std::vector<MyThread*> threadPool;
  for (int i = 0; i < threadNum; i++)
  {
    threadPool.push_back(new MyThread(i, mutex, conds[i]));
    threadPool[i]->start();
  }
    
  // sleep a while for child thread ready
  sleep(3);

  // signal them
  for (size_t idx = 0; idx < conds.size(); idx++)
  {
    conds[idx]->signal();
    // conds[idx]->signal();
    break;
    // cout << "singnal it!" << endl;
    // sleep(1);
  }
  
  sleep(1);
  // mutex->lock();
  conds[0]->wait<ThreadMutex>(*mutex);
  mutex->unlock();
  cout << "================" << endl;

  // waitting for the print message in thread
  while (true)
  {
    sleep(3);
  }
  cout << "test finished." << endl;

  return 0;
}
