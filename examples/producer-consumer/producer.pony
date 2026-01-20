use "pony_test"

actor Producer
  let _out: OutStream
  let _n: I64
  let _fifo: Fifo
  var _next: I64 = 0
  let _parent: MainFifo
  let _h: TestHelper

  new create(out:OutStream, n:I64, fifo:Fifo, parent:MainFifo, h: TestHelper) =>
    _out = out
    _out.print("producer: created. will produce " + n.string())
    _fifo = fifo
    _n = n
    _parent = parent
    _h = h

  be start() =>
    _out.print("producer: started. _next = " + _next.string())
    _fifo.requestToProduce(this, _next)

  be produce(id:I64) =>
    let prod:Product iso = Product(id)
    _fifo.append(this, consume prod) 
    _out.print("producer: has produced " + id.string())
    _next = _next + 1
    if _next < _n then
        _fifo.requestToProduce(this, _next)
    else
      // notify parent
      _parent.producerDone(_h)
    end
