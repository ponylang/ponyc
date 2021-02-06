## Fix calculating the number of CPU cores on FreeBSD/DragonFly

The number of CPU cores is used by Pony to limit the number of scheduler
threads (`--ponymaxthreads`) and also used as the default number of scheduler
threads.

Previously, `sysctl hw.ncpu` was incorrectly used to determine the number of
CPU cores on FreeBSD and DragonFly. On both systems `sysctl hw.ncpu` reports
the number of logical CPUs (hyperthreads) and NOT the number of physical CPU
cores.

This has been fixed to use `kern.smp.cores` on FreeBSD and
`hw.cpu_topology_core_ids` * `hw.cpu_topology_phys_ids` on DragonFly to
correctly determine the number of physical CPU cores.

This change only affects FreeBSD and DragonFly systems with hardware that
support hyperthreading. For instance, on a 4-core system with 2-way HT,
previously 8 scheduler threads would be used by default. After this change, the
number of scheduler threads on the same system is limited to 4 (one per CPU
core).

