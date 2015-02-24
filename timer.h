// vim: set ts=2 sw=2 expandtab:

// Copyright (c) 2014 Luchang Jin
// All rights reserved.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef INCLUDED_TIMER_H
#define INCLUDED_TIMER_H

#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef USE_PAPI
#include <papi.h>
#include <omp.h>
#endif

#if defined USE_MPI || defined USE_QMP
#define USE_MULTI_NODE
#endif

#ifdef USE_MULTI_NODE
#include <mpi.h>
#endif

inline double getTime() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return((double)tp.tv_sec + (double)tp.tv_usec * 1e-6);
}

inline int getRank() {
#ifdef USE_MULTI_NODE
  int myid;
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  return myid;
#else
  return 0;
#endif
}

inline double getStartTime() {
  static double time = getTime();
  return time;
}

inline double getTotalTime() {
  return getTime() - getStartTime();
}

inline void DisplayInfo(const char *cname, const char *fname, const char *format, ...) {
  if (0 != getRank()) {
    return;
  }
  const int max_len = 2048;
  char str[max_len];
  va_list args;
  va_start(args, format);
  vsnprintf(str, max_len, format, args);
  printf("%s::%s : %s", cname, fname, str);
}

inline long long getTotalFlops() {
  long long flops = 0;
#ifdef USE_PAPI
  const int n_threads = omp_get_max_threads();
  long long flopses[n_threads];
  memset(flopses, 0, n_threads * sizeof(long long));
#pragma omp parallel
  {
    float rtime, ptime, mflops;
    int i = omp_get_thread_num();
    PAPI_flops(&rtime, &ptime, &flopses[i], &mflops);
  }
  for (int i = 0; i < n_threads; i++) {
    flops += flopses[i];
  }
#endif
  return flops;
}

inline void initializePAPI() {
#ifdef USE_PAPI
  static bool initialized = false;
  if (initialized) {
    return;
  }
  DisplayInfo("PAPI", "initializePAPI", "Start.\n");
  PAPI_library_init(PAPI_VER_CURRENT);
  PAPI_thread_init((unsigned long (*)(void)) (omp_get_thread_num));
  initialized = true;
  DisplayInfo("PAPI", "initializePAPI", "Finish.\n");
#endif
}

class TimerInfo {
  public:
    std::string fname;
    double dtime;
    double accumulated_time;
    long long dflops;
    long long accumulated_flops;
    int call_times;
    //
    TimerInfo() {
      fname = "Unknown";
      dtime = 0.0 / 0.0;
      accumulated_time = 0;
      dflops = 0;
      accumulated_flops = 0;
      call_times = 0;
    }
    //
    void showLast(const char *info = NULL) {
      double total_time = getTotalTime();
      std::string fnameCut;
      fnameCut.assign(fname, 0, 30);
      DisplayInfo("Timer", NULL == info ? "" : info,
          "%30s :%5.1f%%%9d calls. Last %.3E secs%8.3f Gflops (%.3E per call)\n",
          fnameCut.c_str(),
          accumulated_time / total_time * 100, call_times,
          dtime,
          dflops / dtime / 1.0E9,
          (double)dflops);
    }
    //
    void showAvg(const char *info = NULL) {
      double total_time = getTotalTime();
      std::string fnameCut;
      fnameCut.assign(fname, 0, 30);
      DisplayInfo("Timer", NULL == info ? "" : info,
          "%30s :%7.3f%%%9d calls. Avg %.2E(%.2E) secs%6.2f Gflops (%.2E(%.2E)flops)\n",
          fnameCut.c_str(),
          accumulated_time / total_time * 100, call_times,
          accumulated_time / call_times,
          accumulated_time,
          accumulated_flops / accumulated_time / 1.0E9,
          (double)accumulated_flops / (double)call_times,
          (double)accumulated_flops);
    }
    //
    void show(const char *info = NULL) {
      showAvg(info);
    }
};

inline bool compareTimeInfoP(TimerInfo *p1, TimerInfo *p2) {
  return p1->accumulated_time < p2->accumulated_time;
}

