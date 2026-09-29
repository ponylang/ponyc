## Fix classification percentages skewed by regression replay

When a property used `classify`, `cover`, `tabulate`, or `collect` and had a stored regression, the regression replay sample's classifications were counted alongside the normal samples. The replay added to the classification numerator but not to the sample count denominator, so reported percentages exceeded 100% (e.g., 110.0% instead of 100.0% for a label that every sample carried).
