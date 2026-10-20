## Fix Sort stack overflow on degenerate inputs

Sorting an already-sorted, reverse-sorted, or all-equal array of moderate size (around 100,000 elements or more, depending on the platform's stack limit) caused a stack overflow. Sorting these inputs now completes in O(n log n) time regardless of input pattern.
