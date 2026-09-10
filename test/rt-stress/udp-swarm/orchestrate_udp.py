#!/usr/bin/env python3
"""Swarm orchestrator for the UDP stress engine (test/rt-stress/udp-swarm).

Each master seed draws one UDP workload -- random datagram counts, payload sizes,
batch sizes, client counts, and read-loop tuning -- and runs the prebuilt engine
binary once. Omission is the swarm mechanism: each lever is drawn independently,
so different seeds push the net package's UDP stack down different code paths. A failure
(payload corruption, crash, or hang) writes a bundle
recording the seed.

Pass `--ponyc <path>` to have this orchestrator compile the engine before
running it (requires `--ssl` to set the SSL version flag), or build the engine
separately with `ponyc -d -o build/debug test/rt-stress/udp-swarm` and point
`--binary` at the result. The run mechanism -- the no-progress watchdog, the
non-failing backstop, and the optional lldb crash-backtrace wrapper -- is shared
with the TCP swarm orchestrator.
"""
import argparse
import json
import os
import random
import re
import shlex
import signal
import subprocess
import sys
import threading
import time

try:
    import resource  # POSIX only; absent on Windows
except ImportError:
    resource = None

BINARY_NAME = "udp_flood"
SOURCE_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(SOURCE_DIR)))

SSL_FLAG_MAP = {
    "3.0.x": "-Dopenssl_3.0.x",
    "1.1.x": "-Dopenssl_1.1.x",
    "4.0.x": "-Dopenssl_4.0.x",
    "libressl": "-Dlibressl",
}

DEFAULT_NO_PROGRESS_SECONDS = 300
DEFAULT_TIMEOUT_SECONDS = 3000
DEFAULT_MEM_LIMIT_MB = 4096

# ------------------------------------------------------------------- the draw

# UDP has no connection lifecycle and no writev/expect/close-kind levers -- the
# swarm dimensions are the datagram volume, the send batch size, the number of
# concurrent client sockets, and the read-loop tuning (read buffer, per-turn
# datagram ceiling). Each is drawn independently.

DEFAULT_PROFILE = {
    "payload_sizes": [1, 8, 64, 256, 1024, 4096, 8192],
    "batch_sizes": [1, 3, 5, 10, 15],
    "clients": [1, 2, 4],
    "read_buffer_sizes": [1024, 4096, 16384, 65536],
    "max_datagrams_per_turn": [1, 4, 16, 64, 256],
    "datagram_buckets": {"small": (10, 50), "medium": (51, 200),
                         "large": (201, 500)},
}

WORKLOAD_PROFILES = {"default": DEFAULT_PROFILE}

RUN_MAX_ROUND_TRIPS = 10_000         # clients * datagrams
RUN_MAX_BYTES = 50_000_000           # ~50 MB moved per run
MAX_BURST_BYTES = 64_000             # initial burst (clients * batch * payload);
                                     # must fit in the server's UDP receive buffer
                                     # (Windows default ~64 KB is the floor)
MAX_BURST_DATAGRAMS = 200            # per-datagram kernel overhead (~700 bytes on
                                     # Linux) limits how many fit regardless of
                                     # payload size
MIN_DATAGRAMS = 10


def info(message):
    print(message, flush=True)


def die(message):
    print("FATAL: " + message, file=sys.stderr, flush=True)
    sys.exit(1)


def draw_bucketed(rng, buckets):
    """Draw a small/medium/large bucket at 25/50/25, then a uniform value in it.
    Exactly two rng draws."""
    roll = rng.random()
    if roll < 0.25:
        lo, hi = buckets["small"]
    elif roll < 0.75:
        lo, hi = buckets["medium"]
    else:
        lo, hi = buckets["large"]
    return rng.randint(lo, hi)


