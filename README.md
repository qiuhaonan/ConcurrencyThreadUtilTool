# ConcurrencyThreadUtilTool
编写多线程程序时想要知道在整个程序运行期间内部各个线程对cpu的占用率时,需要对cpu文件和线程文件进行记录和计算.
为了方便对程序进行监测,使用这个小工具可以帮助你快速完成这个工作.
用法:
   声明一个全局对象UtilRipper
   在各个线程启动时调用方法start(string name,int cpu_id),传入绑定的核
   在各个线程结束时调用方法end(string name,int cpu_id).
   在析构UtilRipper对象时会自动报告程序中各个线程的运行情况.
