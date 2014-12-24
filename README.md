unixmon
=======

Extract UNIX system status information from a single C++11 header file

It's part of a bigger project, trying to monitor a Linux system without running lots
of shell scripts.

I tried to ask for information just when necessary, so it's safe to make lots of calls
to these functions, e.g:
   cout << Umon::sysload1u()<<" "<<Umon::sysload5u()<<" "<<Umon::sysload15u() << endl;
this generates just one call to sysinfo().

Some things you can find:

tools
-----
	- Umon::size(long double size[, int8_t precission]) : humanize size in bytes returning a
	  string with size in the biggest available unit. precission indicates the number of 
	  decimal digits to use.
	- Umon::sizePrecission([int8_t value]) : gets/sets size precission (to use with size())
	- Umon::extractFile(char *filename, size_t bufferSize=512) : reads a entire file into a
	  std::string
	- Umon::valueDuration([double seconds]) : gets/sets value duration. Almost all functions
	  will use a cache/stored value to serve data when calling them several times to avoid
	  calculating/generating all the info over and over again. The default value duration is
	  0.3 seconds (300millisec) so, you must wait for this time in order to generate again
	  sysinfo/sysconf/mounts/process data... but this functions will admit a "reload" argument
	  to force data recalculation.
	- Umon::mountWaiting([double seconds]) : gets/sets mount point checking timeout. It is useful
	  when checking network mounts that can be offline (nfs, samba, webdav...), so the filesystem
	  will wait a lot of time before timing out, and sometimes we don't have such time. So it
	  can be a value like 1 second or so.
	- Umon::valueCheckInterval([unsigned val]): In some compiler there's a bug when calling 
	  try_lock_for() method on timed_mutex(es), so here we have a derived class that checks
	  periodically the availability of the mutex. This value is the number of intervals the total
	  waiting time will be divided. (check About mount point summary).

sysinfo
-------
	- Umon::getSysinfo(reload) : direct call to sysinfo() and returns an struct sysinfo. If
	  this is called several times won't call sysinfo() again unless you specify reload=true
	- Umon::uptime() : Time since the machine was powered up. If not available
	  through sysinfo(), it tries reading /proc/uptime file which is more portable
	  through different unix systems.
	- Umon::totalThreads() : Gets total amount of threads running.

sysinfo (memory)
----------------
	- Umon::totalram() : total amount of RAM installed
	- Umon::freeram() : free RAM
	- Umon::usedram() : Used RAM ( total - free )
	- Umon::usedramb(): Total used RAM including buffers
	- Umon::sharedram() : Total shared RAM
	- Umon::bufferram() : RAM used in buffers
	- Umon::totalswap() : Total swap space
	- Umon::freeswap() : Free swap space
	- Umon::usedswap() : Used swap space ( total - free )
	- Umon::totalHighMem() : Total High Mem ( if your computer or device has memory divided. e.g: x86 with PAE and >4Gb)
	- Umon::freeHighmem() : Free High mem
	- Umon::memoryUnitSize() : Multiplier of all memory sysinfo() variables

sysinfo (system load)
-----------------
	- Umon::sysloadu() : Gets sysload as a struct Sysloadsl of unsigned long values
	  for one, five and fifteen minutes.
	- Umon::sysload1u() : Gets sysload as unsigned long in last minute
	- Umon::sysload5u() : Gets sysload as unsigned long in last 5 minutes.
	- Umon::sysload15u() : Gets sysload as unsigned long in last 15 minutes.
	- Umon::sysload() : Gets sysload as a struct Sysloads of double values.
	- Umon::sysload1() : Gets sysload as double in last minute
	- Umon::sysload5() : Gets sysload as double in last 5 minutes
	- Umon::sysload15() : Gets sysload as double in last 15 minutes

