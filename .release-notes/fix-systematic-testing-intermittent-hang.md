## Fix an intermittent hang when using systematic testing

Programs run under systematic testing (`--ponysystematictestingseed`) could
intermittently hang instead of running to completion. The hang was timing
sensitive: the same seed might hang on one run and finish normally on another.
This has been fixed.
