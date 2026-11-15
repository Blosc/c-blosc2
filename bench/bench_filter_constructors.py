"""Paired isolated-package arange/linspace filter benchmark (run with Python -S)."""

import argparse
import gc
import hashlib
import json
import statistics
import subprocess
import sys
import sysconfig
import time
from pathlib import Path


def worker(args):
    package = args.package.resolve()
    sys.path[:0] = [str(package), sysconfig.get_paths()["purelib"]]
    import blosc2
    import numpy as np

    assert Path(blosc2.__file__).is_relative_to(package)
    extension = Path(blosc2.blosc2_ext.__file__)
    assert extension.is_relative_to(package)
    print(json.dumps({"extension": str(extension),
                      "sha256": hashlib.sha256(extension.read_bytes()).hexdigest()}), flush=True)
    for line in sys.stdin:
        if line.strip() == "quit":
            return
        cell = json.loads(line)
        n, task, dtype = cell["size"], cell["task"], np.dtype(cell["dtype"])
        shape = (n,) if task != "linspace3d" else (n // 1_000_000, 1000, 1000)
        chunks = (16_000_000 // dtype.itemsize,)
        blocks = (128_000 // dtype.itemsize,)
        if task == "linspace3d":
            chunks = (16 // dtype.itemsize, 1000, 1000)
            blocks = (1, 80 // dtype.itemsize, 1000)
        filters = {"shuffle": [blosc2.Filter.SHUFFLE],
                   "bitshuffle": [blosc2.Filter.BITSHUFFLE],
                   "fused": [blosc2.Filter.SHUFFLE, blosc2.Filter.BYTEDELTA]}[cell["pipeline"]]
        kwargs = dict(dtype=dtype, chunks=chunks, blocks=blocks,
                      cparams=dict(codec=getattr(blosc2.Codec, cell["codec"]), clevel=cell["clevel"],
                                   filters=filters, nthreads=cell["threads"]))
        group = cell.get("group", 0)
        if group:
            kwargs["cparams"]["filters_meta"] = [group] * len(filters)
        gc.collect()
        start_cpu, start = time.process_time(), time.perf_counter()
        if task == "arange":
            array = blosc2.arange(float(n), **kwargs)
        else:
            array = blosc2.linspace(0, 1, n, shape=shape, **kwargs)
        wall, cpu = time.perf_counter() - start, time.process_time() - start_cpu
        indices = np.unique(np.array([0, 1, blocks[-1]-1, blocks[-1], 1999999, 2000000,
                                      n // 2, n-1]))
        expected = indices.astype(np.float64)
        if task != "arange":
            expected /= n - 1
        actual = [array[tuple(map(int, np.unravel_index(i, shape)))] for i in indices]
        np.testing.assert_allclose(actual, expected.astype(dtype), rtol=2*np.finfo(dtype).eps, atol=0)
        assert array.chunks == chunks and array.blocks == blocks
        result = dict(wall=wall, cpu=cpu, cbytes=array.schunk.cbytes,
                      chunks=chunks, blocks=blocks, cparams=str(array.cparams))
        del array
        print(json.dumps(result), flush=True)


def receive(process):
    line = process.stdout.readline()
    if not line:
        raise RuntimeError(f"Worker exited: {process.poll()}")
    return json.loads(line)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--package", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--candidate", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--size", type=int, default=100_000_000)
    parser.add_argument("--threads", nargs="+", type=int, default=[12])
    parser.add_argument("--tasks", nargs="+", choices=["arange", "linspace", "linspace3d"],
                        default=["arange", "linspace", "linspace3d"])
    parser.add_argument("--dtypes", nargs="+", choices=["float32", "float64"], default=["float32", "float64"])
    parser.add_argument("--pipelines", nargs="+", choices=["shuffle", "bitshuffle", "fused"],
                        default=["shuffle", "bitshuffle", "fused"])
    parser.add_argument("--codecs", nargs="+", choices=["LZ4", "ZSTD"], default=["LZ4", "ZSTD"])
    parser.add_argument("--clevels", nargs="+", type=int, default=[1])
    parser.add_argument("--groups", nargs="+", type=int, choices=[0, 2, 4, 8, 16], default=[0],
                        help="Explicit byte grouping for SHUFFLE and SHUFFLE+BYTEDELTA; 0 keeps defaults")
    parser.add_argument("--repeats", type=int, default=4)
    parser.add_argument("--sessions", type=int, default=2)
    args = parser.parse_args()
    if args.worker:
        if args.package is None:
            parser.error("--worker requires --package")
        worker(args)
        return
    if args.reference is None or args.candidate is None or args.output is None:
        parser.error("--reference, --candidate, and --output are required")
    if args.size < 4_000_000 or args.size % 1_000_000:
        parser.error("--size must be a multiple of 1000000 and at least 4000000")
    if min(args.threads) < 1 or args.repeats < 1 or args.sessions < 1:
        parser.error("threads, repeats, and sessions must be positive")
    cells = [dict(size=args.size, threads=threads, task=task, dtype=dtype, pipeline=pipeline,
                  codec=codec, clevel=clevel, group=group)
             for threads in args.threads for task in args.tasks for dtype in args.dtypes
             for pipeline in args.pipelines for codec in args.codecs for clevel in args.clevels
             for group in args.groups if pipeline != "bitshuffle" or group == 0]
    report = {"sessions": [], "summary": []}
    for session in range(args.sessions):
        processes, metadata, rows = {}, {}, []
        try:
            for label, package in [("before", args.reference), ("after", args.candidate)]:
                processes[label] = subprocess.Popen(
                    [sys.executable, "-S", __file__, "--worker", "--package", str(package)],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
                metadata[label] = receive(processes[label])
            for trial in range(args.repeats + 1):
                shift = (trial + session * 7) % len(cells)
                order = cells[shift:] + cells[:shift]
                if trial % 2:
                    order.reverse()
                for cell in order:
                    pair = {}
                    for label in (["before", "after"] if (trial+session) % 2 else ["after", "before"]):
                        process = processes[label]
                        process.stdin.write(json.dumps(cell) + "\n")
                        process.stdin.flush()
                        pair[label] = receive(process)
                    for key in ("cbytes", "chunks", "blocks", "cparams"):
                        assert pair["before"][key] == pair["after"][key], (cell, key, pair)
                    if trial:
                        rows.append(dict(cell, measurements=pair, trial=trial))
            report["sessions"].append(dict(metadata=metadata, rows=rows))
            args.output.write_text(json.dumps(report, indent=2) + "\n")
            print(f"Session {session+1} complete", flush=True)
        finally:
            for process in processes.values():
                if process.poll() is None:
                    process.stdin.write("quit\n")
                    process.stdin.flush()
                process.wait()
    for cell in cells:
        rows = [r for s in report["sessions"] for r in s["rows"] if all(r[k] == v for k,v in cell.items())]
        medians = {label: {key: statistics.median(r["measurements"][label][key] for r in rows)
                           for key in ("wall", "cpu")} for label in ("before", "after")}
        row = dict(cell, medians=medians, speedup=medians["before"]["wall"]/medians["after"]["wall"])
        report["summary"].append(row)
        print(json.dumps(row), flush=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
