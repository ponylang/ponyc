set pagination off
set confirm off
break frame_collect if count == 2
run 137 --ponymaxthreads=1
python
import gdb
targets = set()
for index in range(2):
    root = 'roots[%d]' % index
    array = '((void**)' + root + ')[1]'
    backing = '((void**)' + array + ')[3]'
    for expr in (root, array, backing):
        targets.add(int(gdb.parse_and_eval('(uintptr_t)(' + expr + ')')))
if len(targets) != 6 or 0 in targets:
    raise gdb.GdbError('expected six distinct allocations')
seen = set()
class Mark(gdb.Breakpoint):
    def stop(self):
        seen.add(int(gdb.parse_and_eval('(uintptr_t)p')))
        return False
watch = Mark('ponyint_heap_mark')
shallow = Mark('ponyint_heap_mark_shallow')
end
tbreak ponyint_heap_endgc
continue
python
if not targets <= seen:
    raise gdb.GdbError('caller or backing allocation missing from marking')
print('Observed marking for both holders, both arrays and both backing allocations before sweep.')
watch.delete()
shallow.delete()
end
continue
