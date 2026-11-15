## Fix FrameDecoder looping forever on sub-byte frames and misplacing non-byte-aligned frames

`FrameDecoder` advanced its position in whole bytes. A codec whose frame size was not a multiple of 8 bits triggered one of two failures: frames of 1 to 7 bits caused an infinite loop because the byte offset never moved, and longer frames like 12 bits lost the sub-byte remainder on each advance, decoding every frame after the first from the wrong position.

`FrameDecoder` now tracks its position in bits and carries a sub-byte offset across `received()` calls.
