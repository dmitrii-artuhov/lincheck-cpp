#!/usr/bin/env python3
import os
import subprocess

import click

file_dir = os.path.join(
    os.path.dirname(__file__))

artifacts_dir = os.path.join(file_dir, "artifacts")
runtime_dir = os.path.join(file_dir, "..", "runtime")

# Don't forget rebuild llvm pass after changes.
llvm_plugin_path = os.path.join(
    file_dir, "..", "build", "codegen", "CoroGenPass.so")

deps = list(map(lambda f: os.path.join(runtime_dir, f), [
    "lib.cpp", "scheduler.cpp", "lin_check.cpp", "logger.cpp",
]))

clang = "clang++"
opt = "opt"
llvm_dis = "llvm-dis"
build_flags = ["-O3", "-std=c++2a"]


def read_file(path):
    with open(path) as f:
        return f.read()


def write_file(path, content):
    with open(path, "w") as f:
        f.write(content)


def run_command_and_get_output(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=None,
        env=None,
        input=None,
):
    if env is None:
        env = os.environ
    if cwd is not None:
        env["PWD"] = str(cwd)
    process = subprocess.Popen(
        cmd,
        env=env,
        cwd=cwd,
        stderr=stderr,
        stdout=stdout,
        stdin=subprocess.PIPE,
    )

    if input is not None:
        input = bytes(input, 'utf-8')
    out, _ = process.communicate(input)
    out = out.decode('utf-8')

    # This print is here to make running tests with -s flag more verbose
    print(out)

    return process.returncode, out


@click.group()
def cmd():
    pass


@cmd.command()
@click.option("-t", "--threads", help="threads count", type=int)
@click.option("-s", "--strategy", help="strategy name", type=str)
@click.option("--tasks", help="tasks per round", type=int)
@click.option("-r", "--rounds", help="number of rounds", type=int)
@click.option("-v", "--verbose", help="verbose output", type=bool,
              is_flag=True)
def run(threads, tasks, strategy, rounds, verbose):
    if not os.path.exists(os.path.join(artifacts_dir, "run")):
        print("firstly, build run")
        return
    threads = threads or 2
    strategy = strategy or "rr"
    tasks = tasks or 15
    rounds = rounds or 5
    args = list(
        map(str, [threads, strategy, tasks, rounds, 1 if verbose else 0]))
    cmd = ["./run"]
    cmd.extend(args)
    run_command_and_get_output(cmd, cwd=artifacts_dir)


@cmd.command()
@click.option("-s", "--src", required=True, help="source directory name")
@click.option("-g", "--debug", help="build with -g", type=bool, is_flag=True)
def build(src, debug):
    src_dir = os.path.join(file_dir, src)
    if not os.path.exists(os.path.join(src_dir, "spec.h")):
        print("spec.h must exist in src")
        return
    if not os.path.exists(os.path.join(src_dir, "target.cpp")):
        print("target.cpp must exist in src")
        return

    run_source_path = os.path.join(runtime_dir, "run.cpp")
    # Replace included spec to the specified.
    run_content = read_file(run_source_path)
    run_content = f'#include "../{src}/spec.h"\n{run_content}'
    run_path = os.path.join(artifacts_dir, "run.cpp")
    write_file(run_path, run_content)

    # Create artifacts dir.
    if not os.path.exists(artifacts_dir):
        os.mkdir(artifacts_dir)

    # Build target.
    cmd = [clang]
    cmd.extend(build_flags)
    cmd.extend(["-c", "-emit-llvm", "target.cpp",
                "-o", os.path.join(artifacts_dir, "bytecode.bc")])
    rc, _ = run_command_and_get_output(cmd, cwd=src_dir)
    assert rc == 0

    # Run llvm pass.
    res_bytecode_path = os.path.join(artifacts_dir, "res.bc")
    cmd = [opt, "--load-pass-plugin", llvm_plugin_path,
           "-passes=coro_gen", "bytecode.bc", "-o", "res.bc"]
    rc, _ = run_command_and_get_output(cmd, cwd=artifacts_dir)
    assert rc == 0

    # Run llvm-dis (debug purposes).
    cmd = [llvm_dis, "res.bc", "-o", "res.ll"]
    rc, _ = run_command_and_get_output(cmd, cwd=artifacts_dir)
    assert rc == 0

    # Build run.cpp.
    cmd = [clang, "-DCLI_BUILD", f"-I{runtime_dir}"]
    cmd.extend(build_flags)
    if debug:
        cmd.extend(["-g"])
    cmd.extend([run_path])
    cmd.extend(deps)
    cmd.extend([res_bytecode_path])
    cmd.extend(["-o", os.path.join(artifacts_dir, "run")])
    rc, _ = run_command_and_get_output(cmd, cwd=artifacts_dir)
    assert rc == 0


cli = click.CommandCollection(sources=[cmd])

if __name__ == "__main__":
    cli()
