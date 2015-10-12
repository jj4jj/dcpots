#pragma once

//--------------------std c ------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <strings.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <setjmp.h>
#include <signal.h>
#include <assert.h>
//-------------------std cpp--------------------------

#include <string>
using std::string;
#include <vector>
using std::vector;
#include <map>
using std::map;
#include <set>
using std::set;
#include <algorithm>
#include <multimap>
#include <multiset>
#include <list>
#include <stack>
#include <iterator>
#include <bitset>
#include <stack>



/////////////////////////cxx new std/////////////////////
#include <memory>
using std::shared_ptr;
using std::make_shared;
#include <functional>
#include <unordered_map>
#include <unordered_set>
using std::unordered_map;
using std::unordered_set;
#include <random>

#include <bitset>
using std::bitset;
//#include <iostream>

//-------------------------------------------------
///////////////////////////////////////////////////////
#ifndef gettid
#include <sys/syscall.h>
#define gettid() syscall(__NR_gettid)
#endif

//------------------linux system call -------------
#include <unistd.h>
#include <dirent.h>

#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/syscall.h>

#include <arpa/inet.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <linux/un.h>
#include <sys/mman.h>

//thread
#include <pthread.h>
#include <semaphore.h>


//////////////////////////////////////////////////////
#ifndef restrict
#define restrict 
#endif

