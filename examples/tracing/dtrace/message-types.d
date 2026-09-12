#!/usr/bin/env dtrace -s

#pragma D option quiet

pony$target:::actor-msg-send
{
  @type[arg1] = count();
}

tick-1sec
{
  printa(@type);
  clear(@type);
  printf("\n");
}
