## Remove `stable` from Docker images

Previously, we were including our very old and very deprecated dependency management tool `stable` in our `ponyc` Docker images. We've stopped.

## Move heap ownership info from chunk to pagemap

An internal runtime change has been made around where/how heap chunk ownership information is stored in order to improve performance. The tradeoff is that this will now use a little more memory in order to realize the performance gains.

