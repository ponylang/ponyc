#ifndef PONYC_DRIVER_TEST_SIGNALS_H
#define PONYC_DRIVER_TEST_SIGNALS_H

#include <signal.h>
#include <string.h>

// Signal names as the test scripts write them, without the SIG prefix.
static const struct
{
  const char* name;
  int number;
} test_signals[] = {
  {"HUP", SIGHUP}, {"INT", SIGINT}, {"QUIT", SIGQUIT}, {"TERM", SIGTERM},
  {"PIPE", SIGPIPE}, {"USR1", SIGUSR1}, {"USR2", SIGUSR2}, {"KILL", SIGKILL},
  {"CHLD", SIGCHLD}
};

#define TEST_SIGNAL_COUNT (sizeof(test_signals) / sizeof(test_signals[0]))

static int test_signal_number(const char* name)
{
  for(size_t i = 0; i < TEST_SIGNAL_COUNT; i++)
  {
    if(strcmp(test_signals[i].name, name) == 0)
      return test_signals[i].number;
  }

  return -1;
}

static const char* test_signal_name(int sig)
{
  for(size_t i = 0; i < TEST_SIGNAL_COUNT; i++)
  {
    if(test_signals[i].number == sig)
      return test_signals[i].name;
  }

  return "OTHER";
}

#endif
