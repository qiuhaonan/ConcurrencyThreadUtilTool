#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <string>
#include <string.h>
#include <pthread.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <sys/types.h>
#include <sys/syscall.h>
#include <memory>
#include <fstream>
#define CPU_PATH "/proc/stat"
#define gettidv1() syscall(__NR_gettid)
#define gettidv2() syscall(SYS_gettid)
using namespace std;

class time_stat
{
   public:
   uint64_t user_time;
   uint64_t sys_time;
   uint64_t nice_time;
   uint64_t idle_time;
   uint64_t iowait_time;
   uint64_t irq_time;
   uint64_t softirq_time;
   uint64_t stealstolen_time;
   uint64_t guest_time;
   uint64_t get_sum()
   {
     return user_time+sys_time;
   }
};

class TimePair
{
  public:
  time_stat start;
  time_stat end;
  TimePair(){}
  TimePair(const TimePair& tp)
  {
    memcpy(&start,&(tp.start),sizeof(time_stat));
    memcpy(&end,&(tp.end),sizeof(time_stat));
  }
  TimePair& operator = (const TimePair& tp)
  {
    memcpy(&start,&(tp.start),sizeof(time_stat));
    memcpy(&end,&(tp.end),sizeof(time_stat));
  }
};

struct IdPair
{
  int thread_id;
  int cpu_id;
};

class UtilRipper
{
  std::mutex time_lock,utilization_lock;
  std::map<string,TimePair> cpu_info,thread_info;
  std::map<string,struct IdPair> register_record;
  std::map<string,double> utilization_record;
  void read_cpu_stat(int cpu_id,time_stat& ts);
  void read_thread_stat(int thread_id,time_stat& ts);
  void split(string& s,string& delim,vector<string>& ret);
  void compute_thread_util(string& name,TimePair& cputp,TimePair& threadtp);
  static pid_t gettid() { return syscall(__NR_gettid);};
  void report();
  void print_star(int num);
  public:
  void start(string name,int cpu_id);
  void end(string name,int cpu_id);
  ~UtilRipper()
  {
    report();
  }
};


UtilRipper ur;


void UtilRipper::print_star(int num)
{
  for(int i=0;i<num;++i)
     cout<<"*";
}

void UtilRipper::report()
{
  print_star(25);
  cout<<setw(2)<<""<<"Thread Utilization Reports"<<setw(2)<<"";
  print_star(25);
  cout<<endl;
  for(auto iter:utilization_record)
  {
    string name = iter.first;
    double uti  = iter.second;
    IdPair& ip  = register_record[name];
    cout.setf(ios::right);
    cout<<setw(20)<<"Name"<<setw(20)<<"Cpu-Id"<<setw(20)<<"Thread-Id"<<setw(20)<<"Utilization"<<endl;
    cout<<setw(20)<<name<<setw(20)<<ip.cpu_id<<setw(20)<<ip.thread_id<<setw(20)<<uti<<endl;
    cout.unsetf(ios::right);
  } 
}

void UtilRipper::start(string name,int cpu_id)
{
  int thread_id = gettid();
  IdPair ip;
  ip.thread_id =thread_id;
  ip.cpu_id =cpu_id;
  TimePair cputp,threadtp;
  read_cpu_stat(cpu_id,cputp.start);
  read_thread_stat(thread_id,threadtp.start);
  {
    std::lock_guard<std::mutex> lock(time_lock);
    cpu_info[name] = cputp;
    thread_info[name] = threadtp;
    register_record[name] = ip;
  }
}

void UtilRipper::end(string name,int cpu_id)
{ 
  int thread_id = gettid();
  TimePair cputp,threadtp;
  {
    std::lock_guard<std::mutex> lock(time_lock);
    cputp = cpu_info[name];
    threadtp = thread_info[name]; 
  }
  read_cpu_stat(cpu_id,cputp.end);
  read_thread_stat(thread_id,threadtp.end);
  {
    std::lock_guard<std::mutex> lock(time_lock);
    cpu_info[name]=cputp;
    thread_info[name]=threadtp; 
  }
  compute_thread_util(name,cputp,threadtp);
}

void UtilRipper::compute_thread_util(string& name,TimePair& cputp,TimePair& threadtp)
{
  uint64_t cpu_diff = cputp.end.get_sum() - cputp.start.get_sum();
  uint64_t thread_diff = threadtp.end.get_sum() - threadtp.start.get_sum();
  double uti = thread_diff*100.0/(cpu_diff);
  std::lock_guard<std::mutex> lock(utilization_lock);
  utilization_record[name] = uti;
}

void UtilRipper::split(std::string& s, std::string& delim,std::vector< std::string >& ret)  
{  
  size_t last = 0;  
  size_t index=s.find_first_of(delim,last);  
  while (index!=std::string::npos)  
  {  
    ret.push_back(s.substr(last,index-last));  
    last=index+1;  
    index=s.find_first_of(delim,last);  
  }  
  if (index-last>0)  
  {  
    ret.push_back(s.substr(last,index-last));  
  }  
} 

void UtilRipper::read_cpu_stat(int core,time_stat& info)
{
  fstream cpu_file;
  cpu_file.open(CPU_PATH,ios::in);
  int line = core+1;
  string content,delim(" ");
  while(line>=0)
  {
    getline(cpu_file,content);
    --line;
  }
  std::vector<string> params;
  split(content,delim,params);
  info.user_time = std::stoi(params[1]);
  info.sys_time  = std::stoi(params[3]);
  cpu_file.close();
}

void UtilRipper::read_thread_stat(int thread_id,time_stat& info)
{ 
  fstream thread_file;
  string thread_path("/proc/");
  thread_path = thread_path + std::to_string(thread_id) + "/task/" + std::to_string(thread_id) + "/stat";
  thread_file.open(thread_path.c_str(),ios::in);
  string content,delim(" ");
  getline(thread_file,content);
  vector<string> params;
  split(content,delim,params);
  info.user_time = std::stoi(params[13]);
  info.sys_time  = std::stoi(params[14]);
  thread_file.close();
}

void bind_core(int cpu_core)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_core,&cpuset);
  pthread_setaffinity_np(pthread_self(),sizeof(cpuset),&cpuset);
}

bool start = false;

void run(int cpu_core)
{
  while(!start);
  bind_core(cpu_core);
  string name("Tester@"+std::to_string(cpu_core));
  ur.start(name,cpu_core);
  uint64_t i= 10229823374;
  while(i--!=0);
  usleep(cpu_core*10000);
  ur.end(name,cpu_core);
}

int main()
{
  thread t = std::thread(run,1);
  thread s = std::thread(run,2);
  start =true;
  t.join();
  s.join();
  return 0;
}


