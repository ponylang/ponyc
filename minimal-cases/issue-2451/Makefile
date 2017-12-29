# Make stdlib and run a long term test running
# ./stdlib --sequential in a loop with logs captured
# in stdlib-ymd-hms.ns.txt.

.PHONY: all
all: make-stdlib test-long-term

.PHONY: test-long-term
test-long-term:
	while true; do echo "Date: `date +%y%m%d-%H%M%S.%N`"; ./stdlib --sequential; echo "stdlib done"; done 2>&1 | tee stdlib-`date +%y%m%d-%H%M%S.%N`.txt

.PHONY: make-stdlib
make-stdlib:
	$(MAKE) stdlib

help:
	@echo 'Run stdlib in a forever loop'
	@echo
