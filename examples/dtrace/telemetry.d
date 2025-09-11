#!/usr/bin/env dtrace -x aggsortkey -x aggsortkeypos=1 -s

#pragma D option quiet

inline unsigned int UINT32_MAX = 4294967295;
inline unsigned int ACTORMSG_APPLICATION_START = (UINT32_MAX - 9);
inline unsigned int ACTORMSG_CHECKBLOCKED = (UINT32_MAX - 8);
inline unsigned int ACTORMSG_ISBLOCKED = (UINT32_MAX - 7);
inline unsigned int ACTORMSG_BLOCK = (UINT32_MAX - 6);
inline unsigned int ACTORMSG_UNBLOCK = (UINT32_MAX - 5);
inline unsigned int ACTORMSG_ACQUIRE = (UINT32_MAX - 4);
inline unsigned int ACTORMSG_RELEASE = (UINT32_MAX - 3);
inline unsigned int ACTORMSG_CONF = (UINT32_MAX - 2);
inline unsigned int ACTORMSG_ACK = (UINT32_MAX - 1);

pony$target:::actor-msg-send
/ (unsigned int)arg1 <= (unsigned int)ACTORMSG_APPLICATION_START /
{
  @counts[arg0, "Application Messages Sent"] = count();
}

pony$target:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_CHECKBLOCKED /
{
  @counts[arg0, "Check Blocked Messages Sent"] = count();
}

pony$target:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_ISBLOCKED /
{
  @counts[arg0, "Is Blocked Messages Sent"] = count();
}

/ (unsigned int)arg1 == (unsigned int)ACTORMSG_BLOCK /
{
  @counts[arg0, "Block Messages Sent"] = count();
}

pony$target:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_UNBLOCK /
{
  @counts[arg0, "Unblock Messages Sent"] = count();
}

pony$target:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_ACQUIRE /
{
  @counts[arg0, "Acquire Messages Sent"] = count();
}

pony$target:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_RELEASE /
{
  @counts[arg0, "Release Messages Sent"] = count();
}

pony$target:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_CONF /
{
  @counts[arg0, "Confirmation Messages Sent"] = count();
}

pony$target:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_ACK /
{
  @counts[arg0, "Acknowledgement Messages Sent"] = count();
}

pony$target:::actor-alloc
{
  @counts[arg0, "Actor Allocations"] = count();
}

pony$target:::heap-alloc
{
  @counts[arg0, "Heap Allocations"] = count();
  @sizes[arg0, "Heap Allocations Size"] = sum(arg1);
}

pony$target:::gc-start
{
  @counts[arg0, "GC Passes"] = count();
  self->start_gc = timestamp;
}

pony$target:::gc-end
{
  @times[arg0, "Time in GC"] = sum(timestamp - self->start_gc);
}

pony$target:::gc-send-start
{
  @counts[arg0, "Objects Sent"] = count();
  self->start_send = timestamp;
}

pony$target:::gc-send-end
{
  @times[arg0, "Time in Send Scan"] = sum(timestamp - self->start_send);
}

pony$target:::gc-recv-start
{
  @counts[arg0, "Objects Received"] = count();
  self->start_recv = timestamp;
}

pony$target:::gc-recv-end
{
  @times[arg0, "Time in Recv Scan"] = sum(timestamp - self->start_recv);
}

END
{
  printf("%?s  %-40s %10s %10s %10s\n", "SCHEDULER", "EVENT", "COUNT", "TIME (ns)", "SIZE");
  printa("%?p  %-40s %@10d %@10d %@10d\n", @counts, @times, @sizes);
}