def clamp_run(clients, datagrams, payload):
    """Trim a drawn (clients, datagrams) to the per-run ceilings."""
    per = max(1, payload)
    if (clients * datagrams) > RUN_MAX_ROUND_TRIPS:
        datagrams = max(MIN_DATAGRAMS, RUN_MAX_ROUND_TRIPS // clients)
    if (clients * datagrams * per) > RUN_MAX_BYTES:
        datagrams = max(MIN_DATAGRAMS, RUN_MAX_BYTES // (clients * per))
        if (clients * datagrams * per) > RUN_MAX_BYTES:
            clients = max(1, RUN_MAX_BYTES // (datagrams * per))
    return clients, datagrams


def resolve_config(master_seed, max_threads, profile="default"):
    """Draw one UDP workload from a master seed. Deterministic per seed."""
    rng = random.Random(master_seed)
    p = WORKLOAD_PROFILES[profile]

    workload = {}
    payload = rng.choice(p["payload_sizes"])
    workload["payload-size"] = payload
    datagrams = draw_bucketed(rng, p["datagram_buckets"])
    workload["datagrams"] = datagrams
    workload["batch-size"] = rng.choice(p["batch_sizes"])
    clients = rng.choice(p["clients"])
    workload["clients"] = clients
    read_buffer = rng.choice(p["read_buffer_sizes"])
    # The read buffer must be at least as large as the payload.
    while read_buffer < payload:
        read_buffer = read_buffer * 2
    workload["read-buffer-size"] = read_buffer
    workload["max-datagrams-per-turn"] = rng.choice(p["max_datagrams_per_turn"])

    # Post-draw clamp.
    workload["clients"], workload["datagrams"] = clamp_run(
        workload["clients"], workload["datagrams"], payload)

    # UDP drops on localhost when actors can't drain receive buffers fast
    # enough. Keep clients ≤ 1.5× the available threads; reduce clients when
    # needed.
    max_safe_clients = max(1, max_threads * 3 // 2)
    if workload["clients"] > max_safe_clients:
        workload["clients"] = max_safe_clients

    # The initial burst (all clients sending their first batch simultaneously)
    # must fit in the server's UDP receive buffer -- both the byte volume and
    # the datagram count (each datagram costs ~700 bytes of kernel SKB
    # overhead regardless of payload).  Reduce clients first when even
    # batch=1 would overflow, then clamp the batch size.
    per = max(1, payload)
    if workload["clients"] * per > MAX_BURST_BYTES:
        workload["clients"] = max(1, MAX_BURST_BYTES // per)
    max_burst_batch = max(1, min(
        MAX_BURST_BYTES // (workload["clients"] * per),
        MAX_BURST_DATAGRAMS // max(1, workload["clients"])))
    workload["batch-size"] = min(workload["batch-size"], max_burst_batch)

    min_threads = max(2, workload["clients"] * 2 // 3)

    runtime = {}
    if rng.random() < 0.5:
        runtime["ponynoscale"] = True
    if rng.random() < 0.3:
        runtime["ponypin"] = True
        if rng.random() < 0.5:
            runtime["ponypinasio"] = True
    if rng.random() < 0.5:
        runtime["ponynoblock"] = True
    runtime["ponymaxthreads"] = rng.randint(
        min(min_threads, max_threads), max_threads)  # LAST

    return {"master_seed": master_seed, "workload": workload, "runtime": runtime}


def build_argv(binary, config):
    """Full command line: workload args first, then runtime flags."""
    argv = [binary]
    for key, value in config["workload"].items():
        argv += ["--" + key, str(value)]
    for key, value in config["runtime"].items():
        if value is True:
            argv.append("--" + key)
        else:
            argv += ["--" + key, str(value)]
    return argv


def parse_result(stdout):
    """Extract the engine's RESULT tally, or {}."""
    result = {}
    for key in ("clients", "client_sent", "server_received",
                "server_corrupted", "client_send_errors", "bind_failed"):
        match = re.search(r"\b" + key + r"=(\d+)", stdout)
        if match is not None:
            result[key] = int(match.group(1))
    return result


# ------------------------------------------------------------- run mechanism

def _rlimit_as_supported(platform, resource_available):
    return resource_available and (platform == "linux")


def _kill_process_tree(proc):
    if os.name == "posix":
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            return
        except (ProcessLookupError, PermissionError):
            pass
    proc.kill()


def _watchdog_kill_reason(now, start, last_progress, timeout,
                          no_progress_seconds):
    if (now - last_progress) > no_progress_seconds:
        return "no_progress"
    if (now - start) > timeout:
        return "backstop"
    return None


_REPORTED_RE = re.compile(rb"HEARTBEAT reported=(\d+)")


def _parse_reported(line):
    match = _REPORTED_RE.search(line)
    return int(match.group(1)) if match is not None else None


def _is_progress(reported, max_reported):
    return (reported is not None) and (reported > max_reported)


def _watch_for_progress(proc, timeout, no_progress_seconds,
                        poll=time.monotonic, sleep=time.sleep):
    start = poll()
    last_progress = [start]
    max_reported = [-1]
    lock = threading.Lock()
    chunks = {"out": [], "err": []}

    def drain(stream, key, track_progress):
        try:
            for line in iter(stream.readline, b""):
                chunks[key].append(line)
                if track_progress:
                    reported = _parse_reported(line)
                    with lock:
                        if _is_progress(reported, max_reported[0]):
                            max_reported[0] = reported
                            last_progress[0] = poll()
        finally:
            stream.close()

    readers = [
        threading.Thread(target=drain, args=(proc.stdout, "out", True),
                         daemon=True),
        threading.Thread(target=drain, args=(proc.stderr, "err", False),
                         daemon=True),
    ]
    for t in readers:
        t.start()

    kill_reason = None
    while proc.poll() is None:
        now = poll()
        with lock:
            last = last_progress[0]
        kill_reason = _watchdog_kill_reason(now, start, last, timeout,
                                            no_progress_seconds)
        if kill_reason is not None:
            _kill_process_tree(proc)
            break
        sleep(0.5)
    proc.wait()
    for t in readers:
        t.join(timeout=5)
    stdout = _decode(b"".join(chunks["out"]))
    stderr = _decode(b"".join(chunks["err"]))
    returncode = None if kill_reason is not None else proc.returncode
    return (kill_reason, returncode, stdout, stderr)


def _capture(argv, timeout, mem_limit_bytes, no_progress_seconds):
    preexec = None
    if (mem_limit_bytes is not None) and _rlimit_as_supported(
            sys.platform, resource is not None):
        def set_limits():
            resource.setrlimit(resource.RLIMIT_AS,
                               (mem_limit_bytes, mem_limit_bytes))
        preexec = set_limits

    info("+ " + " ".join(shlex.quote(a) for a in argv))
    proc = subprocess.Popen(argv, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, preexec_fn=preexec,
                            start_new_session=(os.name == "posix"))
    return _watch_for_progress(proc, timeout, no_progress_seconds)


def _decode(raw):
    return raw.decode(errors="replace") if raw else ""


def lldb_argv(lldb, engine_argv):
    on_crash = ["--one-line-on-crash", "frame variable",
                "--one-line-on-crash", "bt all",
                "--one-line-on-crash", "quit 1"]
    if os.name == "posix":
        setup = ["--one-line", "breakpoint set --name main",
                 "--one-line", "run",
                 "--one-line", "process handle SIGINT --pass true --stop false",
                 "--one-line", "process handle SIGUSR2 --pass true --stop false",
                 "--one-line", "thread continue"]
    else:
        setup = ["--one-line", "run"]
    return [lldb, "--batch"] + setup + on_crash + ["--"] + engine_argv


def lldb_exit_code(output):
    match = re.search(r"Process \d+ exited with status = (\d+)", output)
    return int(match.group(1)) if match is not None else None


class RunResult:
    def __init__(self, outcome, returncode, signal_, stdout, stderr):
        self.outcome = outcome
        self.returncode = returncode
        self.signal = signal_
        self.stdout = stdout
        self.stderr = stderr


def _classify_outcome(kill_reason, returncode):
    if kill_reason == "no_progress":
        return "hang"
    if kill_reason == "backstop":
        return "incomplete"
    return "pass" if returncode == 0 else "fail"


def _is_failure(outcome):
    return outcome in ("fail", "hang")


def run_once(binary, config, timeout, mem_limit_bytes, no_progress_seconds):
    kill_reason, returncode, stdout, stderr = _capture(
        build_argv(binary, config), timeout, mem_limit_bytes,
        no_progress_seconds)
    if kill_reason is not None:
        return RunResult(_classify_outcome(kill_reason, None), None, None,
                         stdout, stderr)
    sig = -returncode if returncode < 0 else None
    return RunResult(_classify_outcome(None, returncode), returncode, sig,
                     stdout, stderr)


def run_under_lldb(binary, lldb, config, timeout, mem_limit_bytes,
                   no_progress_seconds):
    kill_reason, _rc, stdout, stderr = _capture(
        lldb_argv(lldb, build_argv(binary, config)), timeout, mem_limit_bytes,
        no_progress_seconds)
    if kill_reason is not None:
        return RunResult(_classify_outcome(kill_reason, None), None, None,
                         stdout, stderr)
    combined = stdout + "\n" + stderr
    crash = re.search(r"stop reason = signal (\w+)", combined)
    if crash is not None:
        return RunResult("fail", None, crash.group(1), stdout, stderr)
    code = lldb_exit_code(combined)
    if code is None:
        return RunResult("fail", None, "crash", stdout, stderr)
    return RunResult("pass" if code == 0 else "fail", code, None, stdout, stderr)


def probe_max_threads(binary):
    cmd = [binary, "--ponymaxthreads", "1000000", "--ponynoscale",
           "--datagrams", "1", "--clients", "1"]
    try:
        result = subprocess.run(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=30)
        text = (result.stdout.decode(errors="replace")
                + result.stderr.decode(errors="replace"))
        match = re.search(r"> (\d+)\)", text)
        if match is not None:
            return int(match.group(1))
    except subprocess.TimeoutExpired:
        pass
    die("could not read the core-count ceiling from the runtime (probe output did "
        "not match); refusing to guess with os.cpu_count(), which counts logical "
        "cores and would false-fail every seed on an SMT host")


def summary_line(config, result):
    parsed = parse_result(result.stdout)
    shape = config["workload"]
    detail = ("clients=%s client_sent=%s server_received=%s "
              "server_corrupted=%s bind_failed=%s"
              % (parsed.get("clients", "?"), parsed.get("client_sent", "?"),
                 parsed.get("server_received", "?"),
                 parsed.get("server_corrupted", "?"),
                 parsed.get("bind_failed", "?")))
    return ("[seed %d] %s (payload=%s datagrams=%s batch=%s clients=%s) %s"
            % (config["master_seed"], result.outcome.upper(),
               shape["payload-size"], shape["datagrams"],
               shape["batch-size"], shape["clients"], detail))



def bundle_for(config, binary, argv, limits, result):
    return {
        "master_seed": config["master_seed"],
        "workload": config["workload"],
        "runtime_flags": config["runtime"],
        "cli": " ".join(shlex.quote(a) for a in argv),
        "binary": binary,
        "limits": limits,
        "outcome": result.outcome,
        "returncode": result.returncode,
        "signal": result.signal,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }


def write_bundle(out_dir, bundle):
    path = os.path.join(out_dir, "bundle-%d.json" % bundle["master_seed"])
    with open(path, "w") as handle:
        json.dump(bundle, handle, indent=2, sort_keys=True)
    return path


def execute(binary, config, out_dir, timeout, mem_limit_bytes,
            no_progress_seconds, lldb):
    if lldb is not None:
        result = run_under_lldb(binary, lldb, config, timeout, mem_limit_bytes,
                                no_progress_seconds)
    else:
        result = run_once(binary, config, timeout, mem_limit_bytes,
                          no_progress_seconds)
    info(summary_line(config, result))
    if result.stdout:
        info(result.stdout.rstrip("\n"))
    if result.stderr:
        info(result.stderr.rstrip("\n"))
    if _is_failure(result.outcome):
        argv = (lldb_argv(lldb, build_argv(binary, config)) if lldb is not None
                else build_argv(binary, config))
        limits = {"timeout_seconds": timeout, "mem_limit_bytes": mem_limit_bytes}
        path = write_bundle(out_dir,
                            bundle_for(config, binary, argv, limits, result))
        info("wrote failure bundle: " + path)
    return result


def resolve_seeds(args):
    if args.master_seed is not None:
        return [args.master_seed]
    if args.replay is not None:
        return [args.replay]
    if args.count is not None:
        return list(range(args.start, args.start + args.count))
    if args.seeds is not None:
        try:
            return [int(token) for token in args.seeds.split(",")]
        except ValueError:
            die("bad --seeds (expected comma-separated integers): " + args.seeds)
    return [args.start]


def validate_args(no_progress_seconds, timeout_seconds, mem_limit_mb):
    if no_progress_seconds <= 5:
        die("--no-progress-seconds must exceed the engine's 5s heartbeat interval "
            "(got %d)" % no_progress_seconds)
    if timeout_seconds <= no_progress_seconds:
        die("--timeout-seconds (%d) must exceed --no-progress-seconds (%d)"
            % (timeout_seconds, no_progress_seconds))
    if mem_limit_mb < 0:
        die("--mem-limit-mb must be >= 0 (0 disables the cap); got %d" % mem_limit_mb)


def detect_ssl():
    """Probe the installed SSL library and return the ponyc -D flag."""
    for name in ("openssl", "libressl"):
        try:
            result = subprocess.run([name, "version"], capture_output=True,
                                    text=True, timeout=5)
            if result.returncode == 0:
                ver = result.stdout.strip()
                if ver.lower().startswith("libressl"):
                    return "-Dlibressl"
                if ver.startswith("OpenSSL 4."):
                    return "-Dopenssl_4.0.x"
                if ver.startswith("OpenSSL 3."):
                    return "-Dopenssl_3.0.x"
                if ver.startswith("OpenSSL 1.1."):
                    return "-Dopenssl_1.1.x"
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    die("could not detect SSL library; pass --ssl explicitly")
    return ""  # unreachable


def compile_engine(ponyc, ssl_flag, out_dir):
    env = dict(os.environ, PONYPATH=os.path.join(REPO_ROOT, "packages"))
    info("+ compiling engine with " + ponyc + " (" + ssl_flag + ")")
    result = subprocess.run([ponyc, "-d", ssl_flag, "-b", BINARY_NAME,
                             "--pic", "-o", out_dir, SOURCE_DIR], env=env)
    if result.returncode != 0:
        die("compiling the engine failed")
    binary = os.path.join(out_dir, BINARY_NAME)
    if os.name == "nt":
        binary += ".exe"
    if not os.path.isfile(binary):
        die("engine binary not produced at " + binary)
    return binary


def main():
    parser = argparse.ArgumentParser(description="Swarm UDP stress orchestrator")
    engine = parser.add_mutually_exclusive_group(required=True)
    engine.add_argument("--ponyc",
                        help="path to a debug ponyc (compiles the engine)")
    engine.add_argument("--binary",
                        help="path to a prebuilt debug engine")
    parser.add_argument("--ssl", choices=list(SSL_FLAG_MAP.keys()),
                        help="SSL version (auto-detected if omitted)")
    parser.add_argument("--out", default=os.path.join(os.path.expanduser("~"),
                        "tmp", "udp-swarm-out"), help="output dir for bundles")
    parser.add_argument("--lldb", default=None,
                        help="run each seed under this lldb (crash backtraces)")
    parser.add_argument("--start", type=int, default=0,
                        help="first seed for --count / --budget-seconds")
    selector = parser.add_mutually_exclusive_group()
    selector.add_argument("--count", type=int, default=None,
                          help="run --count seeds from --start")
    selector.add_argument("--master-seed", type=int, default=None,
                          help="run this single seed")
    selector.add_argument("--seeds", default=None,
                          help="run this comma-separated list of seeds")
    selector.add_argument("--replay", type=int, default=None,
                          help="reproduce this seed's draw; note that "
                               "--ponymaxthreads and --clients are clamped "
                               "against the local core count, so the config "
                               "can differ across hosts")
    selector.add_argument("--budget-seconds", type=int, default=None,
                          help="run seeds from --start until this many seconds "
                               "pass")
    parser.add_argument("--no-progress-seconds", type=int,
                        default=DEFAULT_NO_PROGRESS_SECONDS,
                        help="hang threshold: fail a run whose reported count has "
                             "not advanced for this long")
    parser.add_argument("--timeout-seconds", type=int,
                        default=DEFAULT_TIMEOUT_SECONDS,
                        help="non-failing backstop: a run still advancing at this "
                             "point is stopped (outcome 'incomplete'), not failed")
    parser.add_argument("--mem-limit-mb", type=int, default=DEFAULT_MEM_LIMIT_MB)
    args = parser.parse_args()
    validate_args(args.no_progress_seconds, args.timeout_seconds,
                  args.mem_limit_mb)

    if args.ponyc:
        ssl_flag = SSL_FLAG_MAP[args.ssl] if args.ssl else detect_ssl()
        binary = compile_engine(args.ponyc, ssl_flag, args.out)
    else:
        binary = args.binary
        if not os.path.isfile(binary):
            die("engine binary not found at " + binary
                + " (build it with `ponyc -d -o build/debug test/rt-stress/udp-swarm`)")
    os.makedirs(args.out, exist_ok=True)
    max_threads = probe_max_threads(binary)
    info("probed max threads: %d" % max_threads)
    mem_limit_bytes = (args.mem_limit_mb * 1024 * 1024
                       if args.mem_limit_mb > 0 else None)

    failures = []
    profile = "default"

    def run_seed(seed):
        config = resolve_config(seed, max_threads, profile)
        result = execute(binary, config, args.out,
                         args.timeout_seconds, mem_limit_bytes,
                         args.no_progress_seconds, args.lldb)
        if _is_failure(result.outcome):
            failures.append(seed)

    if args.budget_seconds is not None:
        if args.budget_seconds <= 0:
            die("--budget-seconds must be positive (got %d)" % args.budget_seconds)
        start_time = time.monotonic()
        seed = args.start
        while (time.monotonic() - start_time) < args.budget_seconds:
            run_seed(seed)
            seed += 1
    else:
        for seed in resolve_seeds(args):
            run_seed(seed)

    if failures:
        die("failures on seeds: " + ", ".join(str(s) for s in failures))
    info("all seeds passed")


if __name__ == "__main__":
    main()
