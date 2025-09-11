## "No op" release to get Windows release out

When we moved from CirrusCI to GitHub Actions, we had some issues with the Windows releases that were do to tiny typos in the config that linting didn't catch.

This resulted in there being no 0.56.0 and 0.56.1 Windows releases. Because 0.56.1 was primarily to fix a Windows bug, we are doing a 0.56.2 release that should hopefully work for Windows as we think we have found all the tiny typos.
