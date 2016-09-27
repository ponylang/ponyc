#!/usr/sbin/dtrace -s

#pragma D option quiet

pony*:::gc-start
{
  @counts["GC Passes"] = count();
  self->start = timestamp;
}

pony*:::gc-end
{
  @times["Time in GC (ms)"] = sum(timestamp - self->start);
}

END
{
  normalize(@times,1000000);
  printa(@counts);
  printa(@times);
}
