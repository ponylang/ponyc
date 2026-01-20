use "pony_test"

actor Consumer
  let _out: OutStream
  let _n: I64
  var _next: I64 = 0
  let _fifo: Fifo
  let _parent: MainFifo
  let _h: TestHelper
  var _saw: I64 = -1 // assert we see contiguous integers
  let _t0: (I64,I64)  

  new create(out:OutStream, n:I64, fifo:Fifo, parent:MainFifo, h: TestHelper, t0:(I64,I64)) =>
    //out.print("consumer: created. will consume " + n.string())
    _fifo = fifo
    _out = out
    _n = n
    _parent = parent
    _h = h
    _t0 = t0
    
  be start() =>
    _out.print("consumer: started. _next = " + _next.string())
    _fifo.consumerRequestsNext(this, _next)

  be consumeThis(prod: Product val) => // class Product
  Assert.equal[I64](prod.id, _saw+1)
  _saw = _saw + 1
  _out.print("consumer: has consumed " + prod.string())
  _next = _next + 1 // should == prod.id + 1
  if _next < _n then
    _out.print("consumer: about to ask for _next = " + _next.string())
    _fifo.consumerRequestsNext(this, _next)
  else
    // notify parent
    _parent.consumerDone(_h, _t0)
  end
