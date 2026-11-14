## Fix NaN target score replacing the best entry in PonyCheck's target pool

`PropertyHelper.target` accepted NaN scores, which silently replaced the best entry in the target pool. Once a NaN was stored, the next score of any value replaced it, losing the original best choice sequence. A scoring function that returned `0/0` or the difference of two infinities was enough to break targeting for that label.

A NaN score now fails the property with a diagnostic message naming the label. Infinite scores are unaffected — they compare normally.
