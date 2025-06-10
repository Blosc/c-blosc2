from pathlib import Path
import re
import os
import subprocess
from typing import Union

import numpy as np
import argparse


def run_command(command: str, return_output: bool = False) -> Union[tuple[list[float], str], tuple[list[float]]]:
    bench_folder = Path(__file__).parent.parent / "build" / "bench"
    if os.name == "nt":
        bench_folder /= "Release"
    res = subprocess.run(f"{bench_folder}/{command}", shell=True, capture_output=True)
    assert res.returncode == 0, f"\nstdout: {res.stdout.decode('utf-8')}\nstderr: {res.stderr.decode('utf-8')}"

    out = res.stdout.decode("utf-8")
    numbers = []
    for number in re.findall(r"-?\d+(?:\.\d+)?", out):
        numbers.append(float(number))
    assert len(numbers) > 0, f"Could not find any numbers in the output:\n{out}"
    
    if return_output:
        return numbers, out
    else:
        return numbers


class ReplaceNumbers:
    def __init__(self, numbers: np.ndarray):
        self.numbers = numbers
        self.index = 0

    def __call__(self, match: re.Match) -> str:
        assert match.group() == "NUMBER"
        number = self.numbers[self.index]
        self.index += 1

        if number.is_integer():
            return str(int(number))
        else:
            return f"{number:.3f}"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=(
            "Repeat multiple runs of a benchmark and aggregate all numbers in the output (the outputs from all runs must "
            "have the same format)."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--command",
        required=True,
        type=str,
        help="The command to execute (e.g. sframe_bench 1000 insert 1000 io_mmap).",
    )
    parser.add_argument(
        "--warmups",
        default=3,
        required=False,
        type=int,
        help="Number of warmup runs.",
    )
    parser.add_argument(
        "--runs",
        default=30,
        required=False,
        type=int,
        help="Number of runs to perform and aggregate the output from.",
    )
    args = parser.parse_args()

    for i in range(args.warmups):
        print(f"Warmup {i+1}/{args.warmups}", end="\r")
        run_command(args.command)
    print()

    # Collect numbers from outputs of multiple runs
    example_output = None
    numbers = []
    for i in range(args.runs):
        print(f"Run {i+1}/{args.runs}", end="\r")

        if example_output is None:
            values, text = run_command(args.command, return_output=True)
            example_output = text
            numbers.append(values)
        else:
            numbers.append(run_command(args.command))
    print()
    
    numbers = np.asarray(numbers)
    numbers = np.min(numbers, axis=0)

    assert example_output is not None, "No output was collected."
    example_output = re.sub(r"-?\d+(?:\.\d+)?", "NUMBER", example_output)
    aggregated_output = re.sub("NUMBER", ReplaceNumbers(numbers), example_output)
    print(aggregated_output)
