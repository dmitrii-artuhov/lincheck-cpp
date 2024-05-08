#!/usr/bin/env python3
import os
import subprocess
import sys

import click

file_dir = os.path.join(
    os.path.dirname(__file__))

artifacts_dir_default = os.path.join(file_dir, "artifacts")
runtime_dir = os.path.join(file_dir, "..", "runtime")

# Don't forget rebuild llvm pass after changes.
llvm_plugin_path = os.path.join(
    file_dir, "..", "build", "codegen", "CoroGenPass.so")

deps = list(map(lambda f: os.path.join(runtime_dir, f), [
    "lib.cpp", "scheduler.cpp", "lin_check.cpp", "logger.cpp",
    "verifying.cpp", "generators.cpp", "builders.cpp", "pretty_printer.cpp",
]))

clang = "clang++"
opt = "opt"
llvm_dis = "llvm-dis"
build_flags = ["-O3", "-std=c++2a"]
# build_flags = ["-O3", "-g", "-std=c++2a", "-fsanitize=address", "-DADDRESS_SANITIZER"]


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


def build_unsafe(src, artifacts_dir):
    # Compile target to bytecode.
    cmd = [clang]
    cmd.extend(build_flags)
    cmd.extend(["-emit-llvm", "-S", "-fno-discard-value-names"])
    cmd.append(src.name)
    cmd.extend(["-o", os.path.join(artifacts_dir, "bytecode.bc")])
    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0

    # Run llvm pass on optimized code.
    # Can lead to incostintency.
    cmd = [clang]
    cmd.extend(build_flags)
    cmd.append(f"-fpass-plugin={llvm_plugin_path}")
    cmd.extend(deps)
    cmd.append(os.path.join(artifacts_dir, "bytecode.bc"))
    cmd.extend(["-o", os.path.join(artifacts_dir, "run")])
    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0
    pass


def build_debug(src, artifacts_dir):
    # Compile to bytecode with pass.
    cmd = [clang]
    cmd.extend(build_flags)
    cmd.extend(["-emit-llvm", "-S"])
    cmd.append("-fno-discard-value-names")
    cmd.append(f"-fpass-plugin={llvm_plugin_path}")
    cmd.append(src.name)
    cmd.extend(["-o", os.path.join(artifacts_dir, "bytecode.bc")])
    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0

    # Compile to binary.
    cmd = [clang]
    cmd.extend(build_flags)
    cmd.append("-g")
    cmd.append("-fno-discard-value-names")
    cmd.append(os.path.join(artifacts_dir, "bytecode.bc"))
    cmd.extend(deps)
    cmd.extend(["-o", os.path.join(artifacts_dir, "run")])

    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0
    pass


def build_stable(src, artifacts_dir):
    cmd = [clang]
    cmd.extend(build_flags)
    cmd.append("-fno-discard-value-names")
    cmd.append(f"-fpass-plugin={llvm_plugin_path}")
    cmd.append(src.name)
    cmd.extend(deps)
    cmd.extend(["-o", os.path.join(artifacts_dir, "run")])

    rc, _ = run_command_and_get_output(cmd, cwd=file_dir)
    assert rc == 0


@cmd.command()
@click.option("-s", "--src", required=True, help="source path",
              type=click.File("r"))
@click.option("-g", "--debug", help="debug build", type=bool, is_flag=True)
@click.option("-u", "--unsafe", help="unsafe build", type=bool, is_flag=True)
@click.option("-a", "--artifacts_dir", help="dir for artifacts", type=str,
              default=None)
def build(src, debug, unsafe, artifacts_dir):
    artifacts_dir = artifacts_dir or artifacts_dir_default

    # Create artifacts dir.
    if not os.path.exists(artifacts_dir):
        os.mkdir(artifacts_dir)

    if unsafe:
        print("building unsafe...")
        build_unsafe(src, artifacts_dir)
    elif debug:
        print("building debug...")
        build_debug(src, artifacts_dir)
    else:
        print("building stable...")
        build_stable(src, artifacts_dir)


cli = click.CommandCollection(sources=[cmd])

if __name__ == "__main__":
    cli()
