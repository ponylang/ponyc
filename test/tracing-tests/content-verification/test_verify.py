#!/usr/bin/env python3
"""Check per-thread trace validation at timestamp ties."""

import itertools
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class VerifyTests(unittest.TestCase):
    def run_verifier(self, events):
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.json"
            trace.write_text(json.dumps(events))
            return subprocess.run(
                [sys.executable, "-I",
                 str(Path(__file__).with_name("verify.py")), str(trace)],
                capture_output=True, text=True, timeout=10)

    def test_valid_transition(self):
        for actor, steal, times in itertools.product(
                ("actor_behavior_run", "actor_behavior_schedule"),
                ("steal_successful", "steal_failure_round"),
                ((0, 1, 2, 2), (0, 1, 2, 3), (0, 0, 0, 0))):
            with self.subTest(actor=actor, steal=steal, times=times):
                events = [
                    {"name": name, "tid": 1, "ts": ts}
                    for name, ts in zip((actor, steal, steal, actor), times)
                ]
                result = self.run_verifier(events)
                self.assertEqual(result.returncode, 0,
                                 result.stdout + result.stderr)
                self.assertIn("PASS tid 1: 2 actor_behavior, 2 steal",
                              result.stdout)

    def test_invalid_regression(self):
        for actor, steal, times in itertools.product(
                ("actor_behavior_run", "actor_behavior_schedule"),
                ("steal_successful", "steal_failure_round"),
                ((0, 1, 1, 1), (0, 1, 2, 3), (0, 0, 0, 0))):
            with self.subTest(actor=actor, steal=steal, times=times):
                events = [
                    {"name": name, "tid": 1, "ts": ts}
                    for name, ts in zip((actor, steal, actor, steal), times)
                ]
                result = self.run_verifier(events)
                self.assertEqual(result.returncode, 1,
                                 result.stdout + result.stderr)
                self.assertIn(
                    f"FAIL tid 1: steal event at {times[-1]} us "
                    "after actor_behavior resumed (state AB2)", result.stdout)

    def test_interleaved_threads(self):
        for invalid_second_thread in (False, True):
            with self.subTest(invalid_second_thread=invalid_second_thread):
                first = [
                    {"name": "actor_behavior_run", "tid": 1, "ts": 0},
                    {"name": "steal_successful", "tid": 1, "ts": 1},
                    {"name": "steal_failure_round", "tid": 1, "ts": 2},
                    {"name": "actor_behavior_schedule", "tid": 1, "ts": 2},
                ]
                second_names = (
                    ("actor_behavior_schedule", "steal_failure_round",
                     "actor_behavior_run", "steal_successful")
                    if invalid_second_thread else
                    ("actor_behavior_schedule", "steal_failure_round",
                     "steal_successful", "actor_behavior_run"))
                second = [
                    {"name": name, "tid": 2, "ts": ts}
                    for name, ts in zip(second_names, (0, 1, 1, 1))
                ]
                events = list(itertools.chain.from_iterable(zip(first, second)))
                result = self.run_verifier(events)
                self.assertEqual(result.returncode, int(invalid_second_thread),
                                 result.stdout + result.stderr)
                self.assertIn("PASS tid 1: 2 actor_behavior, 2 steal",
                              result.stdout)
                if invalid_second_thread:
                    self.assertIn(
                        "FAIL tid 2: steal event at 1 us "
                        "after actor_behavior resumed (state AB2)",
                        result.stdout)
                else:
                    self.assertIn("PASS tid 2: 2 actor_behavior, 2 steal",
                                  result.stdout)

    def test_unequal_timestamps_are_sorted(self):
        result = self.run_verifier([
            {"name": "actor_behavior_run", "tid": 1, "ts": 0},
            {"name": "steal_successful", "tid": 1, "ts": 1},
            {"name": "actor_behavior_schedule", "tid": 1, "ts": 3},
            {"name": "steal_failure_round", "tid": 1, "ts": 2},
        ])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS tid 1: 2 actor_behavior, 2 steal", result.stdout)


if __name__ == "__main__":
    unittest.main()
