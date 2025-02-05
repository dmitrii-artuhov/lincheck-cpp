import pandas as pd
import matplotlib.pyplot as plt
import re

def parse_benchmark_file(filename):
    """Parse the benchmark file into a structured dictionary."""
    strategies = {}
    with open(filename, "r") as file:
        lines = file.readlines()

    strategy_name = None
    for line in lines:
        line = line.strip()
        if not line:
            continue

        if "Total" in line:
            total_match = re.match(r"Total\s+(\d+)", line)
            if total_match:
                strategies["total_tasks"] = int(total_match.group(1))
        if not re.match(r"^\d", line):  # Line doesn't start with a number
            strategy_name = line
            strategies[strategy_name] = {"times": [], "results": []}
        elif strategy_name:
            # Parse numbers from the line
            numbers = list(map(float, line.split()))
            # First half for times, second half for results
            midpoint = len(numbers) // 2
            strategies[strategy_name]["times"].extend(numbers[:midpoint])
            strategies[strategy_name]["results"].extend(numbers[midpoint:])

    return strategies
