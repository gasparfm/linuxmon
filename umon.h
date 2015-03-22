/* @(#)umon.h
 */
#pragma once

#ifndef _UMON_H
#define _UMON_H 1
/*
*************************************************************
* @file umon.cpp
* @brief Linux system monitor
* Some tools to monitor your linux machine
*
* @author Gaspar Fernández <blakeyed@totaki.com>
* License:
* -----------------------------------------------------------------------------
* Copyright (c) 2014, Gaspar Fernández
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*  * Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of unixmon nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* -------------------------------------------------------------------------------
* @version
* @date 15 nov 2014
* Changelog:
* 20141115: Starting this project
* 20141116: sysinfo() wrappers
* 20141120: sysconf() wrappers
* 20141122: some tools
* 20141123: mounts summary
* 20141206: starting process summary
* 20141212: use std::function to walk through processes
* 20141220: bug fix: SIGABRT when loading mountpoints information. Some threads where finishing after 
*           finishing mountsInfo() function and the internal vector was not written properly.
* 20141222: processes information
* 20141224: some doc and githubbing !!
* 20150320: Made functions static
*
* Bugs:
* 20141219: Sometimes received SIGABRT when loading mounts information. FIXED 20141220
*
* To-dos:
*  - Have process start time in chrono::time_point
*  - Insert cmdline into process information
*  - Insert OOM information into process struct
*  - Process state constants (R, S, Z...)
*  - List processes by state
*  - Folder size counter
*  - Network device detection
*  - Temperature fetch
*
* Useful stuff for future features:
* * doc for proc: http://man7.org/linux/man-pages/man5/proc.5.html (better than my man proc)
* * proc states (from man 5 proc):
	(3) state  %c
		One of the following characters, indicating process
		state:
                        R  Running
                        S  Sleeping in an interruptible wait
                        D  Waiting in uninterruptible disk sleep
                        Z  Zombie
                        T  Stopped (on a signal) or (before Linux 2.6.33)
                           trace stopped
                        t  Tracing stop (Linux 2.6.33 onward)
                        W  Paging (only before Linux 2.6.0)
                        X  Dead (from Linux 2.6.0 onward)
                        x  Dead (Linux 2.6.33 to 3.13 only)
                        K  Wakekill (Linux 2.6.33 to 3.13 only)
                        W  Waking (Linux 2.6.33 to 3.13 only)
                        P  Parked (Linux 3.9 to 3.13 only)
*
*
*************************************************************/

#include <string>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <cstdint>
#include <sys/sysinfo.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <map>
#include <mntent.h>
#include <mutex>
#include <thread>
#include <sys/vfs.h>
#include <functional>
#include <fcntl.h>
#include <sys/dir.h>

namespace Umon
{

  /* private helper variables and stuff*/
  namespace
    {
      /* Variables */
      int8_t _sizePrecission = 3;

      /* Values will be fetched from a variable if calls are too close in time */
      std::chrono::steady_clock::duration _valueDuration = std::chrono::milliseconds(300);
      std::chrono::steady_clock::duration _mountWaiting = std::chrono::milliseconds(1000);
      unsigned _valueCheckInterval = 200;

      std::chrono::steady_clock::duration proccessSummaryRebuild = std::chrono::milliseconds(1500);
      unsigned char lastProcessUpdate=0;

      /* Rewriting try_lock_for method because of bugs in some compilers (gcc versions).
         It does not wait for rel_time, just returns false. */
      /* It can be a little quick and dirty but as a provisional fix, it seems to work */
      class timed_mutex : public std::timed_mutex
      {
      public:
	template <class Rep, class Period>
	bool try_lock_for(const std::chrono::duration<Rep,Period>& rel_time)
	{
	  bool ok = false;
	  unsigned count=0;
	  std::chrono::duration<Rep,Period> sleep = rel_time/_valueCheckInterval;
	  while (!(ok = (this->try_lock())) && (count++<_valueCheckInterval))
	    {
	      std::this_thread::sleep_for(sleep);
	    }
	  return ok;
	}
      };

    };

