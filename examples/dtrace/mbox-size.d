#!/usr/bin/env dtrace -s

#pragma D option quiet

/*
 * This script only tracks user actor -> user actor messages.
 * Messages such as ACTORMSG_ACK and ACTORMSG_CONF (see telemetry.d)
 * aren't caused directly by Pony code that is visible to the
 * programmer.  Invisible system messages are detected with the
 * 0xffffff00 mask.
 */

pony$target:::actor-msg-send
/(arg1 & 0xffffff00)!= 0xffffff00/
{
  @in[arg3] = count();
}

pony$target:::actor-msg-run
{
  @out[arg1] = count();
}

tick-1sec
{
  printf("Into mbox (actor-msg-send):\n");
  printa(@in);
  clear(@in);
  printf("Out of mbox (actor-msg-run):\n");
  printa(@out);
  clear(@out);
  printf("\n");
}
