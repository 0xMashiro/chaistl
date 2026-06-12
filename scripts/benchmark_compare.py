#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# benchmark_compare.py - run, save, and compare chaistl benchmark baselines.

import argparse
import datetime as _datetime
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


DEFAULT_BINARY = "build/clang-benchmark/benchmark/chaistl_benchmarks"
DEFAULT_REPETITIONS = 5
DEFAULT_THRESHOLD = 10.0
TIME_UNITS_TO_NS = {
    "s": 1_000_000_000.0,
    "ms": 1_000_000.0,
    "us": 1_000.0,
    "ns": 1.0,
}


class BenchmarkError(Exception):
    pass


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 2


def load_json(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as stream:
            data = json.load(stream)
    except OSError as exc:
        raise BenchmarkError(f"cannot read {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise BenchmarkError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise BenchmarkError(f"{path}: top-level JSON value must be an object")
    if "benchmarks" not in data:
        raise BenchmarkError(f"{path}: missing required 'benchmarks' field")
    if not isinstance(data["benchmarks"], list):
        raise BenchmarkError(f"{path}: 'benchmarks' must be a list")
    return data


def benchmark_base_name(entry: dict) -> str:
    name = require_string(entry, "name")
    if is_median_entry(entry):
        if name.endswith("_median"):
            return name[: -len("_median")]
        run_name = entry.get("run_name")
        if isinstance(run_name, str) and run_name:
            return run_name
    return name


def aggregate_base_name(entry: dict) -> str:
    name = require_string(entry, "name")
    aggregate_name = entry.get("aggregate_name")
    if isinstance(aggregate_name, str) and name.endswith("_" + aggregate_name):
        return name[: -(len(aggregate_name) + 1)]
    run_name = entry.get("run_name")
    if isinstance(run_name, str) and run_name:
        return run_name
    return name


def is_median_entry(entry: dict) -> bool:
    name = entry.get("name")
    if isinstance(name, str) and name.endswith("_median"):
        return True
    return (
        entry.get("run_type") == "aggregate"
        and entry.get("aggregate_name") == "median"
    )


def require_string(entry: dict, field: str) -> str:
    value = entry.get(field)
    if not isinstance(value, str) or not value:
        raise BenchmarkError(f"benchmark entry missing string field '{field}'")
    return value


def require_number(entry: dict, field: str, name: str) -> float:
    value = entry.get(field)
    if not isinstance(value, (int, float)):
        raise BenchmarkError(f"{name}: missing numeric field '{field}'")
    return float(value)


def normalize_benchmarks(data: dict, metric: str, path: Path) -> dict:
    median_keys = set()
    for entry in data["benchmarks"]:
        if not isinstance(entry, dict):
            raise BenchmarkError(f"{path}: each benchmark entry must be an object")
        if is_median_entry(entry):
            median_keys.add(benchmark_base_name(entry))

    normalized = {}
    for entry in data["benchmarks"]:
        name = require_string(entry, "name")
        if is_median_entry(entry):
            key = benchmark_base_name(entry)
        elif entry.get("run_type") == "aggregate" and aggregate_base_name(entry) in median_keys:
            continue
        else:
            key = name

        unit = require_string(entry, "time_unit")
        if unit not in TIME_UNITS_TO_NS:
            raise BenchmarkError(f"{path}: {name}: unsupported time_unit '{unit}'")
        value = require_number(entry, metric, name) * TIME_UNITS_TO_NS[unit]
        if key in normalized:
            raise BenchmarkError(f"{path}: duplicate normalized benchmark name '{key}'")
        normalized[key] = {
            "value_ns": value,
            "display_value": require_number(entry, metric, name),
            "time_unit": unit,
        }
    return normalized


def format_time(value_ns: float) -> str:
    abs_value = abs(value_ns)
    if abs_value >= TIME_UNITS_TO_NS["s"]:
        return f"{value_ns / TIME_UNITS_TO_NS['s']:.3f}s"
    if abs_value >= TIME_UNITS_TO_NS["ms"]:
        return f"{value_ns / TIME_UNITS_TO_NS['ms']:.3f}ms"
    if abs_value >= TIME_UNITS_TO_NS["us"]:
        return f"{value_ns / TIME_UNITS_TO_NS['us']:.3f}us"
    return f"{value_ns:.3f}ns"


def delta_percent(baseline_ns: float, contender_ns: float) -> float:
    if baseline_ns == 0:
        if contender_ns == 0:
            return 0.0
        return float("inf")
    return ((contender_ns - baseline_ns) / baseline_ns) * 100.0


def render_table(rows: list[tuple[str, float, float, float]]) -> list[str]:
    headers = ("name", "baseline", "contender", "delta")
    rendered = [
        (name, format_time(base), format_time(cont), format_delta(delta))
        for name, base, cont, delta in rows
    ]
    widths = [
        max(len(headers[index]), *(len(row[index]) for row in rendered))
        if rendered
        else len(headers[index])
        for index in range(4)
    ]
    lines = [
        f"{headers[0]:<{widths[0]}} | {headers[1]:>{widths[1]}} | "
        f"{headers[2]:>{widths[2]}} | {headers[3]:>{widths[3]}}",
        f"{'-' * widths[0]} | {'-' * widths[1]} | "
        f"{'-' * widths[2]} | {'-' * widths[3]}",
    ]
    for row in rendered:
        lines.append(
            f"{row[0]:<{widths[0]}} | {row[1]:>{widths[1]}} | "
            f"{row[2]:>{widths[2]}} | {row[3]:>{widths[3]}}"
        )
    return lines


def format_delta(delta: float) -> str:
    if delta == float("inf"):
        return "+inf%"
    if delta == float("-inf"):
        return "-inf%"
    return f"{delta:+.2f}%"


def color_red(text: str) -> str:
    if sys.stdout.isatty():
        return f"\033[31m{text}\033[0m"
    return text


def print_section(title: str, rows: list[tuple[str, float, float, float]], red: bool = False) -> None:
    if not rows:
        return
    heading = color_red(title) if red else title
    print()
    print(heading)
    for line in render_table(rows):
        print(color_red(line) if red else line)


def compare_files(baseline_path: Path, contender_path: Path, threshold: float, metric: str) -> int:
    baseline = normalize_benchmarks(load_json(baseline_path), metric, baseline_path)
    contender = normalize_benchmarks(load_json(contender_path), metric, contender_path)

    matched_rows = []
    regressions = []
    improvements = []
    for name in sorted(set(baseline) & set(contender)):
        base_ns = baseline[name]["value_ns"]
        cont_ns = contender[name]["value_ns"]
        delta = delta_percent(base_ns, cont_ns)
        row = (name, base_ns, cont_ns, delta)
        matched_rows.append(row)
        if delta > threshold:
            regressions.append(row)
        elif delta < -threshold:
            improvements.append(row)

    unmatched = []
    for name in sorted(set(baseline) - set(contender)):
        unmatched.append(f"baseline only:  {name}")
    for name in sorted(set(contender) - set(baseline)):
        unmatched.append(f"contender only: {name}")

    print(f"metric: {metric}")
    print(f"threshold: {threshold:g}%")
    print(f"matched: {len(matched_rows)}")
    for line in render_table(matched_rows):
        print(line)

    print_section("REGRESSIONS", regressions, red=True)
    print_section("IMPROVEMENTS", improvements)
    if unmatched:
        print()
        print("UNMATCHED")
        for line in unmatched:
            print(line)

    return 1 if regressions else 0


def command_run(args: argparse.Namespace) -> int:
    root = repo_root()
    binary = Path(args.binary)
    if not binary.is_absolute():
        binary = root / binary
    out = Path(args.output)
    command = [
        str(binary),
        f"--benchmark_out={out}",
        "--benchmark_out_format=json",
        f"--benchmark_repetitions={args.repetitions}",
        "--benchmark_report_aggregates_only=true",
    ]
    if args.filter:
        command.append(f"--benchmark_filter={args.filter}")
    try:
        return subprocess.call(command)
    except OSError as exc:
        raise BenchmarkError(f"cannot run benchmark binary {binary}: {exc}") from exc


def compiler_name_from_context(data: dict) -> str | None:
    context = data.get("context")
    if not isinstance(context, dict):
        return None
    fields = ("compiler", "compiler_id", "compiler_version", "cxx_compiler")
    text = " ".join(str(context[field]) for field in fields if field in context)
    match = re.search(r"\b(clang|gcc|appleclang)[^\d]*(\d+)", text, re.IGNORECASE)
    if match:
        name = match.group(1).lower()
        if name == "appleclang":
            name = "appleclang"
        return f"{name}{match.group(2)}"
    return None


def command_save(args: argparse.Namespace) -> int:
    source = Path(args.result)
    data = load_json(source)
    name = args.name or compiler_name_from_context(data)
    if not name:
        raise BenchmarkError("compiler information not found in JSON context; pass --name")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", name):
        raise BenchmarkError("--name may contain only letters, digits, '_', '-', and '.'")

    today = _datetime.date.today().isoformat()
    destination = repo_root() / "benchmark" / "baselines" / f"{today}-{name}.json"
    if destination.exists() and not args.force:
        raise BenchmarkError(f"{destination} already exists; pass --force to overwrite")
    try:
        shutil.copyfile(source, destination)
    except OSError as exc:
        raise BenchmarkError(f"cannot copy {source} to {destination}: {exc}") from exc
    print(destination)
    return 0


def command_compare(args: argparse.Namespace) -> int:
    return compare_files(Path(args.baseline), Path(args.contender), args.threshold, args.metric)


def latest_baseline() -> Path:
    baseline_dir = repo_root() / "benchmark" / "baselines"
    try:
        candidates = sorted(path for path in baseline_dir.iterdir() if path.suffix == ".json")
    except OSError as exc:
        raise BenchmarkError(f"cannot read {baseline_dir}: {exc}") from exc
    if not candidates:
        raise BenchmarkError(f"no .json baselines found in {baseline_dir}")
    return candidates[-1]


def command_check(args: argparse.Namespace) -> int:
    baseline = Path(args.baseline) if args.baseline else latest_baseline()
    with tempfile.NamedTemporaryFile(prefix="chaistl-benchmark-", suffix=".json", delete=False) as tmp:
        contender = Path(tmp.name)
    run_args = argparse.Namespace(
        binary=DEFAULT_BINARY,
        filter=args.filter,
        repetitions=args.repetitions,
        output=str(contender),
    )
    run_code = command_run(run_args)
    if run_code != 0:
        try:
            contender.unlink()
        except OSError:
            pass
        return run_code
    try:
        return compare_files(baseline, contender, args.threshold, "real_time")
    finally:
        try:
            contender.unlink()
        except OSError:
            pass


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run and compare chaistl benchmark JSON results.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run = subparsers.add_parser("run", help="run the benchmark binary and write JSON")
    run.add_argument("--binary", default=DEFAULT_BINARY)
    run.add_argument("--filter")
    run.add_argument("--repetitions", type=positive_int, default=DEFAULT_REPETITIONS)
    run.add_argument("-o", "--output", required=True, metavar="OUT.json")
    run.set_defaults(func=command_run)

    save = subparsers.add_parser("save", help="save a benchmark result as a dated baseline")
    save.add_argument("result", metavar="RESULT.json")
    save.add_argument("--name")
    save.add_argument("--force", action="store_true")
    save.set_defaults(func=command_save)

    compare = subparsers.add_parser("compare", help="compare two benchmark JSON files")
    compare.add_argument("baseline", metavar="BASELINE.json")
    compare.add_argument("contender", metavar="CONTENDER.json")
    compare.add_argument("--threshold", type=nonnegative_float, default=DEFAULT_THRESHOLD)
    compare.add_argument("--metric", choices=("real_time", "cpu_time"), default="real_time")
    compare.set_defaults(func=command_compare)

    check = subparsers.add_parser("check", help="run benchmarks into a temporary file and compare")
    check.add_argument("--baseline")
    check.add_argument("--filter")
    check.add_argument("--threshold", type=nonnegative_float, default=DEFAULT_THRESHOLD)
    check.add_argument("--repetitions", type=positive_int, default=DEFAULT_REPETITIONS)
    check.set_defaults(func=command_check)

    return parser


def positive_int(text: str) -> int:
    try:
        value = int(text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("must be an integer") from exc
    if value <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return value


def nonnegative_float(text: str) -> float:
    try:
        value = float(text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("must be a number") from exc
    if value < 0:
        raise argparse.ArgumentTypeError("must be non-negative")
    return value


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except BenchmarkError as exc:
        return fail(str(exc))


if __name__ == "__main__":
    sys.exit(main())
