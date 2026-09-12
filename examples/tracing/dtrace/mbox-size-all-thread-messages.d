#!/usr/bin/env dtrace -x aggsortkey -x aggsortkeypos=0 -q -s

BEGIN
{
  printf("Column headings:      scheduler #, msg type, msg-in|msg-out, =, count\n");
  printf("Special scheduler #s: KQUEUE = -10, IOCP = -11, EPOLL =- 12\n");
  printf("Message types:        SCHED_BLOCK = 20, SCHED_UNBLOCK = 21,\n");
  printf("Message types:        SCHED_CNF = 30, SCHED_ACK = 31,\n");
  printf("Message types:        SCHED_TERMINATE = 40, SCHED_UNMUTE_ACTOR = 50\n");
  printf("Message types:        SCHED_NOISY_ASIO = 51, SCHED_UNNOISY_ASIO = 52\n");
  printf("Message types:        OTHER = 0\n");
  printf("\n");
}

pony$target:::thread-msg-push
{
  this->msg_id = arg0;
  this->thread_from = arg1;
  this->thread_to = arg2;
  @all[this->thread_to, this->msg_id, "msg-in"] = count();
}

pony$target:::thread-msg-pop
{
  this->msg_id = arg0;
  this->thread = arg1;
  @all[this->thread, this->msg_id, "msg-out"] = count();
}

tick-1sec
{
  printa("%3d %3d %-7s = %@12d\n", @all);
  printf("\n");
  clear(@all);
}

END
{
  printf("Final:\n");
  printa("%3d %3d %-7s = %@12d\n", @all);
  printf("\n");
}
