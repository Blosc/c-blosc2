"""Benchmark an isolated Python-Blosc2 package: conda Python -S this_file --package PATH."""

import argparse
import gc
import json
import statistics
import sys
import sysconfig
import time
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--package", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--size", type=int, default=1_000_000_000)
parser.add_argument("--repeat", type=int, default=5)
args = parser.parse_args()
sys.path[:0] = [str(args.package.resolve()), sysconfig.get_paths()["purelib"]]
import blosc2
import numpy as np

assert Path(blosc2.__file__).is_relative_to(args.package.resolve())
assert Path(blosc2.blosc2_ext.__file__).is_relative_to(args.package.resolve())
print(blosc2.__file__, blosc2.blosc2_ext.__file__, flush=True)
results = {"package": str(args.package.resolve()), "size": args.size, "results": []}
for codec in [blosc2.Codec.BLOSCLZ, blosc2.Codec.LZ4, blosc2.Codec.ZSTD]:
    create, read = [], []
    for repeat in range(args.repeat + 1):
        gc.collect()
        start_cpu, start = time.process_time(), time.perf_counter()
        array = blosc2.arange(float(args.size), chunks=(2_000_000,), blocks=(16_000,),
                              cparams={"codec": codec, "clevel": 1, "nthreads": 12,
                                       "filters": [blosc2.Filter.BITSHUFFLE]},
                              dparams={"nthreads": 12})
        timing = [time.perf_counter() - start, time.process_time() - start_cpu]
        if repeat: create.append(timing)
        cbytes = array.schunk.cbytes
        start_cpu, start = time.process_time(), time.perf_counter()
        values = array[:]
        timing = [time.perf_counter() - start, time.process_time() - start_cpu]
        if repeat: read.append(timing)
        indices = np.array([0, 1, 15999, 16000, 1999999, 2000000, args.size // 2, args.size - 1])
        np.testing.assert_array_equal(values[indices], indices.astype(np.float64))
        del values, array
    row = {"codec": codec.name, "cbytes": cbytes, "create": create, "read": read,
           "create_wall_median": statistics.median(t[0] for t in create),
           "read_wall_median": statistics.median(t[0] for t in read)}
    results["results"].append(row)
    print(json.dumps(row), flush=True)
args.output.write_text(json.dumps(results, indent=2) + "\n")
