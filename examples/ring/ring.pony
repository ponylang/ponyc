"""
A ring is a group of processes connected to each other using
unidirectional links through which messages can pass from process to
process in a cyclic manner.

The logic of this program is as follows:
* Each process in a ring is represented by the actor `Ring`
* `Main` creates Ring by instantiating `Ring` actors based on the
  arguments passed and links them with each other by setting the next
  actor as the previous ones number and at the end linking the last actor
  to the first one thereby closing the links and completing the ring
* Once the ring is complete messages can be passed by calling the `pass`
  behaviour on the current `Ring` to its neighbour.
* The program prints the id of the last `Ring` to receive a message

For example if you run this program with the following options `--size 3`
and `--pass 2`. It will create a ring that looks like this:


        *  *              *  *
     *        *        *        *
    *     1    *_ _ _ *     2    *
    *          *      *          *
     *        *        *        *
        *  *              *  *
          \                 /
           \               /
            \             /
             \    *  *   /
               *        *
              *     3    *
              *          *
               *        *
                  *  *

and print 3 as the id of the last Ring actor that received the
message.
"""
