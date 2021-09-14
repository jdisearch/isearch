// #include "../Atomic.h"
// #include "../Thread.h"
#include "TransactionExecutor.h"

#include <iostream>
#include <vector>
#include <bitset>
using namespace std;

const int threadNum = 7;
const int gLoopNum = 770000;
bool threadFin[threadNum] = {false};
// bitset<gLoopNum + 1> gBitValue;
char* gBitValue = (char*)malloc(gLoopNum * threadNum + 1);
Atomic gThreadValue;

class MyThread : public Thread
{
  int threadIdx;
public:
  MyThread(int idx) : threadIdx(idx){}
  void run()
  {
    int loopNum = gLoopNum;
    while (loopNum--)
    {
      Atomic rValue = gThreadValue.inc();
      //gBitValue.set(rValue.get(), 1);
      gBitValue[rValue.get()] = 1;     
    }
    threadFin[threadIdx] = true;
  }
};

int main()
{
  std::vector<MyThread*> threadPool;
  for (int i = 0; i < threadNum; i++)
  {
    threadPool.push_back(new MyThread(i));
    threadPool[i]->start();
  }
   
  // wait for all thread finish
  int finishedNum = 0;
  while (finishedNum != threadNum)
  {
    sleep(3);

    for (size_t i = 0; i < threadNum; i++)
    {
      if (threadFin[i]) finishedNum++;
    }
  }
  
  // check result
  int res = gThreadValue.get();
  if (threadNum * gLoopNum != res)
  {
    cout << "test failed. gThreadValue:" << res << " corrValue:" << threadNum * gLoopNum << endl;
  }
  else
  {
    // set the first bit to 1
    // gBitValue[0] = 1;
    // all bit must be 1
    // if (!gBitValue.all())
    cout << "test return value begin!" << endl;
    for (size_t i = 1; i <= gLoopNum * threadNum; i++)
    {
      if (!gBitValue[i])
      {
        cout << "Test failed in bitmap!" << endl;
      }
      else
      {
        // cout << "test successful. gThreadValue:" << res << " corrValue:" << threadNum * gLoopNum << endl;
      }
    }
  }
  
  cout << "test successful. gThreadValue:" << res << " corrValue:" << threadNum * gLoopNum << endl;

  return 0;
}