  /* Sysload as unsigned long values */
  struct Sysloadsl
  {
    unsigned long one, five, fifteen;
  };

  /* Sysload as double values */
  struct Sysloads
  {
    double one, five, fifteen;
  };

  /* Mount points information */
  namespace Mounts
  {
    /* A simple mount point information, self-explanatory? */
    struct MountPoint
    {
      std::string fileSystem;
      std::string mountPoint;
      std::string type;
      std::string options;
      int dumpFrequency;
      int passNumber;
      long blockSize;
      unsigned long freeBlocks;
      unsigned long freeBlocksUU;	/* free blocks for unprivileged users */
      unsigned long totalBlocks;
      long maxNamelen;		        /* max name length */
      unsigned long fileNodes;
      unsigned long freeFileNodes;
      int statfs_errno;		        /* errno by statfs */

      /** Returns mount free space */
      unsigned long freeSpace()
      {
	return (statfs_errno!=0)?0:(freeBlocksUU * blockSize);
      }

      /** Returns free space for privileged users  */
      unsigned long freeSpacePriv()
      {
	return (statfs_errno!=0)?0:(freeBlocks * blockSize);
      }

      /** Returns total space */
      unsigned long totalSpace()
      {
	return (statfs_errno!=0)?0:(totalBlocks * blockSize);
      }

      /** Returns used space  */
      unsigned long usedSpace()
      {
	return (statfs_errno!=0)?0:((totalBlocks-freeBlocks) * blockSize);
      }

      /** Returns used disk ratio  */
      double usedRatio()
      {
	return ( (statfs_errno!=0) || (totalBlocks==0))?0:(1-(double)freeBlocksUU/totalBlocks);
      }
    };

    /** Vector with all mount points information  */
    typedef std::vector<MountPoint> MountPoints;
  };

  /* Some getters/setters */

  /** get size precission (to use with @see size() ) */
  static int8_t sizePrecission()
  {
    return _sizePrecission;
  }

  /** set size precission (to use with @see size() ) */
  static int8_t sizePrecission(int8_t val)
  {
    _sizePrecission = val;
    return _sizePrecission;
  }

  /* double value duration getter/s */
  static double valueDuration()
  {
    return std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1>>>(_valueDuration).count();
  }

  static double valueDuration(double val)
  {
    _valueDuration = std::chrono::milliseconds(static_cast<unsigned long>(val * 1000));
    return std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1>>>(_valueDuration).count();
  }

  /* double mount waiting getter/s */
  static double mountWaiting()
  {
    return std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1>>>(_mountWaiting).count();
  }

  static double mountWaiting(double val)
  {
    _mountWaiting = std::chrono::milliseconds(static_cast<unsigned long>(val * 1000));
    return std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1>>>(_mountWaiting).count();
  }

  /* value check interval getter/s */
  static unsigned valueCheckInterval()
  {
    return _valueCheckInterval;
  }

  static unsigned valueCheckInterval(unsigned val)
  {
    if (val > 0)
      _valueCheckInterval = val;

    return _valueCheckInterval;
  }

  /** Humanize size */
  static std::string size(long double size, int8_t precission=-1)
  {
    static const char* units[10]={"bytes","Kb","Mb","Gb","Tb","Pb","Eb","Zb","Yb","Bb"};
    /* Using a char instead of ostringstream precission because it's faster */
    char temp[32];
    char format[10];

    int i= 0;

    while (size>1024) {
      size = size /1024;
      i++;
    }
    if (precission < 0)
      precission=sizePrecission();

    snprintf(format, 10, "%%.%dLf%%s", precission);
    snprintf(temp, 32, format, size, units[i]);

    return std::string(temp);
  }

