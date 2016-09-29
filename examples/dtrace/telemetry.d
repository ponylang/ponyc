#!/usr/sbin/dtrace -x aggsortkey -x aggsortkeypos=1 -s

#pragma D option quiet

inline unsigned int UINT32_MAX = -1;
inline unsigned int ACTORMSG_BLOCK = (UINT32_MAX - 6);
inline unsigned int ACTORMSG_UNBLOCK = (UINT32_MAX - 5);
inline unsigned int ACTORMSG_ACQUIRE = (UINT32_MAX - 4);
inline unsigned int ACTORMSG_RELEASE = (UINT32_MAX - 3);
inline unsigned int ACTORMSG_CONF = (UINT32_MAX - 2);
inline unsigned int ACTORMSG_ACK = (UINT32_MAX - 1);

pony*:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_BLOCK /
{
  @counts[arg0, "Block Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_UNBLOCK /
{
  @counts[arg0, "Unblock Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_ACQUIRE /
{
  @counts[arg0, "Acquire Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_RELEASE /
{
  @counts[arg0, "Release Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_CONF /
{
  @counts[arg0, "Confirmation Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (unsigned int)arg1 == (unsigned int)ACTORMSG_ACK /
{
  @counts[arg0, "Acknowledgement Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (unsigned int)arg1 < (unsigned int)ACTORMSG_BLOCK /
{
  @counts[arg0, "Application Messages Sent"] = count();
}

pony*:::actor-alloc
{
  @counts[arg0, "Actor Allocations"] = count();
}

pony*:::heap-alloc
{
  @counts[arg0, "Heap Allocations"] = count();
  @sizes[arg0, "Heap Allocations Size"] = sum(arg1);
}

pony*:::gc-start
{
  @counts[arg0, "GC Passes"] = count();
  self->start_gc = timestamp;
}

pony*:::gc-end
{
  @times[arg0, "Time in GC"] = sum(timestamp - self->start_gc);
}

pony*:::gc-send-start
{
  @counts[arg0, "Objects Sent"] = count();
  self->start_send = timestamp;
}

pony*:::gc-send-end
{
  @times[arg0, "Time in Send Scan"] = sum(timestamp - self->start_send);
}

pony*:::gc-recv-start
{
  @counts[arg0, "Objects Received"] = count();
  self->start_recv = timestamp;
}

pony*:::gc-recv-end
{
  @times[arg0, "Time in Recv Scan"] = sum(timestamp - self->start_recv);
}

END
{
  printf("%?s  %-40s %10s %10s %10s\n", "SCHEDULER", "EVENT", "COUNT", "TIME (ns)", "SIZE");
  printa("%?p  %-40s %@10d %@10d %@10d\n", @counts, @times, @sizes);
}
