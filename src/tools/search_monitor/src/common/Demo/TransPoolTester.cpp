#include "TransactionExecutor.h"

static int64_t gInitValue = 0;
static int64_t gCoalesceValue = 0;

#define TEST_MEM_LEAK 0

class Response : public BasicResponse
{
private:
  int64_t mCounterValue;

public:
  Response() : mCounterValue(0) {}

  virtual int procResponse(std::deque<BasicResponsePtr>& response)
  {
    while (!response.empty())
    {
      BasicResponsePtr resp = response.front();
#if TEST_MEM_LEAK
      char *val = (char*)(static_cast<Response*>(resp.get())->mCounterValue);
      delete val;
#else
      gCoalesceValue += static_cast<Response*>(resp.get())->mCounterValue;
#endif
      response.pop_front();
    }

    return 1;
  }

  void setValue(const int64_t value) { mCounterValue = value; }
};

class Request : public BasicRequest
{
public:
  Request() {}

  int procRequest(BasicResponsePtr& resp)
  {
    resp = new Response();
#if TEST_MEM_LEAK
    // allocate mem here and release in the response
    char *val = new char(1 << 20); // 1M
    static_cast<Response*>(resp.get())->setValue((int64_t)val);
#else
    int64_t val = gInitValue + 1;
    static_cast<Response*>(resp.get())->setValue(val);
#endif

    return 1;
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
  int loopNum = 1000000;
#else
   int loopNum = 10000;
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
    gInitValue = random();

    TransactionExecutor::getInstance()->executeTransSync(reqs, -1);

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
   *   ResponseUnLock      3         100w             245552365
   *   ResponseUnLock      7         100w             260934935 
   * */
  log_error("run test success! timeCost:%ld", tEnd - tBegin);
sleep(1000000);
  return 0;
}