  /** Extract a whole file into a string  */
  static std::string extractFile(const char *filename, size_t bufferSize=512)
  {
    int fd = open(filename, O_RDONLY);
    std::string output;

    if (fd==-1)
      return "";		/* error opening */

    char *buffer = (char*)malloc(bufferSize);
    if (buffer==NULL)
      return "";		/* Can't allocate memory */

    int datalength;
    while ((datalength = read(fd, buffer, bufferSize)) > 0)
      output.append(buffer, datalength);

    close(fd);
    return output;
  }

  /* Linux specific routines */
  /** get basic sysinfo (ram, swap, uptime, sysload...)  */
  static struct sysinfo getSysInfo(bool reload=false)
  {
    static struct sysinfo _sysinfo;		/* sysinfo cached value */
    static std::chrono::steady_clock::time_point _sysinfo_tp; /* last sysinfo fetched */

    if ( (reload) || (_sysinfo_tp+_valueDuration < std::chrono::steady_clock::now()) )
      {
	sysinfo(&_sysinfo);
      	_sysinfo_tp = std::chrono::steady_clock::now();
      }

    return _sysinfo;
  }

  /* Memory related */
  /** gets total ram  */
  static unsigned long totalram()
  {
    auto si = getSysInfo();
    return si.totalram * si.mem_unit;
  }

  /** gets free ram  */
  static unsigned long freeram()
  {
    auto si = getSysInfo();
    return si.freeram * si.mem_unit;
  }

  /** gets used ram  */
  static unsigned long usedram()
  {
    auto si = getSysInfo();
    return ( si.totalram - si.freeram )* si.mem_unit;
  }

  /** gets total used ram (used and buffers) */
  static unsigned long usedramb()
  {
    auto si = getSysInfo();
    return ( si.totalram - si.freeram - si.bufferram )* si.mem_unit;
  }

  /** gets shared ram  */
  static unsigned long sharedram()
  {
    auto si = getSysInfo();
    return si.sharedram * si.mem_unit;
  }

  /** gets buffer ram  */
  static unsigned long bufferram()
  {
    auto si = getSysInfo();
    return si.bufferram * si.mem_unit;
  }

  /** gets total swap  */
  static unsigned long totalswap()
  {
    auto si = getSysInfo();
    return si.totalswap * si.mem_unit;
  }

  /** gets free swap space  */
  static unsigned long freeswap()
  {
    auto si = getSysInfo();
    return si.freeswap * si.mem_unit;
  }

  /** gets used swap */
  static unsigned long usedswap()
  {
    auto si = getSysInfo();
    return ( si.totalswap - si.freeswap ) * si.mem_unit;
  }

  /* We will get high mem usually with PAE in x86 with >4Gb of RAM */
  /** gets total high mem  */
  static unsigned long totalHighMem()
  {
    auto si = getSysInfo();
    return si.totalhigh * si.mem_unit;
  }

  /** gets free high mem  */
  static unsigned long freeHighMem()
  {
    auto si = getSysInfo();
    return si.freehigh * si.mem_unit;
  }

  /** gets memory unit size  */
  static unsigned int memoryUnitSize()
  {
    return getSysInfo().mem_unit;
  }

  /** gets uptime via sysinfo or via /proc/uptime (more portable to other unixes)  */
  static long uptime()
  {
    long tmp = getSysInfo().uptime;
    if (!tmp)
      {
	/* Extract uptime the old way */
	std::string data = extractFile("/proc/uptime", 32);
	double rawdata;
	sscanf(data.c_str(), "%lf", &rawdata);
	tmp = (long)rawdata;
      }
    return tmp;
  }

  /* Sysload as unsigned long */
  /** gets sysload in struct  */
  static Sysloadsl sysloadu()
  {
    auto sy = getSysInfo();
    return {sy.loads[0], sy.loads[1], sy.loads[2]};
  }

  /** gets sysload in 1 min  */
  static unsigned long sysload1u()
  {
    return getSysInfo().loads[0];
  }

  /** gets sysload in 5min  */
  static unsigned long sysload5u()
  {
    return getSysInfo().loads[1];
  }

