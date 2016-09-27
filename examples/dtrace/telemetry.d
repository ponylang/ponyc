#!/usr/sbin/dtrace -s

#pragma D option quiet

inline unsigned int UINT32_MAX = -1;
inline unsigned int ACTORMSG_BLOCK = (UINT32_MAX - 6);
inline unsigned int ACTORMSG_UNBLOCK = (UINT32_MAX - 5);
inline unsigned int ACTORMSG_ACQUIRE = (UINT32_MAX - 4);
inline unsigned int ACTORMSG_RELEASE = (UINT32_MAX - 3);
inline unsigned int ACTORMSG_CONF = (UINT32_MAX - 2);
inline unsigned int ACTORMSG_ACK = (UINT32_MAX - 1);

pony*:::actor-msg-send
/ (int)arg0 == ACTORMSG_BLOCK /
{
  @messages["Block Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (int)arg0 == ACTORMSG_UNBLOCK /
{
  @messages["Unblock Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (int)arg0 == ACTORMSG_ACQUIRE /
{
  @messages["Acquire Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (int)arg0 == ACTORMSG_RELEASE /
{
  @messages["Release Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (int)arg0 == ACTORMSG_CONF /
{
  @messages["Confirmation Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (int)arg0 == ACTORMSG_ACK /
{
  @messages["Acknowledgement Messages Sent"] = count();
}

pony*:::actor-msg-send
/ (int)arg0 < ACTORMSG_BLOCK /
{
  @messages["Application Messages Sent"] = count();
}

pony*:::actor-alloc
{
  @counts["Actor Allocations"] = count();
}

pony*:::heap-alloc
{
  @counts["Heap Allocations"] = count();
  @sizes["Heap Allocations Size"] = sum(arg0);
}

pony*:::gc-start
{
  @counts["GC Passes"] = count();
  self->start_gc = timestamp;
}

pony*:::gc-end
{
  @times["Time in GC (ns)"] = sum(timestamp - self->start_gc);
}

pony*:::gc-send-start
{
  @counts["Objects Sent"] = count();
  self->start_send = timestamp;
}

pony*:::gc-send-end
{
  @times["Time in Send Scan (ns)"] = sum(timestamp - self->start_send);
}

pony*:::gc-recv-start
{
  @counts["Objects Received"] = count();
  self->start_recv = timestamp;
}

pony*:::gc-recv-end
{
  @times["Time in Recv Scan (ns)"] = sum(timestamp - self->start_recv);
}

END
{
  printa(@counts);
  printa(@messages);
  printa(@times);
  printa(@sizes);
}
