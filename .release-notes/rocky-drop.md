## Drop Rocky 8 support

In July of 2021, we added prebuilt binaries for Rocky 8. We did this at the request of a ponyc user. However, after attempting an update to get Rocky 8 building again, we hit a problem where in the new CI environment we built, we are no longer able to build ponyc. We do not have time to investigate beyond the few hours we have already put in. If anyone in the community would like to get the Rocky 8 environment working again, we would be happy to add support back in.

The current issue involves libatomic not being found during linking. If you would like to assist, hop into the [CI stream](https://ponylang.zulipchat.com/#narrow/stream/190359-ci) on the ponylang Zulip.