  /** gets sysload in 15min  */
  static unsigned long sysload15u()
  {
    return getSysInfo().loads[2];
  }

  /** Sysload as double values (as $ uptime does) */
  static Sysloads sysload()
  {
    auto sy = getSysInfo();
    return {sy.loads[0] / float(1 << SI_LOAD_SHIFT), sy.loads[1] / float(1 << SI_LOAD_SHIFT), sy.loads[2] / float(1 << SI_LOAD_SHIFT)};
  }

  /** gets sysload in 1 min (double) */
  static double sysload1()
  {
    return getSysInfo().loads[0] / float(1 << SI_LOAD_SHIFT);
  }

  /** gets sysload in 5 min (double) */
  static double sysload5()
  {
    return getSysInfo().loads[1] / float(1 << SI_LOAD_SHIFT);
  }

  /** gets sysload in 15 min (double) */
  static double sysload15()
  {
    return getSysInfo().loads[2] / float(1 << SI_LOAD_SHIFT);
  }

  /** Get total proccesses. It shows current total number of THREADS in our system,
   but sysinfo() says it show processes. */
  static unsigned short totalThreads()
  {
    return getSysInfo().procs;
  }

  /* System configuration */
  /** gets max arguments length  */
  static long maxArgumentsLength()
  {
    return sysconf(_SC_ARG_MAX);
  }

  /** gets max processes per user  */
  static long maxProcessesPerUser()
  {
    return sysconf(_SC_CHILD_MAX);
  }

  /** gets clock ticks per second  */
  static long ticksPerSecond()
  {
    return sysconf(_SC_CLK_TCK);
  }

  /** gets max opened files  */
  static long maxOpenedFiles()
  {
    return sysconf(_SC_OPEN_MAX);
  }

  /** get page size. Page size doesn't change, so it's calculated once */
  static long pageSize()
  {
    static long psize = sysconf(_SC_PAGESIZE);
    return psize;
  }

  /* These functions are not using POSIX standard */
  /** gets total RAM pages  */
  static long totalPagesRam()
  {
    return sysconf(_SC_PHYS_PAGES);
  }

  /** gets available RAM pages  */
  static long availablePagesRam()
  {
    return sysconf(_SC_AVPHYS_PAGES);
  }

  /** gets number of cpus (or cores)  */
  static unsigned cpuCount()
  {
    return sysconf(_SC_NPROCESSORS_CONF);
  }

  /** gets number of online cpus (or cores)  */
  static unsigned onlineCpuCount()
  {
    return sysconf(_SC_NPROCESSORS_ONLN);
  }

  /** Mount point related stuff  */
  namespace Mounts
  {
    /** Gets all mount points information  */
    static MountPoints mountsInfo(bool reload=false)
    {
      static MountPoints res;
      static std::chrono::steady_clock::time_point _mpinfo_tp; /* last mounts info fetched */

      if ( (!reload) && (_mpinfo_tp+_valueDuration >= std::chrono::steady_clock::now()) )
	return res;

      struct mntent *ent;
      struct statfs sfs;
      FILE *fd = setmntent("/etc/mtab", "r");

      if (fd == NULL)
	return res;

      while ( (ent = getmntent(fd)) != NULL)
	{
	  /* c++11 futures can't be cancelled and this could cause memory corruption if
	     statfs hangs. We must cancel it.*/
	  /* c++11 threads also can't be cancelled. But as this lib is being used with
	     POSIX threads, we can calcel them by hand. Quick and dirty, but it's the best
	     we have. */
	  /* Podemos hacer una función con esto */
	  MountPoint *mp = new MountPoint({ent->mnt_fsname, 
		ent->mnt_dir,
		ent->mnt_type,
		ent->mnt_opts,
		ent->mnt_freq,
		ent->mnt_passno,
		0,0,0,0,0,0,0,
		-1	/* Not ready yet */
		});
	  timed_mutex sfsmutex;
	  std::mutex mpmutex;

	  sfsmutex.lock();
	  std::thread sfsthread([&]() {
	      struct statfs sfs;
	      errno=0;
	      int err = statfs(mp->mountPoint.c_str(), &sfs);

	      mpmutex.lock();
	      mp->blockSize = sfs.f_bsize;
	      mp->freeBlocks = sfs.f_bfree;
	      mp->freeBlocksUU =sfs.f_bavail;
	      mp->totalBlocks = sfs.f_blocks;
	      mp->maxNamelen = sfs.f_namelen;
	      mp->fileNodes = sfs.f_files;
	      mp->freeFileNodes = sfs.f_ffree;
	      mp->statfs_errno=(err<0)?errno:0;
	      mpmutex.unlock();
	      sfsmutex.unlock();
	    });
	  sfsthread.detach();

	  if (!sfsmutex.try_lock_for(_mountWaiting))
	    {
	      pthread_cancel(sfsthread.native_handle());
	    }
	  else
	    sfsmutex.unlock();

	  mpmutex.lock();
	  res.push_back(*mp);
	  mpmutex.unlock();
	}

      _mpinfo_tp = std::chrono::steady_clock::now();

      return res;
    }

