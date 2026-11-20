#include <actor/actor.h>
#include <gc/root_audit.h>
#include <sched/scheduler.h>

#include <gtest/gtest.h>
#include <cerrno>
#include <fcntl.h>
#ifdef PLATFORM_IS_WINDOWS
#include <io.h>
#include <winsock2.h>
#else
#include <unistd.h>
#endif

TEST(RootAudit, RefusesOtherActor)
{
  pony_ctx_t ctx{};
  pony_actor_t current{};
  pony_actor_t other{};
  ctx.current = &current;
  void* roots[] = {&other};
  const char* modes[] = {"receiver"};

  testing::internal::CaptureStderr();
  ponyint_gc_root_audit(&ctx, &other, "example", 1, 4, nullptr, 1, roots,
    modes);
  EXPECT_EQ("gc-root-audit example 1:4 refused receiver-mismatch receiver=0\n",
    testing::internal::GetCapturedStderr());
}

TEST(RootAudit, PreservesErrorOnFailedWrite)
{
  pony_ctx_t ctx{};
  pony_actor_t actor{};
  ctx.current = &actor;
  void* roots[] = {&actor};
  const char* modes[] = {"receiver"};

#ifdef PLATFORM_IS_WINDOWS
  int fd = _open("NUL", _O_RDONLY);
#else
  int fd = open("/dev/null", O_RDONLY);
#endif
  ASSERT_GE(fd, 0);
  testing::internal::CaptureStderr();
#ifdef PLATFORM_IS_WINDOWS
  int redirected = _dup2(fd, _fileno(stderr));
  _close(fd);
  _set_doserrno(ERROR_FILE_NOT_FOUND);
  WSASetLastError(WSAEWOULDBLOCK);
  SetLastError(WSAEWOULDBLOCK);
#else
  int redirected = dup2(fd, fileno(stderr));
  close(fd);
#endif
  errno = EDOM;
  ponyint_gc_root_audit(&ctx, &actor, "example", 1, 4, nullptr, 1, roots,
    modes);
#ifdef PLATFORM_IS_WINDOWS
  DWORD last_error = GetLastError();
  int socket_error = WSAGetLastError();
  unsigned long dos_error;
  _get_doserrno(&dos_error);
#endif
  int observed_errno = errno;
  int write_error = ferror(stderr);
  clearerr(stderr);
  testing::internal::GetCapturedStderr();

  EXPECT_GE(redirected, 0);
  EXPECT_NE(write_error, 0);
  EXPECT_EQ(observed_errno, EDOM);
#ifdef PLATFORM_IS_WINDOWS
  EXPECT_EQ(last_error, (DWORD)WSAEWOULDBLOCK);
  EXPECT_EQ(socket_error, WSAEWOULDBLOCK);
  EXPECT_EQ(dos_error, (unsigned long)ERROR_FILE_NOT_FOUND);
#endif
}
