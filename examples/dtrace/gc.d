#!/usr/bin/env dtrace -s

#pragma D option quiet

pony$target:::gc-start
{
  @count["GC Passes"] = count();
  self->start_gc = timestamp;
}

pony$target:::gc-end
{
  @quant["Time in GC (ns)"] = quantize(timestamp - self->start_gc);
  @times["Total time"] = sum(timestamp - self->start_gc);
}

END
{
  printa(@count);
  printa(@quant);
  printa(@times);
}
