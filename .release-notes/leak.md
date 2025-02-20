## Fix memory leak

In the process of fixing a "in theory could be use after free" that tooling found, we created a real memory leak. We've reverted back to the "in theory bad, in practice not" code from before and fixed the leak.
