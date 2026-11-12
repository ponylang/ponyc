"""Check reporter output with controlled outcomes and real CLI completion."""

import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest


class ReportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="mutation reports-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.first = self.root / "source one.pony"
        self.second = self.root / "source-two.pony"
        self.source = '// "source" \\ café � 😀\n// second line\n// third line\n'
        for path in (self.first, self.second):
            path.write_bytes(self.source.encode("utf-8"))
        self.binary = self.root / self.fixture.name
        shutil.copy2(self.fixture, self.binary)
        self.info = self.binary.with_suffix(".mutants.json")
        self.coverage = Path(str(self.binary) + ".coverage")
        self.records = [
            dict(id=i, coverage_counter=i - 1, file=str(path), line=i,
                 column=1, category="aor", original="+", replacement="-")
            for i, path in enumerate((self.first, self.second), 1)
        ]
        self.info.write_text(json.dumps({"mutants": self.records}),
                             encoding="utf-8")
        self.coverage.write_bytes(struct.pack("<iQQ", 2, 0, 0))
        self.env = os.environ.copy()
        self.env.pop("PONY_MUTANT_ID", None)

    def run_process(self, args, expected, **process_options):
        result = subprocess.run(
            [str(a) for a in args], capture_output=True, encoding="utf-8",
            env=self.env, timeout=30, **process_options)
        self.assertEqual(result.returncode, expected,
                         result.stdout + result.stderr)
        return result

    def fixture_output(self, mode, *args, expected=0):
        return self.run_process(
            [self.fixture, mode, self.first, self.second, *args,
             "--ponymaxthreads=1", "--ponynoscale"], expected)

    def cli(self, *args, expected=0):
        return self.run_process(
            [self.runner, self.binary, "--workers=1", *args], expected)

    def report(self, text):
        data = json.loads(text)
        self.assertEqual(data["schemaVersion"], "1")
        self.assertEqual(data["thresholds"], {"high": 80, "low": 60})
        for name, file in data["files"].items():
            self.assertEqual(file["language"], "pony")
            self.assertEqual(file["source"],
                             Path(name).read_bytes().decode("utf-8"))
        if self.validator is not None:
            self.validator.validate(data)
        return data

    def file_report(self, path):
        self.assertTrue(path.is_file(), f"report missing: {path}")
        return self.report(path.read_text(encoding="utf-8"))

    def test_terminal_mixed(self):
        output = self.fixture_output("terminal").stdout
        lines = output.splitlines()
        for line in (
                "Total mutants:  8", "Killed:         1 (12.5%)",
                "Survived:       3 (37.5%)", "Timed out:      2 (25.0%)",
                "No coverage:    2 (25.0%)",
                "Mutation score: 50.0% (killed + timed out) / tested"):
            with self.subTest(line=line):
                self.assertIn(line, lines)
        survivors = output.split("Survived mutants:\n", 1)[1].splitlines()
        self.assertEqual(survivors, [
            f"  {self.first}:1:8  aor  + -> -",
            f"  {self.first}:2:1  aor  + -> -",
            f"  {self.second}:3:4  aor  + -> -",
        ])

    def check_zero_terminal(self, output, total):
        lines = output.splitlines()
        self.assertIn(f"Total mutants:  {total}", lines)
        for status in ("Killed:         ", "Survived:       ",
                       "Timed out:      "):
            self.assertIn(status + "0 (0.0%)", lines)
        self.assertIn("Mutation score: 0.0% (killed + timed out) / tested",
                      lines)
        self.assertNotIn("Survived mutants:", lines)
        if total:
            self.assertIn(f"No coverage:    {total} (100.0%)", lines)
        else:
            self.assertNotIn("No coverage:", output)

    def test_terminal_empty(self):
        self.check_zero_terminal(self.fixture_output("empty").stdout, 0)

    def test_terminal_uncovered(self):
        self.check_zero_terminal(self.fixture_output("uncovered").stdout, 2)

    def test_json_mixed(self):
        data = self.report(self.fixture_output("json").stdout)
        self.assertEqual(set(data["files"]),
                         {str(self.first), str(self.second)})
        text = ''.join(chr(i) for i in range(32)) + '"\\ café'
        expected = {
            1: (self.first, 1, 1, "Killed"),
            2: (self.first, 2, 3, "Timeout"),
            3: (self.second, 1, 2, "Timeout"),
            4: (self.second, 3, 4, "Survived"),
            5: (self.first, 2, 1, "Survived"),
            6: (self.first, 1, 8, "Survived"),
            7: (self.first, 3, 2, "NoCoverage"),
            8: (self.second, 2, 4, "NoCoverage"),
        }
        ids = []
        for name, file in data["files"].items():
            for mutant in file["mutants"]:
                with self.subTest(id=mutant["id"]):
                    self.assertIsInstance(mutant["id"], str)
                    ident = int(mutant["id"])
                    ids.append(ident)
                    path, line, column, status = expected[ident]
                    self.assertEqual(name, str(path))
                    self.assertEqual(mutant["status"], status)
                    self.assertEqual(mutant["location"]["start"],
                                     {"line": line, "column": column})
                    self.assertEqual(mutant["location"]["end"],
                                     {"line": line, "column": column + 1})
                    self.assertEqual(mutant["replacement"],
                                     text if ident == 1 else "-")
                    self.assertEqual(mutant["mutatorName"],
                                     f"{text}/{text}->{text}" if ident == 1
                                     else "aor/+->-")
        self.assertCountEqual(ids, expected)

    def test_json_write_file(self):
        path = self.root / "report.json"
        path.write_bytes(b"previous contents" * 1000)
        self.fixture_output("file", path)
        self.assertEqual(self.file_report(path),
                         self.report(self.fixture_output("json").stdout))

    def test_empty_source(self):
        self.first.write_bytes(b"")
        data = self.report(self.fixture_output("json").stdout)
        self.assertEqual(data["files"][str(self.first)]["source"], "")
        path = self.root / "empty-source.json"
        self.fixture_output("file", path)
        self.assertEqual(self.file_report(path)["files"][str(self.first)]
                         ["source"], "")

    def test_cli_json(self):
        path = self.root / "cli.json"
        result = self.cli("--no-baseline", "--format=json",
                          "--json-file=" + str(path), expected=1)
        data = self.report(result.stdout)
        self.assertEqual(self.file_report(path), data)
        mutants = [m for f in data["files"].values() for m in f["mutants"]]
        self.assertCountEqual([m["id"] for m in mutants], ["1", "2"])
        self.assertEqual([m["status"] for m in mutants], ["Survived"] * 2)

    def test_cli_terminal_with_json_file(self):
        path = self.root / "terminal.json"
        result = self.cli("--no-baseline", "--json-file=" + str(path),
                          expected=1)
        self.assertIn("Mutation Testing Report", result.stdout)
        data = self.file_report(path)
        self.assertCountEqual([m["id"] for f in data["files"].values()
                               for m in f["mutants"]], ["1", "2"])

    def test_cli_empty_selection(self):
        path = self.root / "empty.json"
        result = self.cli("--scope=absent.pony", "--format=json",
                          "--json-file=" + str(path))
        self.assertIn("After filtering: 0 mutants", result.stderr)
        data = self.report(result.stdout)
        self.assertEqual(data["files"], {})
        self.assertEqual(self.file_report(path), data)
        self.check_zero_terminal(self.cli("--scope=absent.pony").stdout, 0)

    def test_cli_uncovered_selection(self):
        path = self.root / "uncovered.json"
        result = self.cli("--format=json", "--json-file=" + str(path))
        self.assertIn("Skipping 2 mutant(s) with no coverage", result.stderr)
        data = self.report(result.stdout)
        self.assertEqual(self.file_report(path), data)
        self.assertEqual(set(data["files"]),
                         {str(self.first), str(self.second)})
        for record in self.records:
            mutants = data["files"][record["file"]]["mutants"]
            self.assertEqual(len(mutants), 1)
            self.assertEqual(mutants[0]["id"], str(record["id"]))
            self.assertEqual(mutants[0]["status"], "NoCoverage")
        self.check_zero_terminal(self.cli().stdout, 2)

    def test_cli_overwrite(self):
        path = self.root / "overwrite.json"
        self.cli("--json-file=" + str(path))
        self.assertEqual(len(self.file_report(path)["files"]), 2)
        self.cli("--scope=absent.pony", "--json-file=" + str(path))
        self.assertEqual(self.file_report(path)["files"], {})

    def test_cli_missing_source(self):
        self.first.unlink()
        result = self.cli("--format=json", expected=1)
        self.assertEqual(result.stdout, "")
        self.assertIn("error: cannot read source " + str(self.first),
                      result.stderr)
        path = self.root / "missing-source.json"
        result = self.cli("--json-file=" + str(path), expected=1)
        self.assertIn("error: cannot read source " + str(self.first),
                      result.stderr)
        self.assertFalse(path.exists())
        self.check_zero_terminal(self.cli().stdout, 2)

    def test_write_file_failure(self):
        path = self.root / "missing-parent" / "report.json"
        result = self.fixture_output("file", path, expected=1)
        self.assertIn("error: cannot write " + str(path), result.stderr)
        self.assertFalse(path.exists())
        result = self.cli("--json-file=" + str(path), expected=1)
        self.assertIn("error: cannot write " + str(path), result.stderr)
        self.assertFalse(path.exists())

    def test_invalid_source_encoding(self):
        path = self.root / "invalid-source.json"
        for invalid in (b"\x80", b"\xff", b"\xe2\x82", b"\xc0\xaf",
                        b"\xed\xa0\x80", b"\xf4\x90\x80\x80"):
            with self.subTest(invalid=invalid):
                self.first.write_bytes(b"// comment " + invalid + b"\n")
                result = self.fixture_output("json", expected=1)
                self.assertEqual(result.stdout, "")
                self.assertIn("error: invalid UTF-8 in JSON report for "
                              + str(self.first), result.stderr)
                path.write_text("previous report", encoding="utf-8")
                result = self.fixture_output("file", path, expected=1)
                self.assertIn("error: invalid UTF-8 in JSON report for "
                              + str(self.first), result.stderr)
                self.assertEqual(path.read_text(encoding="utf-8"),
                                 "previous report")

    def test_invalid_metadata_encoding(self):
        result = self.fixture_output("invalid-json", expected=1)
        self.assertEqual(result.stdout, "")
        self.assertIn("error: invalid UTF-8 in JSON report for "
                      + str(self.first), result.stderr)

    def test_source_short_read(self):
        self.first = Path("/sys/kernel/uevent_seqnum")
        try:
            size = self.first.stat().st_size
            length = len(self.first.read_bytes())
        except OSError:
            self.skipTest("Linux sysfs sequence file is unavailable")
        if length >= size:
            self.skipTest("sysfs sequence file does not provide a short read")
        result = self.fixture_output("json", expected=1)
        self.assertEqual(result.stdout, "")
        self.assertIn("error: cannot read source " + str(self.first),
                      result.stderr)
        path = self.root / "short-source.json"
        path.write_text("previous report", encoding="utf-8")
        result = self.fixture_output("file", path, expected=1)
        self.assertIn("error: cannot read source " + str(self.first),
                      result.stderr)
        self.assertEqual(path.read_text(encoding="utf-8"), "previous report")

    @unittest.skipUnless(Path("/dev/full").is_char_device(),
                         "requires /dev/full")
    def test_truncate_failure(self):
        result = self.fixture_output("file", "/dev/full", expected=1)
        self.assertIn("error: cannot write /dev/full", result.stderr)

    @unittest.skipUnless(os.name == "posix", "requires POSIX file size limits")
    def test_write_failure_after_open(self):
        import resource
        import signal

        def limit_file_size():
            resource.setrlimit(resource.RLIMIT_FSIZE, (0, 0))
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)

        path = self.root / "write-failure.json"
        result = self.run_process(
            [self.fixture, "file", self.first, self.second, path,
             "--ponymaxthreads=1", "--ponynoscale"], 1,
            preexec_fn=limit_file_size)
        self.assertIn("error: cannot write " + str(path), result.stderr)
        self.assertTrue(path.is_file())
        self.assertEqual(path.stat().st_size, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--schema", action="store_true")
    options, remaining = parser.parse_known_args()
    ReportTests.fixture = options.fixture.resolve()
    ReportTests.runner = options.runner.resolve()
    ReportTests.validator = None
    if options.schema:
        from jsonschema import Draft7Validator
        schema_path = (Path(__file__).parent / "schema"
                       / "mutation-testing-report-schema.json")
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        Draft7Validator.check_schema(schema)
        ReportTests.validator = Draft7Validator(schema)
    unittest.main(argv=[__file__, *remaining])
