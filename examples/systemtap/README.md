# SystemTap in PonyC

SystemTap is the Linux alternative to DTrace. Although DTrace is available for Linux the licensing and philosophical differences have caused Linux developers to create an alternative tracing framework that is compatible with the GPLv2, thus SystemTap was created. SystemTap and DTrace operate in a similar fashion but since then the frameworks have been developed independently. Complete SystemTap documentation can be found [here](https://sourceware.org/systemtap/documentation.html)

## Requirements

*   Linux Kernel with UPROBES or UTRACE.

  If your Linux kernel is version 3.5 or higher, it already includes UPROBES. To verify that the current kernel supports UPROBES natively, run the following command:

  ```
  # grep CONFIG_UPROBES /boot/config-`uname -r`
  ```

  If the kernel supports the probes this will be the output:
  ```
  CONFIG_UPROBES=y
  ```
        
  If the kernel's version is prior to 3.5, SystemTap automatically builds the UPROBES module. However, you also need the UTRACE kernel extensions required by the SystemTap user-space probing to track various user-space events. This can be verified with this command:
  
  ```
  # grep CONFIG_UTRACE /boot/config-`uname -r
  ```
  If the < 3.5 kernel supports user tracing this will be the output:
  
  ```
  CONFIG_UTRACE=y
  ```

  Note: UTRACE does not exist on kernel versions > 3.5 and has been replaced by UPROBES

*   SystemTap > 2.6

    You need the `dtrace` commandline tool to generate header and object files that are needed in the compilation and linking of PonyC compiler. This is requred on Linux systems. At the time of writing this the version of SystemTap that these probes have been implemented and tested with is 2.6, on Debian 8 Stable. Later versions should work. Earlier versions might not work and must be tested independently. In debian based systems, SystemTap can be installed with apt-get
    
        ```
        $sudo apt-get install systemtap systemtap-runtime
        ```

## Using SystemTap scripts for PonyC

You can find various example SystemTap scripts in `example/systemtap/`. To run these
scripts, a sample command would be of the form:

```
# stap [script path] -c [binary path]
```

NB: stap must be run as root (or sudo) since it compiles the SystemTap script into a loadable kernel module and loads it into the kernel when the stap script is running.

You need to use a PonyC compiler compiled with the DTrace flag to use SystemTap. To compile a SystemTap enabled compiler, add the use=dtrace flag into the make command: 

`make config=release use=dtrace`.

## Writing scripts for SystemTap in PonyC

SystemTap and DTrace use the same syntax for creating providers and thus we direct to the [DTrace
documentation](http://dtrace.org/guide/preface.html) for more information on the
syntax and possibilities.

The PonyC provider for SystemTap consists of DTrace probes implemented in the
Runtime. A binary compiled with DTrace enabled will allow the user access to the
information of the probes, which work with SystemTap scripts. You can find a list of all probes and their arguments
in [`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d).  The
following is a toy example of a SystemTap script:

```
global gc_passes

probe process.mark("gc-start")
{
  gc_passes <<< $arg1;
}

probe end
{
  printf("Total number of GC passes: %d\n", @count(gc_passes));
}

```

This simple example will hook into the executable that the -c parameter provides and searches for the probe named "gc-start". Once execution of the executable is started, the script increases a counter every time the "gc-start" probe is accessed and
at the end of the run the results of the counter are printed

In SystemTap you can use the DTrace syntax of "gc-start" but you may also call them like they are in the [`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d) file, "gc__start".

All available probes in an executable can be listed like this:

```
# stap -L 'process("<name_and_path_of_executable>").mark("*")'
```

This is useful to confirm that probes are ready to be hooked in.

## Extending the SystemTap implementation in Pony

You can extend the SytemTap implementation by adding more probes or extra
information to existing probes. All probes are defined in
[`src/common/dtrace_probes.d`](../../src/common/dtrace_probes.d). Usually their
names of their module and the event that triggers them. To install Probes in C
use the macros defined in `src/common/dtrace.h`.  To fire a probe in the C code
use the macro `DTRACEx`; where `x` is the number of arguments of  the probe.
There is also a macro `DTRACE_ENABLED`; its use allows for code to be only
executed when a probe is enabled.

For adding and extending probes please refer to the DTrace [`README`](../dtrace/README.md#adding-a-probe) file, since this process works exactly the same for both systems.
