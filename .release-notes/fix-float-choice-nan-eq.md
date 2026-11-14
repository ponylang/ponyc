## Fix identical NaN property failures reported as distinct

Property tests using float generators that produce NaN reported each failure as a new one, even when the failures were identical. The equality check compared floats with `==`, which returns false when either operand is NaN, so identical failure sequences containing NaN were never recognized as duplicates.

Float choices are now compared by their bit patterns. Two choices with the same bits are equal regardless of NaN, and `-0.0` and `0.0` are now distinct because they have different bits.
