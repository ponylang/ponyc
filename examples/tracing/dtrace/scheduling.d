#!/usr/bin/env dtrace -x aggsortkey -x aggsortkeypos=1 -s

#pragma D option quiet

pony$target:::cpu-nanosleep
{
  @timer["Nanosleep (ns)"] = sum(arg0)
}

pony$target:::actor-scheduled
{
  @counter["Actor Scheduled", arg1] = count();
}

pony$target:::actor-descheduled
{
  @counter["Actor De-Scheduled", arg1] = count();
}

pony$target:::actor-msg-run
{
  @counter["Actor Messages", arg1] = count();
}

pony$target:::work-steal-successful
{
  @work["Successful Work Stealing", arg0] = count();
}

pony$target:::work-steal-failure
{
  @work["Failed Work Stealing", arg0] = count();
}

END
{
  printa(@counter);
  printa(@work);
  printa(@timer);
}