    /** Get free space in mountpoint */
    static long getFreeSpace(std::string name)
    {
      auto mps = mountsInfo();
      for (auto m : mps)
	{
	  if ( (m.fileSystem == name) || (m.mountPoint == name) )
	    return m.freeSpace();
	}
      return 0;
    }

    /** Get total size of mount point */
    static long getTotalSpace(std::string name)
    {
      auto mps = mountsInfo();
      for (auto m : mps)
	{
	  if ( (m.fileSystem == name) || (m.mountPoint == name) )
	    return m.totalSpace();
	}
      return 0;
    }

    /** Get used space in mount point*/
    static long getUsedSpace(std::string name)
    {
      auto mps = mountsInfo();
      for (auto m : mps)
	{
	  if ( (m.fileSystem == name) || (m.mountPoint == name) )
	    return m.usedSpace();
	}
      return 0;
    }

    /** Get used space ratio in mount point*/
    static double getUsedRatio(std::string name)
    {
      auto mps = mountsInfo();
      for (auto m : mps)
	{
	  if ( (m.fileSystem == name) || (m.mountPoint == name) )
	    return m.usedRatio();
	}
      return 0;
    }

    /** Gets mount point's type  */
    static std::string getType(std::string name)
      {
	auto mps = mountsInfo();
	for (auto m : mps)
	  {
	    if ( (m.fileSystem == name) || (m.mountPoint == name) )
	      return m.type;
	  }

	return "error";
      }
  };

  /** Process related stuff  */
  namespace Proc
  {
    /** Process user visible to the user */
    struct SingleProc
    {
      std::string name;
      char 
      state;
      int error,
	pid,
	ppid,
	pgrp,
	session,
	tty;
      double
      pcpu,
	totalpcpu;
      unsigned long
      flags,
	vsize;
      unsigned long long
      start_time;
      long
      priority,
	nice,
	rss;
    };

    /** Used when returning all processes with given name  */
    struct MultiProc
    {
      std::string name;
      double
      pcpu,			/* Process instant cpu time */
	totalpcpu;		/* Total cpu time from the beginning */
      unsigned long long
      totalvsize;
      long
	totalrss;
      unsigned
      count;			/* Number of processes with this name */
      std::map <unsigned, SingleProc> processes; /* SingleProc with all processes */
    };
  };

  /** Private processes stuff  */
  namespace
    {
      /** Used internally, lot of information about process.  */
      struct proc_t 
      {
	char
	state,
	  name[32];
	unsigned char
 	updated,
	  newproc;			/* flag this process as updated */
	int
	error,			/* error reading anything */
	  pid,
	  ppid,
	  pgrp, 
	  session, 
	  tty,
	  tpgid,
	  nlwp;
	double
	pcpu,
	  totalpcpu;

