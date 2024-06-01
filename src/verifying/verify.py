#!/usr/bin/env python3
import os
import subprocess
import sys

import click

file_dir = os.path.join(
    os.path.dirname(__file__))

artifacts_dir_default = os.path.join(file_dir, "artifacts")
runtime_dir = os.path.join(file_dir, "..", "runtime")

# Don't forget rebuild llvm passes after changes.
llvm_plugin_path = os.path.join(
    file_dir, "..", "build", "codegen", "CoroGenPass.so")

yield_plugh_path = os.path.join(
    file_dir, "..", "build", "codegen", "YieldPass.so")

deps = list(map(lambda f: os.path.join(runtime_dir, f), [
    "lib.cpp", "scheduler.cpp", "lin_check.cpp", "logger.cpp",
    "verifying.cpp", "generators.cpp", "pretty_printer.cpp",
]))

clang = "clang++"
opt = "opt"
llvm_dis = "llvm-dis"
# build_flags = ["-O3", "-std=c++2a"]
build_flags1 = ["-g", "-std=c++20", "-O0", "-fno-omit-frame-pointer", "-DADDRESS_SANITIZER", "-fsanitize=address"]
build_flags2 = ["-g", "-std=c++20", "-O0", "-ggdb3"]
# build_flags = ["-g", "-std=c++2a", "-fsanitize=address", "-DADDRESS_SANITIZER -fsanitize-address-use-after-return=always"]


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
    print(out)
    return process.returncode, out


@click.group()
def cmd():
    pass


@cmd.command()
@click.option("-t", "--threads", help="threads count", type=int)
@click.option("--tasks", help="tasks per round", type=int)
@click.option("--switches", help="max switches per round", type=int,
              default=None)
@click.option("-r", "--rounds", help="number of rounds", type=int)
@click.option("-v", "--verbose", help="verbose output", type=bool,
              is_flag=True)
@click.option("-s", "--strategy", type=str)
@click.option("-w", "--weights", help="weights for random strategy", type=str)
def run(threads, tasks, switches, rounds, verbose, strategy, weights):
    if not os.path.exists(os.path.join(artifacts_dir_default, "run")):
        print("firstly, build run")
        return

    threads = threads or 2
    tasks = tasks or 15
    if switches != 0:
        switches = switches or 100000000
    rounds = rounds or 5
    strategy = strategy or "rr"
    weights = weights or ""
    args = list(
        map(str, [threads, tasks, switches, rounds, 1 if verbose else 0,
                  strategy, weights]))
    cmd = ["./run"]
    cmd.extend(args)
    subprocess.run(cmd, cwd=artifacts_dir_default, stdout=sys.stdout)


# Check src/Makefle to understand debug build logic and unsafe reasonable.


def run_build(src, artifacts_dir, debug=False):
    cmd = [clang]
    cmd.extend(build_flags)
    if debug:
        cmd.append("-g")
    cmd.append(f"-fpass-plugin={yield_plugh_path}")
    cmd.extend(deps)
    cmd.append(src.name)
    cmd.extend(["-o", os.path.join(artifacts_dir, "run")])
    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0


def run_build_on_optimized(src, artifacts_dir, debug=False):
    # Compile target to bytecode.
    cmd = [clang]
    cmd.extend(build_flags2)
    if debug:
        cmd.append("-g")
    cmd.extend(["-I", "/home/src/verifying/targets/vk_channel"])
    cmd.extend(["-I", "/home/src/verifying/targets/vk_channel"])
    cmd.extend(["/home/src/verifying/targets/vk_channel/channel.cpp"])
    cmd.extend(["/home/src/verifying/targets/vk_channel/hazard-ptr.cpp"])
    cmd.extend(["/home/src/verifying/targets/vk_channel/indexer.cpp"])
    cmd.extend(["/home/src/verifying/targets/vk_channel/lock-free-list.cpp"])
    cmd.extend(["/home/src/verifying/targets/vk_channel/precise-time.cpp"])
    cmd.extend(["/home/src/verifying/targets/vk_channel/scheduler.cpp"])
    cmd.extend(["/home/src/verifying/targets/vk_channel/time-point-handle.cpp"])
    cmd.extend(["-emit-llvm", "-S"])
    cmd.append(src.name)
    # cmd.extend(["-o", artifacts_dir])
    # cmd.extend(["-o", os.path.join(artifacts_dir, "bytecode.bc")])
    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0

    # Run yield pass on optimized code.
    cmd = [clang]
    cmd.extend(build_flags1)
    if debug:
        cmd.append("-g")
    cmd.append(f"-fpass-plugin={yield_plugh_path}")
    cmd.extend(deps)
    cmd.extend(["channel.ll"])
    cmd.extend(["hazard-ptr.ll"])
    cmd.extend(["indexer.ll"])
    cmd.extend(["lock-free-list.ll"])
    cmd.extend(["precise-time.ll"])
    cmd.extend(["scheduler.ll"])
    cmd.extend(["time-point-handle.ll"])
    # cmd.append(os.path.join(artifacts_dir, "bytecode.bc"))
    cmd.extend(["-o", os.path.join(artifacts_dir, "run")])
    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0


@cmd.command()
@click.option("-s", "--src", required=True, help="source path",
              type=click.File("r"))
@click.option("-g", "--debug", help="debug build", type=bool, is_flag=True)
@click.option("-a", "--artifacts_dir", help="dir for artifacts", type=str,
              default=None)
@click.option("--no-optimized", help="run pass on unopimized code", type=bool,
              is_flag=True)
def build(src, debug, no_optimized, artifacts_dir):
    artifacts_dir = artifacts_dir or artifacts_dir_default

    # Create artifacts dir.
    if not os.path.exists(artifacts_dir):
        os.mkdir(artifacts_dir)

    if no_optimized:
        print("building non optimized")
        run_build(src, artifacts_dir, debug)
    else:
        print("building...")
        run_build_on_optimized(src, artifacts_dir, debug)
    print("OK")


cli = click.CommandCollection(sources=[cmd])

if __name__ == "__main__":
    cli()
