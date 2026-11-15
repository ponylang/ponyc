## Fix HashMap performance degradation after insert-remove churn

`HashMap` (and `HashSet`, which wraps it) could degrade to full-table scans after repeated insert-remove cycles at steady size. Removing a key marked the slot as deleted, but deleted slots were never counted toward the load factor that triggers a resize. Once every empty slot in the table became a deleted slot, lookups of missing keys and insertions of new keys probed every slot in the table.

The load factor now counts deleted slots alongside live entries. When deleted slots are what filled the table, the map rehashes at the same capacity to reclaim them without growing.