sysconf
-------
	- Umon::maxArgumentsLength() : Gets max arguments length for a program
	- Umon::maxProcessesPerUser() : Gets max amount of processes a user can run at a time
	- Umon::ticksPerSecond() : System's ticks per second
	- Umon::totalPagesRam() : Total RAM pages
	- Umon::availablePagesRam() : Available RAM pages
	- Umon::cpuCount() : Number of CPUs or cores installed
	- Umon::onlineCpuCount() : Number of CPU or cores currently online

Mounts
------
	- Umon::Mounts::mountsInfo([reload=false]) : Returns all mount points information
	- Umon::Mounts::getFreeSpace(name) : Gets free space on a mounted deviced specified
	  by its mount point or device.
	- Umon::Mounts::getTotalSpace(name) : Gets total space on a device
	- Umon::Mounts::getUsedSpace(name) : Gets used space on a device
	- Umon::Mounts::getUsedRatio(name) : Used space ration on a device
	- Umon::Mounts::getType(name) : Mount point type.

Proc
----
	- Umon::Proc::buildProcSummary([reload=false]): Build all processes summary to itearate over
	  it or to use access functions.
	- Umon::Proc::buildAdvancedSummary([reload=false]): Collects all processes and stores one entry
	  per process name (for example to list forks, or applications launched several times).
	- Umon::Proc::timeToBuildSummary() : Time taken to build last process summary
	- Umon::Proc::processCount() : Number of processes running now. Not the same as Umon::totalThreads()
	- Umon::Proc::countProcess(name) : Count number of processes with given name.
	- Umon::Proc::totalCPU(name, [allTime=false]) : Gives us the total %CPU of all processes with a given
	  name. e.g: calculating %CPU of apache2 or php-fpm, summing all instances of the application.
	  If the allTime argument is true the %CPU will be calculated from the beginning of the process.
	- Umon::Proc::getByName(name) : Gets all processes by a given name (maybe just one or zero). If it's zero,
	  an empty MultiProc struct will be returned.
	- Umon::Proc::getAllProcs() : Gives us a map with all processes information.
	- Umon::Proc::getByPCPU(threshold, [allTime=false]) : Gets a list of processes which CPU use is >= threshold.
	- Umon::Proc::getByPCPUCol(threshold, [allTime=false]) : Gets a list of processes collections which CPU use is
	  >= threshold. A processes collection is a set of processes with the same name. 
	- Umon::Proc::getByVsize(threshold) : Processes which vsize is >= threshold
	- Umon::Proc::getByVsizeCol(threshold) : Processes collection which vsize is >= threshold

About mount point summary
=========================
	I'm using multi-threads to get this to create a time out when getting mount point information. It has to do
	with network drives which can be offline or last too much in responding. I can't wait for 60 sec for it to 
	know if it's alive or not. It must be tested a little bit more...

	I could use future and promises present in C++11 but they don't have real timeout. I want to cancel
	the thread if it times out, and as it hangs I can't do anything. I cancel it with pthread_cancel(), I know it's
	not the best way but it just can't live.

	Note: I extended std::timed_mutex into timed_mutex class to provide a bug free try_lock_for() method. This method
	does not work properly in some GCC versions. It returns the function immediately without waiting for the mutex,
	and it really was a problem.

Some more notes
===============
	%CPU is calculated for periods of time, so the first time we build processes summary, they
	doesn't have instant %CPU, but they do have total %CPU (from the beginning of the process).
	If the sleep a time greater than valueDuration and build the summary again, we will have 
	instant %CPU values filled.

compilation
===========

	It was tested with GCC 4.7 and 4.8.2
	To compile it, you need pthread support in order to get mount points information. 

	To compile the example, just do:
	$ g++ -o sample01 sample01.cpp -std=c++11 -lpthread

to-do
=====
	This information will be included also in the header file.
	*  - Have process start time in chrono::time_point
	*  - Insert cmdline into process information
	*  - Insert OOM information into process struct
	*  - Process state constants
	*  - System instant %CPU
	*  - List processes by state
	*  - Folder size counter
	*  - Network interface detection
	*  - Temperature fetch