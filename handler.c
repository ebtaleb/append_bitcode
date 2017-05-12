#include <stdlib.h>
#include <signal.h>

void gdb(int sig) {
  system("exec terminal -e lldb -p \"$PPID\"");
  abort();
}

void _init() {
  signal(SIGSEGV, gdb);
  signal(SIGABRT, gdb);
}