	unsigned long
	flags, 
	  min_flt, 
	  cmin_flt,
	  maj_flt, 
	  cmaj_flt,
	  vsize;

	unsigned long long
	utime,
	  stime,
	  cutime,
	  cstime,
	  start_time,
	  oldtime;

	long
	priority,
	  nice,
	  alarm,
	  rss;
      };

      /** Processes information struct (internal use)  */
      struct
      {
	std::chrono::steady_clock::duration generationTime;

	std::map<std::string, Proc::MultiProc> advanced;
	std::map<unsigned, proc_t*> processes;
      } ProcessSummary;

      /** Fill in process struct with useful information. Even calculate %CPU from 
       last call it there have been enough time between calls. */
      bool createProcessSummary(char *procId, double timeFromLast, unsigned char update)
      {
	char filename[32];
	snprintf(filename, 32, "/proc/%s/stat", procId);
	std::string proc = extractFile(filename);
	unsigned num;
	short namelen;
	char* tmp = (char*)proc.c_str();
	unsigned long long procuptime, total_time;
	proc_t* P=ProcessSummary.processes[atoi(procId)];
	if (P == NULL)
	  {
	    P = (proc_t*)malloc(sizeof(proc_t));
	    P->newproc = 1;
	    P-> oldtime = 0;
	  }
	else
	  P->newproc = 0;

	sscanf(tmp, "%d", &P->pid);
	char *pstart=strchr(tmp, '('), *pend=strchr(tmp,')');
	if ( (pstart==NULL) || (pend ==NULL) )
	  {
	    P->error=1;		/* we shouldn't see this here. It could be a kernel error */
	  }
	else
	  {
	    size_t namesize = (pend-pstart<33)?pend-pstart-1:32;
	    strncpy(P->name, pstart+1, namesize);
	    P->name[namesize] = '\0';
	    /* Borrowed from readproc from procps */
	    num = sscanf(pend+2,
			 "%c "
			 "%d %d %d %d %d "
			 "%lu %lu %lu %lu %lu "
			 "%Lu %Lu %Lu %Lu "  /* utime stime cutime cstime */
			 "%ld %ld "
			 "%d "
			 "%ld "
			 "%Lu "  /* start_time */
			 "%lu "  /* Vsize */
			 "%ld ", /* Resident size */
			 &P->state,
			 &P->ppid, &P->pgrp, &P->session, &P->tty, &P->tpgid,
			 &P->flags, &P->min_flt, &P->cmin_flt, &P->maj_flt, &P->cmaj_flt,
			 &P->utime, &P->stime, &P->cutime, &P->cstime,
			 &P->priority, &P->nice,
			 &P->nlwp,
			 &P->alarm,
			 &P->start_time,
			 &P->vsize,
			 &P->rss
			 );

	    procuptime = uptime() - (P->start_time) / 100;
	    total_time =  P->utime + P->stime; 
	    P->totalpcpu = (procuptime>0)?((double)total_time / procuptime):0;
	    if ( (timeFromLast) && (P->oldtime) )
	      {
		P->pcpu=((double)total_time - (double)P->oldtime) / timeFromLast ;
	      }
	    else
	      P->pcpu=0;

	    P->oldtime = total_time;
	    P->updated = update;
	    ProcessSummary.processes[P->pid] = P;
	  }
	//	std::cout << "PID: "<<P->pid<<" - "<<P->name<<"** "<<total_time<<" "<<procuptime<<" "<<P->totalpcpu<<"% Intervalo: "<<P->pcpu<<" **"<<std::endl;
	return true;
      }

      /** Iterate over all processes and call a function with each one.
       I think it could be useful it we have more things to do with processes
      not just createProcessSummary() */
      void walkProcesses(std::function<bool(char*)> f)
      {
	DIR* proc_dir;
	direct *ent;

	proc_dir = opendir("/proc");
	while ((ent = readdir(proc_dir)))
	  {
	    if ((*ent->d_name>'0') && (*ent->d_name<='9')) /* Be sure it's a pid */
	      {
		/* snprintf(filename, 32, "/proc/%s/stat", ent->d_name); */
		if (!f(ent->d_name))
		  break;
	      }
	  }
	closedir(proc_dir);
      }

