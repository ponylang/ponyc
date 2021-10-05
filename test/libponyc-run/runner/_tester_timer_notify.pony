use "time"

class iso _TesterTimerNotify is TimerNotify
  let _tester: _Tester

  new iso create(tester: _Tester) =>
    _tester = tester

  fun ref apply(timer: Timer, count: U64 val): Bool =>
    _tester.timeout()
    false
