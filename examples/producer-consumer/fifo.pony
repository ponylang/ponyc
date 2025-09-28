use "pony_test"
use "time"

class val Product
  let id: I64
  new create(id':I64) =>
  id = id'
  fun string(): String =>
  id.string()

// MainFifo acts as our test parent
// in _TestInfoBasic, for the producerDone
// and consumerDone behaviors.
actor MainFifo 
  let _out: OutStream
  let _h:TestHelper
  
  new create(h:TestHelper) =>
    _out = h.env.out
    _h = h
    
  be producerDone(h:TestHelper) =>
    h.complete(true) // success, stop the long_test timeout
    
  be consumerDone(h:TestHelper, t0: (I64,I64)) =>
    let t1 = Time.now()
    let elap = (t1._2 - t0._2) + ((t1._1 - t0._1) * 1_000_000_000)
    _out.print("elapsed nanosec = " + elap.string())  
    h.complete(true) // success, stop the long_test timeout

actor Fifo
  let _out: OutStream
  let _cap: USize
  var _buf: Array[Product val]
  var _promised: I64 = 0
  let _h:TestHelper
  let _env:Env

  var _waitQprod: Array[(Producer,I64)] = Array[(Producer,I64)]
  var _waitQcons: Array[(Consumer,I64)] = Array[(Consumer,I64)]

  // treat _buf like a ring buffer to reduce copying
  var _ringBeg: USize = 0
  var _ringReadable: USize = 0 // replace .size() with this.

  fun ref popfront(): Product val? =>
    _ringReadable = _ringReadable -1
    let front = _buf(_ringBeg)?
    _ringBeg = _ringBeg + 1
    if _ringBeg == _cap then
      _ringBeg = 0
    end
    consume front

  fun ref pushback(p: Product val)? =>
    let writeStart = (_ringBeg + _ringReadable) % _cap
    _buf(writeStart)? = consume p
    _ringReadable = _ringReadable + 1
  
  fun ref _clearQs() =>
    _waitQprod = Array[(Producer,I64)]
    _waitQcons = Array[(Consumer,I64)]

  new create(out:OutStream, n:USize, h:TestHelper) =>
    _h = h
    _cap = n
    _out = out
    _buf = Array[Product val].init(Product(0), n)
    _out.print("fifo: created with capacity: " + n.string() + " and size: " + _buf.size().string())
    _env = h.env

  be consumerRequestsNext(fromCons:Consumer, next:I64) =>
    if _ringReadable == 0 then
      _out.print("fifo: consumerRequestsNext: nothing for consumer, add them to _waitQcons")
      _waitQcons.push((fromCons, next))
      return
    end
    Assert.lte[USize](_ringReadable, _cap, "buf must be <= _cap")
    _out.print("fifo: consumerRequestsNext() has _buf.size = " + _buf.size().string())
     
    try
      var x = popfront()?      
      _out.print("fifo: consumerRequestsNext about to provide = " + x.string() + " ; now _buf.size = " + _buf.size().string())
      
      // assert we get the expected next
      if x.id != next then
        Assert.crash("x.id("+x.id.string()+") != next("+next.string()+")")
      end
      
      fromCons.consumeThis(consume x)
    end
    nudgeProducer()

  be append(producer:Producer, product:Product iso) => // class Product
    Assert.gt[I64](_promised, 0, "_promised must be > 0 in append(); product = " + product.string())
    Assert(_ringReadable < _cap, "_ringReadable must be < _cap before a push; promised = " + _promised.string())

    try
      pushback(consume product)?    
      _promised = _promised - 1
      dispatch()
    end

  fun ref dispatch() =>
    if (_ringReadable == 0) or (_waitQcons.size() == 0) then  
      return // nothing to deliver, or no consumer to deliver it to.
    end

    try
      (var fromCons, var next) = _waitQcons.delete(0)?
      _out.print("fifo: dispatch() is taking consumer off _waitQcons to provide them next = "+next.string())
      fromCons.consumeThis(popfront()?)      
    end
    nudgeProducer()

  fun ref nudgeProducer() =>
    if (_waitQprod.size() > 0) and ((_ringReadable.i64() + _promised) < _cap.i64()) then
      // the producer can now use the newly freed slot
      try
        (let producer, let next) = _waitQprod.delete(0)?
        _out.print("fifo: sees free slot and waiting producer, allowing = " + next.string())
        _promised = _promised + 1
        producer.produce(next)
      end
    end


  be requestToProduce(fromProd:Producer, next:I64) =>
    if (_ringReadable.i64() + _promised) < _cap.i64() then
      _out.print("fifo: requestToProduce(next="+next.string()+") allows producer to produce id = " + next.string()+ " since _buf.size = " + _buf.size().string() + " and _promised = " + _promised.string() + " together are < _cap == " + _cap.string())
      _promised = _promised + 1
      fromProd.produce(next)
    else
      _out.print("fifo: requestToProduce() adds producer to waitQ at next = " + next.string())
      _waitQprod.push((fromProd, next))
    end
