## Fix brief scheduler busy-spin after a long idle on 32-bit ARM

On 32-bit ARM, a scheduler thread that had been idle (suspended) for longer than a couple of minutes could, on waking, briefly burn CPU before suspending again instead of re-suspending promptly. This happened because the CPU cycle counter the scheduler uses to measure idle time wraps during such a long idle. Idle scheduler threads on 32-bit ARM now re-suspend promptly no matter how long they have been idle. Programs on other platforms were unaffected.