      /** Cleanup processes not seen in a while (finished processes)  */
      void processedCleanup()
      {
	for (auto i=ProcessSummary.processes.rbegin(); i!=ProcessSummary.processes.rend(); ++i)
	  if (i->second->updated < lastProcessUpdate-1) /* No cleanup of just dead processes */
	    ProcessSummary.processes.erase(std::next(i).base());
	/* std::cout << "REMOVE: "<<i->second->pid<<std::endl; */
      }
    };

  /** Processes public functions  */
 namespace Proc
 {
   /** build process internal summary. Many functions will build the entire process summary
       as it isn't too expensive, save programming time and these functions won't be used just
       once for a single process. My intention is to iterate over processes each time. */
   static void buildProcSummary(bool reload=false)
   {
     static std::chrono::steady_clock::time_point _procsum_tp; /* last sysinfo fetched */
     auto now = std::chrono::steady_clock::now();
     if ( (reload) || (_procsum_tp+proccessSummaryRebuild < now) )
       {
	 double elapsedTime = std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1>>>(now-_procsum_tp).count();
	 walkProcesses(std::bind(createProcessSummary, std::placeholders::_1, elapsedTime, ++lastProcessUpdate));
	 processedCleanup();
	 _procsum_tp = std::chrono::steady_clock::now();
       }

     ProcessSummary.generationTime = (std::chrono::steady_clock::now() - now);
   }

   /** build advanced process summary. Automatically calls buildProcSummary()  */
   static void buildAdvancedSummary(bool reload=false)
   {
     buildProcSummary(reload);
     static std::chrono::steady_clock::time_point _procsum_tp; /* last sysinfo fetched */
     auto now = std::chrono::steady_clock::now();
     if ( (reload) || (_procsum_tp+proccessSummaryRebuild < now) )
       {
	 ProcessSummary.advanced.clear();
	 for (auto p : ProcessSummary.processes)
	   {
	     auto _p = p.second;
	     SingleProc sp({_p->name, _p->state, _p->error, _p->pid,
		   _p->ppid, _p->pgrp,      _p->session, _p->tty,
		   _p->pcpu, _p->totalpcpu, _p->flags,   _p->vsize,
		   _p->start_time, _p->priority, _p->nice, _p->rss});
	     auto item = ProcessSummary.advanced.find(_p->name);
	     if (item == ProcessSummary.advanced.end())
	       {
		 MultiProc mp({_p->name, _p->pcpu, _p->totalpcpu, _p->vsize,
		       _p->rss, 1});
		 mp.processes[_p->pid] = sp;
		 ProcessSummary.advanced[_p->name] = mp;
	       }
	     else
	       {
		 item->second.pcpu+=_p->pcpu;
		 item->second.totalpcpu+=_p->totalpcpu;
		 item->second.totalvsize+=_p->vsize;
		 item->second.totalrss+=_p->rss;
		 ++item->second.count;
		 item->second.processes[_p->pid] = sp;
	       }
	   }
       }
   }

   /** Returns time taken to build the summary  */
   static double timeToBuildSummary()
   {
     return std::chrono::duration_cast<std::chrono::duration<double,std::ratio<1>>>(ProcessSummary.generationTime).count();
   }

   /** another process count, this time with our process map  */
   static unsigned processCount()
   {
     buildProcSummary();
     return ProcessSummary.processes.size();
   }

   /** count processes with given name  */
   static unsigned countProcess(std::string name)
   {
     buildProcSummary();
     buildAdvancedSummary();

     auto it = ProcessSummary.advanced.find(name);
     if (it == ProcessSummary.advanced.end())
       return 0;
     return it->second.count;
   }

