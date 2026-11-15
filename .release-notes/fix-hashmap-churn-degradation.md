## Fix HashMap performance degradation under insert/remove churn

`HashMap` and `HashSet` operations slowed progressively when entries were repeatedly inserted and removed. A map that sustained insert/remove churn eventually took hundreds of times longer per lookup than a map of the same size that was only inserted into. The degradation was permanent — only clearing and repopulating the map recovered normal speed.

Lookups, insertions, and removals now run in constant expected time regardless of how many entries have been added and removed over the map's lifetime.
