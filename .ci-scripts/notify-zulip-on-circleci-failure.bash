#!/bin/bash

curl -X POST https://ponylang.zulipchat.com/api/v1/messages \
    -u pony-circleci-bot@ponylang.zulipchat.com:57HRshi7qDYxJvAib8LzbCFTT32foDHh \
    -d "type=stream" \
    -d "to=zulip" \
    -d "topic=testing a thing" \
    -d "content=$CIRCLE_BUILD_URL failed"