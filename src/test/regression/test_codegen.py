import os
import subprocess

import pytest
import yaml

CLANG = "clang++"
OPT = "opt"
LLVM_DIS = "llvm-dis"

DIR = os.path.dirname(__file__)
TESTDATA_DIR = os.path.join(DIR, "testdata")
SRC_DIR = os.path.join(DIR, "..", "..")
LIB = os.path.join(SRC_DIR, "runtime", "lib.cpp")

COROGEN_PATH = os.path.join(SRC_DIR, "build", "codegen", "CoroGenPass.so")
assert COROGEN_PATH, "To tun tests build coro gen pass firstly"


def get_test_names():
    return [f.replace(".ll", "") for f in os.listdir(TESTDATA_DIR)
            if f.endswith(".ll")]


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


@pytest.mark.parametrize('name', get_test_names())
def test_codegen(name, tmpdir):
    path = os.path.join(TESTDATA_DIR, name)

    # Compile ll file to llvm bytecode.
    path_ll = f"{path}.ll"
    cmd = [CLANG, "-emit-llvm", "-S", "-o", "bytecode.bc", path_ll]
    rc, _ = run_command_and_get_output(cmd, cwd=tmpdir)
    assert rc == 0

    # Run coro-gen pass.
    cmd = [OPT, "--load-pass-plugin", COROGEN_PATH,
           "-passes", "coro_gen",
           "bytecode.bc",
           "-o", "res.bc"]
    rc, _ = run_command_and_get_output(cmd, cwd=tmpdir)
    assert rc == 0

    # Print output for debug.
    cmd = [LLVM_DIS, "res.bc", "-o", "res.ll"]
    rc, _ = run_command_and_get_output(cmd, cwd=tmpdir)
    assert rc == 0
    with open(os.path.join(tmpdir, "res.ll")) as f:
        print(f.read())

    # Compile test_func.
    cmd = [CLANG, "res.bc", "-std=c++2a",
           LIB, os.path.join(DIR, "test_func.cpp"), "-o", "run"]
    rc, _ = run_command_and_get_output(cmd, cwd=tmpdir)
    assert rc == 0

    # Run test_func and compare result with expected.
    with open(os.path.join(TESTDATA_DIR, f"{name}.yml")) as f:
        expected = yaml.safe_load(f.read())
    rc, output = run_command_and_get_output(["./run"], cwd=tmpdir)
    assert rc == 0
    assert output.rstrip("\n") == "\n".join([str(i) for i in expected])