   /** total %CPU by a given process. If allTime is true, the % will be
       calculated from the process start. If not, it will be calculated from
       the last call to any process function (as it uses buildProcSummary). */
   static double totalPCPU(std::string name, bool allTime=false)
   {
     buildProcSummary();
     buildAdvancedSummary();

     auto it = ProcessSummary.advanced.find(name);
     if (it == ProcessSummary.advanced.end())
       return 0;

     return (allTime)?it->second.totalpcpu:it->second.pcpu;
   }

   /** Get all processes with a given name in a MultiProc  */
   static MultiProc getByName(std::string name)
   {
     buildAdvancedSummary();
     return ProcessSummary.advanced[name];
   }

   /** Get all processes information */
   static std::map<unsigned, SingleProc> getAllProcs()
   {
     std::map<unsigned, SingleProc> result;
     buildProcSummary();

     for (auto p : ProcessSummary.processes)
       {
	 auto _p = p.second;
	 SingleProc sp({_p->name, _p->state, _p->error, _p->pid,
	       _p->ppid, _p->pgrp,      _p->session, _p->tty,
	       _p->pcpu, _p->totalpcpu, _p->flags,   _p->vsize,
	       _p->start_time, _p->priority, _p->nice, _p->rss});
	 result[_p->pid] = sp;
       }
     return result;
   }

   /** Gets all process over a %CPU threshold  */
   static std::vector<SingleProc> getByPCPU(double threshold, bool allTime=false)
   {
     std::vector<SingleProc> result;
     buildProcSummary();

     for (auto p : ProcessSummary.processes)
       {
	 auto _p = p.second;
	 SingleProc sp({_p->name, _p->state, _p->error, _p->pid,
	       _p->ppid, _p->pgrp,      _p->session, _p->tty,
	       _p->pcpu, _p->totalpcpu, _p->flags,   _p->vsize,
	       _p->start_time, _p->priority, _p->nice, _p->rss});
	 if (allTime)
	   {
	     if (_p->totalpcpu >= threshold)
	       result.push_back(sp);
	   }
	 else
	   {
	     if (_p->pcpu >= threshold)
	       result.push_back(sp);
	   }
       }

     return result;
   }

   /** Gets all process over a %CPU threshold (counting all processes with the same name)  */
   static std::map<std::string, MultiProc> getByPCPUCol(double threshold, bool allTime=false)
   {
     std::map<std::string, MultiProc> result;
     buildAdvancedSummary();

     for (auto p : ProcessSummary.advanced)
       {
	 auto _p = p.second;

	 if (allTime)
	   {
	     if (_p.totalpcpu >=threshold)
	       result[p.first] = p.second;
	   }
	 else
	   {
	     if (_p.pcpu >=threshold)
	       result[p.first] = p.second;
	   }
       }

     return result;
   }

   /** Gets all process over a Vsize threshold */
   static std::vector<SingleProc> getByVsize(unsigned long threshold)
   {
     std::vector<SingleProc> result;
     buildProcSummary();

     for (auto p : ProcessSummary.processes)
       {
	 auto _p = p.second;
	 SingleProc sp({_p->name, _p->state, _p->error, _p->pid,
	       _p->ppid, _p->pgrp,      _p->session, _p->tty,
	       _p->pcpu, _p->totalpcpu, _p->flags,   _p->vsize,
	       _p->start_time, _p->priority, _p->nice, _p->rss});

	 if (_p->vsize >= threshold)
	   result.push_back(sp);
       }

     return result;
   }

   /** Gets all process over a Vsize threshold (counting all processes with the same name) */
   static std::map<std::string, MultiProc> getByVsizeCol(unsigned long long threshold)
   {
     std::map<std::string, MultiProc> result;
     buildAdvancedSummary();

     for (auto p : ProcessSummary.advanced)
       {
	 auto _p = p.second;

	 if (_p.totalvsize >= threshold)
	   result[p.first] = p.second;
       }

     return result;
   }

 };
};


#endif /* _UMON_H */

