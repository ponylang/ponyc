#!/usr/bin/env dtrace -s

#pragma D option quiet
#pragma D option dynvarsize=64m

pony$target:::gc-start
{
  @count["GC Passes"] = count();
  self->start_gc = timestamp;
}

pony$target:::gc-end
/self->start_gc != 0/
{
  @quant["Time in GC (ns)"] = quantize(timestamp - self->start_gc);
  @times["Total time"] = sum(timestamp - self->start_gc);
  self->start_gc = 0;
}

END
{
  printa(@count);
  printa(@quant);
  printa(@times);
}
