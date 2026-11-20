# Generated polls with live callers

The experimental compiler published a mutable holder in each of two suspended callers and inserted a poll between ordinary synchronous calls. Inside the measured behavior, the actor crossed its normal heap threshold and collected once. After both calls returned, the actor stored the holders in fields and queued a behavior that read and changed their array elements. A separate debugger run observed marking of both holders, their arrays and their backing allocations before sweep.

Instrumentation is limited to a selected source file. The first statement of a parameterless actor method must initialize a local by calling a concrete mutable class constructor, with no other local declaration anywhere in the initializer. The next statement must be a direct, nonpartial, zero-argument call on the same actor returning None. The compiler brackets that call with publication and removal of the local. Only the named callee can claim that publication, once on entry. Inside an authorized function, the compiler inserts polls between direct calls in the initial sequence of statements. The runtime refuses a chain over 16 frames or an ineligible heap. Ordinary actor-boundary collection still uses the original roots.

All 20 debug and optimized runs passed. The stock compiler, raising the threshold, and breaking eligibility in either caller produced zero collections inside the behavior. The generated executables used the runtime's standard tracing for the cycle counts. [Results](runs.csv)

From a configured x86-64 Linux debug checkout, run the following with a fresh scratch directory. GNU objdump and GDB are used for the native checks.

```bash
scratch=$(mktemp -d)
exp=experiments/behavior-gc/automatic
cp experiments/behavior-gc/forced/guard.py "$scratch/guard.py"
python3 "$scratch/guard.py" timeout 360 python3 -B "$exp/build.py" "$scratch/build"
python3 "$scratch/guard.py" timeout 360 python3 -B "$exp/run.py" "$scratch/build" "$scratch/one"
python3 "$scratch/guard.py" timeout 120 gdb -nx -batch -x "$exp/marks.gdb" "$scratch/one/enabled-debug/work"
```
