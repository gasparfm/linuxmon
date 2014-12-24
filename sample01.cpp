/**
*************************************************************
* @file sample01.cpp
* @brief Umon sample file
* Show which thing we can monitor with umon.h
*
* @author Gaspar Fernández <blakeyed@totaki.com>
* @version 0.1 Alpha
* @date 19 nov 2014
*
* Changelog:
*
*
*
*
*************************************************************/

#include "umon.h"
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
  cout << "Tests:" << endl;
  cout << "size(): "<< endl;
  //  cout << "Simple size: "<<Umon::size(123456, -1000)<< endl; // overflow
  cout << " * Simple size [prec=-100]: "<<Umon::size(12345678, -100)<< endl;
  cout << " * Simple size  [prec=-10]: "<<Umon::size(12345678, -10)<< endl;
  cout << " * Simple size    [prec=0]: "<<Umon::size(12345678, 0)<< endl;
  cout << " * Simple size    [prec=1]: "<<Umon::size(12345678, 1)<< endl;
  cout << " * Simple size   [prec=10]: "<<Umon::size(12345678, 10)<< endl;
  cout << " * Simple size  [prec=100]: "<<Umon::size(12345678, 100)<< endl;
  cout << "sysinfo() and related access methods: "<<endl;
  cout << " * Total amount of RAM: "<<Umon::size(Umon::totalram())<<endl;
  cout << " * Free RAM: "<<Umon::size(Umon::freeram())<<endl;
  cout << " * Used RAM: "<<Umon::size(Umon::usedram())<<endl;
  cout << " * Used RAM (with buffers): "<<Umon::size(Umon::usedramb())<<endl;
  cout << " * Shared RAM: "<<Umon::size(Umon::sharedram())<<endl;
  cout << " * Buffer RAM: "<<Umon::size(Umon::bufferram())<<endl;
  cout << " * Total Swap: "<<Umon::size(Umon::totalswap())<<endl;
  cout << " * Free Swap: "<<Umon::size(Umon::freeswap())<<endl;
  cout << " * Used Swap: "<<Umon::size(Umon::usedswap())<<endl;
  cout << " * Total High Memory: "<<Umon::size(Umon::totalHighMem())<<endl;
  cout << " * Free High Memory: "<<Umon::size(Umon::freeHighMem())<<endl;
  cout << " * Memory Unit: "<<Umon::size(Umon::memoryUnitSize())<<endl;
  cout << " * Seconds since uptime: "<<Umon::uptime()<<endl;
  cout << " * Total threads: "<<Umon::totalThreads()<<endl;
  cout << " * System load: "<<Umon::sysload1u()<<" "<<Umon::sysload5u()<<" "<<Umon::sysload15u()<<endl;
  cout << " * System load: "<<Umon::sysload1()<<" "<<Umon::sysload5()<<" "<<Umon::sysload15()<<endl;
  cout << "system configuration information: "<<endl;
  cout << " * Max argument length: "<<Umon::maxArgumentsLength()<<endl;
  cout << " * Max processes per user: "<<Umon::maxProcessesPerUser()<<endl;
  cout << " * Clock Ticks Per Second: "<<Umon::ticksPerSecond()<<endl;
  cout << " * Max opened files: "<<Umon::maxOpenedFiles()<<endl;
  cout << " * Memory page size: "<<Umon::size(Umon::pageSize())<<endl;
  cout << " * Total RAM pages: "<<Umon::totalPagesRam()<<endl;
  cout << " * Available RAM pages: "<<Umon::availablePagesRam()<<endl;
  cout << " * Processors configured: "<<Umon::cpuCount()<<endl;
  cout << " * Processors online: "<<Umon::onlineCpuCount()<<endl;
  cout << "mount points information:"<<endl;
  Umon::valueCheckInterval(500);
  Umon::Mounts::MountPoints mp = Umon::Mounts::mountsInfo();
  for (Umon::Mounts::MountPoint m : mp)
    {
      cout << "  - "<<m.fileSystem<<" "<<m.mountPoint<<" "<<m.type<<" "<<m.options<<" "<<m.dumpFrequency<<" "<<m.passNumber<<"=>"<<m.freeSpace()<<"("<<Umon::size(m.freeSpace())<<")"<<m.totalSpace()<<" "<<m.usedSpace()<<" "<<m.usedRatio()<<" "<<m.statfs_errno<<endl;
    }
  Umon::Proc::buildProcSummary();
  cout << "---------------------------___"<<endl;
  cout << "Process count: "<<Umon::Proc::processCount()<<std::endl;
  cout << "TTBS: "<<Umon::Proc::timeToBuildSummary()<<endl;
  cout << "Free space on / : "<<Umon::size(Umon::Mounts::getFreeSpace("/"))<<endl;
  cout << "Used space on / : "<<Umon::size(Umon::Mounts::getUsedSpace("/"))<<endl;
  cout << "Total space on / : "<<Umon::size(Umon::Mounts::getTotalSpace("/"))<<endl;
  cout << "Used ratio on / : "<<Umon::Mounts::getUsedRatio("/")<<endl;
  cout << "Number of apache processes: "<<Umon::Proc::countProcess("apache2")<<endl;
  cout << "Total PCPU of Emacs: "<<Umon::Proc::totalPCPU("emacs")<<endl;
  Umon::Proc::MultiProc p = Umon::Proc::getByName("apacho");
  cout << p.name<<endl;
  for (auto sp : Umon::Proc::getAllProcs())
    {
      cout << "["<<sp.first<<"] "<<sp.second.name<<". TotalCPU: "<<sp.second.totalpcpu<<endl;
    }
  cout << "Processes with CPU >= 2%"<<endl<<"----------------------------"<<endl;
  for (auto sp : Umon::Proc::getByPCPU(2, true))
    {
      cout << "["<<sp.pid<<"] "<<sp.name<<". TotalCPU: "<<sp.totalpcpu<<" Vsize: "<<Umon::size(sp.vsize)<<" RSS: "<<Umon::size(sp.rss)<<endl;
    }
  cout << "Processes with all threads CPU >= 5%"<<endl<<"----------------------------"<<endl;
  for (auto sp : Umon::Proc::getByPCPUCol(3, true))
    {
      cout << "["<<sp.first<<"] "<<sp.second.name<<". TotalCPU: "<<sp.second.totalpcpu<<endl;
    }
  cout << "Processes with Vsize >= 1Gb"<<endl<<"----------------------------"<<endl;
  for (auto sp : Umon::Proc::getByVsize(1024*1024*1024))
    {
      cout << "["<<sp.pid<<"] "<<sp.name<<". TotalCPU: "<<sp.totalpcpu<<" Vsize: "<<Umon::size(sp.vsize)<<" RSS: "<<Umon::size(sp.rss)<<endl;
    }
  cout << "Processes with all threads Vsize >= 2Gb"<<endl<<"----------------------------"<<endl;
  for (auto sp : Umon::Proc::getByVsizeCol((unsigned long)2*1024*1024*1024))
    {
      cout << "["<<sp.first<<"] "<<sp.second.name<<". TotalCPU: "<<sp.second.totalpcpu<<" Vsize: "<<Umon::size(sp.second.totalvsize)<<" RSS: "<<Umon::size(sp.second.totalrss)<<endl;
    }

  cout << "Value duration: "<<Umon::valueDuration()<<endl;
  cout << "Sets value duration to 1.5: "<<Umon::valueDuration(1.5)<<endl;
  cout << "Value duration: "<<Umon::valueDuration()<<endl;

  // sleep(1);
  // Umon::buildProcessSummary();
  // cout << "TTBS: "<<Umon::timeToBuildSummary()<<endl;
  // cout << "Process count: "<<Umon::processCount()<<std::endl;
  return 0;
}

