## Fix multiple races within actor/cycle detector interactions

The Pony runtime includes an optional cycle detector that is on by default. The cycle detector runs and looks for groups of blocked actors that will have reference counts above 0 but are unable to do any more work as all members are blocked and don't have additional work to do.

Over time, we have made a number of changes to the cycle detector to improve it's performance and mitigate it's impact on running Pony programs. In the process of improving the cycle detectors performance, it has become more and more complicated. That complication led to several race conditions that existed in the interaction between actors and the cycle detector. Each of these race conditions could lead to an actor getting freed more than once, causing an application crash or an attempt to access an actor after it had been deleted.

We've identified and fixed what we believe are all the existing race conditions in the current design.
