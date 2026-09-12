#!/usr/bin/env dtrace -s

#pragma D option quiet

pony$target:::cpu-nanosleep
{
  @sleep[cpu] = quantize(arg0 / 1000);
}

tick-1sec
{
  printf("nanosleep call times, in microseconds, per CPU\n");
  printa(@sleep);
  clear(@sleep);
}
