#include "ConcurrTransExecutor.h"

static int32_t gInitValue = 0;
static int64_t gCoalesceValue = 0;

#define TEST_MEM_LEAK 0

class Request : public BasicRequest
{
private:
  char *mValue;

public:
  Request()
  {
    // log_error("construct request. this:%x", this);
  }
  virtual ~Request()
  {
    // log_error("deconstruct request. this:%x", this);
    delete mValue; 
  }

  int procRequest()
  {
#if TEST_MEM_LEAK
    // allocate mem here and release in the response
    mValue = (char*)malloc(1 << 20); // 1M
#else
    mValue = (char*)malloc(4);
    *(int*)mValue = gInitValue + 1;
    log_info("proc finished, value:%d", *(int*)mValue);
#endif

    return 1;
  }

  int procResponse(std::vector<BasicRequestPtr>& requests)
  {
    log_info("proc trans response!");
#if TEST_MEM_LEAK
#else
    for (size_t idx = 0; idx < requests.size(); idx++)
    {
      gCoalesceValue += *(int*)(static_cast<Request*>(requests[idx].get())->mValue);
      // log_error("request ref:%d", requests[idx].mRawPointer->getRef());
    }
#endif
  }
};

void createLogFile()
{
  const std::string& logFilePath = "./log";
  _init_log_("TransactionPool", logFilePath.c_str());
  _set_log_level_(3);

  if (access(logFilePath.c_str(), F_OK != 0))
  {
	  std::string cmd = "mkdir " + logFilePath;
	  system(cmd.c_str());
  }
  else
  {
    log_info("directory exist!!!!");
  }
	
	// check whether create successful
	if (access(logFilePath.c_str(), W_OK | X_OK) < 0)
	{
		log_error("create log file directory failed. path:%s", logFilePath.c_str());
  }

  return;
}

void limitCurrProcessCpuUsage(
    const struct timeval &tStart,
    const float usage)
{
  int64_t tElapse;
  struct timeval tEnd, tSleep;

  gettimeofday(&tEnd, NULL);
  tElapse = (tEnd.tv_sec - tStart.tv_sec) * 1000000 + (tEnd.tv_usec - tStart.tv_usec);
  tElapse *= usage;

  tSleep.tv_sec = tElapse / 1000000;
  tSleep.tv_usec = tElapse % 1000000;

  select(0, NULL, NULL, NULL, &tSleep);
}

int main()
{
  srand(NULL);

  createLogFile();
   daemon(1, 0);
  
  // begin to test the transaction pool
  struct timeval tv;
#if 1
  int loopNum = 100000000;
#else
   int loopNum = 1;
#endif
int64_t tBegin = GET_TIMESTAMP();
  while (loopNum--)
  {
    static int curLoop = 0;

    // cpu usage limitation
    gettimeofday(&tv, NULL);
    
    std::vector<BasicRequestPtr> reqs;
    int requestNum = 10;
    for (int i = 0; i < requestNum; i++)
    {
      reqs.push_back(new Request());
    }
    gInitValue = random() % 10000;
    // log_error("request ref:%d", reqs[0].mRawPointer->getRef());

    ConcurrTransExecutor::getInstance()->executeTransSync(reqs, -1);

    // log_error("request ref:%d", reqs[0].mRawPointer->getRef());

#if TEST_MEM_LEAK
#else
    // check result
    int64_t targetValue = requestNum * (gInitValue + 1);
    // int64_t targetValue = requestNum * (gInitValue++);
    if (targetValue != gCoalesceValue)
    {
      log_error("run test failed. initValue:%lld, coalValue:%lld, targValue:%lld", gInitValue, gCoalesceValue, targetValue);
      return -1;
    }
#endif

    gCoalesceValue = 0;
    curLoop++;
    // log_error("runing step:%d", curLoop);

    // limitCurrProcessCpuUsage(tv, 0.6);
  }
int64_t tEnd = GET_TIMESTAMP();
  
  /* 
   * Summary:
   * CPU-bound:
   *                    ThreadNum   DataNum          TimeCost(us)
   *   DoubleLock          3         100w             [184018359  204070493]
   *   DoubleLock          7(2*core) 100w             [180809002  ...]
   *   ResponseUnLock      3         100w              120886830 
   *   ResponseUnLock      7         100w              155974665 
   * */
  log_error("run test success! timeCost:%ld", tEnd - tBegin);
  /*while (true)
  {
    sleep(100);
  }*/

  return 0;
}
