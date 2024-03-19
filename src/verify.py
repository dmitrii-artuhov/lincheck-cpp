#!/usr/bin/env python3
import os
import subprocess

import click


@click.command()
@click.option("-t", "--threads", help="threads count", type=int)
@click.option("-s", "--strategy", help="strategy name", type=str)
@click.option("--tasks", help="tasks per round", type=int)
@click.option("-r", "--rounds", help="number of rounds", type=int)
@click.option("-v", "--verbose", help="verbose output", type=bool,
              is_flag=True)
def main(threads, tasks, strategy, rounds, verbose):
    threads = threads or 2
    strategy = strategy or "rr"
    tasks = tasks or 15
    rounds = rounds or 5
    args = list(
        map(str, [threads, strategy, tasks, rounds, 1 if verbose else 0]))
    cmd = ["./run"]
    cmd.extend(args)
    subprocess.run(cmd, cwd=os.path.join(
        os.path.dirname(__file__), "verifying"))


if __name__ == "__main__":
    main()
