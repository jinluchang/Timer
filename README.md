#Timer

A simple but powerful C++ code performance measurement library which is contained in a single header file.

Simple example within the timer.h file

```
inline void* timer_malloc(size_t size) {
  TIMER_FLOPS("timer_malloc");
  timer.flops += size;
  void* p = malloc(size);
  memset(p, 0, size);
  return p;
}

inline void timer_free(void* ptr) {
  TIMER("timer_free");
  free(ptr);
}

inline void tmalloc(size_t size)
{
  timer_malloc(size);
}

inline void tfree(void* ptr)
{
  timer_free(ptr);
}
```

To show the summary
```
Timer::display()
```

