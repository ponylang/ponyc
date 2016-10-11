# DTrace in PonyC

[DTrace](http://dtrace.org/guide/preface.html) provides a library for dynamic
tracing of events. This document describes the implementation of DTrace within
the Pony compiler. In particular, it focuses on how to use and extend the DTrace
implementation within Pony.

DTrace is only available for MacOS and BSD. For Linux there is an alternative
called [SystemTap](https://sourceware.org/systemtap/). SystemTap uses the same
resources as DTrace, but uses different scripts. You will find more information
on using SystemTap within the Pony compiler [here](../systemtap/README.md).

## Using DTrace scripts for Pony

You can find various example DTrace scripts in `example/dtrace/`. To run these
scripts, you can use a command in the form of `[script] -c [binary]`. You need
to use a PonyC compiler compiled with the DTrace flag to use DTrace. To compile
a DTrace enabled compiler, you can use the following command: `make
config=release use=dtrace`.

We can also execute a script using the dtrace command. This is necessary if the
script does not contain a "Shebang". We can do it using the following command:
`dtrace -s [script] -c [binary]`.

### MacOS 10.11+

MacOS El Capitan for the first time included "System Integrity Protection". This
feature does not allow for all DTrace features to be used. This includes custom
DTrace providers. To get around this problem, the following steps can be
followed:

1. Boot your Mac into Recovery Mode.
2. Open the Terminal from the Utilities menu.
3. Then use the following commands:

```bash

csrutil clear # restore the default configuration first

csrutil enable --without dtrace # disable dtrace restrictions *only*

```

After completing these steps your Mac should be able to use DTrace after a
restart. For more information visit the following article:
[link](http://internals.exposed/blog/dtrace-vs-sip.html).

## Writing scripts for DTrace in Pony

The following paragraph will inform you on the Pony provider and it's probes for
DTrace. We refer to the [DTrace
documentation](http://dtrace.org/guide/preface.html) for more information on the
syntax and possibilities.

The Pony provider for DTrace consists of DTrace probes implemented in the
Runtime. A binary compiled with DTrace enabled will allow the user access to the
information of the probes. You can find a list of all probes and their arguments
in [`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d).  The
following is a toy example DTrace script:

```

pony$target:::gc-start
{
  @ = count();
}

END
{
  printa(@);
}

```

This script increases a counter every time the "GC Start" probe is *fired* and
prints it result at the end. Note the way in which we (describe the probe). It
consists of two parts: the first part is the provider and the second part, after `:::`
is the name of the probe. The provider is during runtime appended by the process
ID. In this example we use the `$target` variable to find the process ID.
Another option is to use `pony*`, but note that this could match many processes.
The name of the probe are also different from the names in
[`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d). In Dtrace
scripts you have to replace the `__` in the names by `-`.

To make these scripts executable, like the ones in the examples, we use the
following "Shebang": `#!/usr/bin/env dtrace -s`. Extra options can be added to
improve its functionality.

## Extending the DTrace implementation in Pony

You can extend the DTrace implementation by adding more probes or extra
information to existing probes.  All probes are defined in
[`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d). Usually their
names of their module and the event that triggers them. To install Probes in C
use the macros defined in `src/common/dtrace.h`.  To fire a probe in the C code
use the macro `DTRACEx`; where `x` is the number of arguments of  the probe.
There is also a macro `DTRACE_ENABLED`; its use allows for code to be only
executed when a probe is enabled.

### Adding a probe

The first step to add a probe is to add its definition into
[`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d). We have split
the names of the probes into two parts: its category and the specific event. The
name is split using `__`. We also add a comment to a probe explaining when it's
fired and the information contained in its arguments.

After adding the probe, we use it in the C code using the macro's. Note that the
split in the name is only a single underscore in the C code. The name of the
probe should also be capitalised. We can call the probe defined as `gc__start`
using the statement `DTRACE1(GC_START,  scheduler)`. In this statement
`scheduler` is the data used as the first argument.

Then once the probe has been placed in all the appropriate places, we are ready
to recompile. Make sure to use the DTrace flag while compiling. Recompiling will
create the appropriate files using the system installation of DTrace.

### Extending a probe

We can extend a probe by adding an extra argument to the probe. Like adding a
probe, the probe definition needs to be appended in
[`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d). Note that this
extra information needs to be available **everywhere** the probe is used.

Once you've added the argument, you need to change **all** instances where the
probe is in use. Note that you have to both add the new argument and change the
`DTRACE` macro. Then you can recompile the PonyC compiler to use your new
arguments.

*Do not forget that if you change the order of the arguments for any of the
existing nodes, you also need to change their usage in the example scripts.*