class Timer {
  public:
    const char *cname;
    int index;
    TimerInfo info;
    bool isUsingTotalFlops;
    bool isRunning;
    double start_time;
    double stop_time;
    long long start_flops;
    long long stop_flops;
    long long flops;
    //
    static std::vector<TimerInfo *>& getTimerDatabase() {
      static std::vector<TimerInfo *> timerDatabase;
      return timerDatabase;
    }
    //
    static double& minimum_autodisplay_interval() {
      static double time = 60;
      return time;
    }
    //
    static double& minimum_duration_for_show_stop_info() {
      static double time = 0.1;
      return time;
    }
    //
    static double& minimum_duration_for_show_start_info() {
      static double time = 0.1;
      return time;
    }
    //
    Timer() {
      init();
    }
    Timer(const std::string& fname_str) {
      init();
      init(fname_str);
    }
    Timer(const std::string& cname_str, const std::string& fname_str) {
      init();
      init(cname_str, fname_str);
    }
    Timer(const std::string& fname_str, const bool isUsingTotalFlops_) {
      init();
      init(fname_str);
      isUsingTotalFlops = isUsingTotalFlops_;
    }
    //
    void init() {
      cname = "Timer";
      isUsingTotalFlops = true;
      getStartTime();
      initializePAPI();
      index = getTimerDatabase().size();
      getTimerDatabase().push_back(&info);
      isRunning = false;
    }
    void init(const std::string& fname_str) {
      info.fname = fname_str;
    }
    void init(const std::string& cname_str, const std::string& fname_str) {
      info.fname = cname_str;
      info.fname += "::";
      info.fname += fname_str;
    }
    //
    void start(bool verbose = false) {
      if (isRunning) {
        return;
      } else {
        isRunning = true;
      }
      info.call_times++;
      if (verbose || info.call_times == 1 || info.dtime >= minimum_duration_for_show_start_info()) {
        info.showLast("start");
      }
      start_flops = isUsingTotalFlops ? getTotalFlops() : 0 ;
      flops = 0;
      start_time = getTime();
    }
    //
    void stop(bool verbose = false) {
      stop_time = getTime();
      assert(isRunning);
      if (0 == flops && isUsingTotalFlops) {
        stop_flops = getTotalFlops();
      } else {
        stop_flops = start_flops + flops;
      }
      info.dtime = stop_time - start_time;
      info.dflops = stop_flops - start_flops;
      info.accumulated_time += info.dtime;
      info.accumulated_flops += info.dflops;
      if (verbose || info.call_times == 1 || info.dtime >= minimum_duration_for_show_stop_info()) {
        info.showLast("stop ");
      }
      autodisplay(stop_time);
      isRunning = false;
    }
    //
    static void display(const std::string& str = "") {
      static Timer time("Timer");
      static Timer time_noflop("Timer-noflop");
      static Timer time_test("Timer-test");
      time.isUsingTotalFlops = false;
      time_noflop.isUsingTotalFlops = false;
      time_test.isUsingTotalFlops = false;
      time_test.start();
      time_test.stop();
      time.start();
      time_test.isUsingTotalFlops = true;
      time_test.start();
      time_test.stop();
      time.stop();
      time_noflop.start();
      time_test.isUsingTotalFlops = false;
      time_test.start();
      time_test.stop();
      time_noflop.stop();
      double total_time = getTotalTime();
      std::vector<TimerInfo *> db(getTimerDatabase());
      std::sort(db.begin(), db.end(), compareTimeInfoP);
      DisplayInfo("Timer", "display-start", "%s ------------ total %.4e secs -----------------------\n", str.c_str(), total_time);
      for (int i = 0; i < db.size(); i++) {
        db[i]->showAvg("display");
      }
      DisplayInfo("Timer", "display-end  ", "%s ------------ total %.4e secs -----------------------\n", str.c_str(), total_time);
    }
    //
    static void autodisplay(const double time) {
      static double last_time = getTime();
      if (time - last_time > minimum_autodisplay_interval()) {
        last_time = time;
        display("autodisplay");
      }
    }
    static void autodisplay() {
      const double time = getTime();
      autodisplay(time);
    }
};

class TimerCtrl {
  public:
    Timer* ptimer;
    bool verbose;
    //
    TimerCtrl() {
    }
    TimerCtrl(Timer& timer, bool verbose_ = false) {
      init(timer, verbose_);
    }
    //
    ~TimerCtrl() {
      ptimer->stop(verbose);
    }
    //
    void init(Timer& timer, bool verbose_ = false) {
      ptimer = &timer;
      verbose = verbose_;
      ptimer->start(verbose);
    }
};

#define TIMER(FNAME) \
  static const char* fname = FNAME; \
  static Timer timer(fname); \
  TimerCtrl timerctrl(timer);

#define TIMER_FLOPS(FNAME, FLOPS) \
  static const char* fname = FNAME; \
  static Timer timer(fname, false); \
  TimerCtrl timerctrl(timer); \
  timer.flops += FLOPS;

#define TIMER_VERBOSE(FNAME) \
  static const char* fname = FNAME; \
  static Timer timer(fname); \
  TimerCtrl timerctrl(timer, true);

inline void* time_malloc(size_t size) {
  TIMER_FLOPS("time_malloc", size);
  void* p = malloc(size);
  memset(p, 0, size);
  return p;
}

inline void time_free(void* ptr) {
  TIMER("time_free");
  free(ptr);
}

#define malloc(x) time_malloc(x)

#define free(x) time_free(x)

#endif
