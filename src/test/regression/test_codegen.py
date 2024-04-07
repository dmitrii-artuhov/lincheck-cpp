
import os

import pytest
import yaml
from utils import run_command_and_get_output

CLANG = "clang++"
OPT = "opt"
LLVM_DIS = "llvm-dis"

DIR = os.path.dirname(__file__)
TESTDATA_DIR = os.path.join(DIR, "testdata")
SRC_DIR = os.path.join(DIR, "..", "..")
LIB = os.path.join(SRC_DIR, "runtime", "lib.cpp")

COROGEN_PATH = os.path.join(SRC_DIR, "build", "codegen", "CoroGenPass.so")
assert COROGEN_PATH, "To tun tests build coro gen pass firstly"


def build(path, tmpdir, flag=None):
    cmd = [CLANG, "-O3", "-std=c++2a",
           "-fno-discard-value-names",
           f"-fpass-plugin={COROGEN_PATH}", path, LIB,
           os.path.join(DIR, "codegen_runner.cpp"), "-o", "run"]
    if flag:
        cmd.append(f"-D{flag}")
    rc, _ = run_command_and_get_output(cmd, cwd=tmpdir)
    assert rc == 0


def get_suspends_test_filenames():
    return [f.replace(".ll", "") for f in os.listdir(TESTDATA_DIR)
            if f.endswith(".ll")]


@pytest.mark.parametrize('name', get_suspends_test_filenames())
def test_codegen_suspends(name, tmpdir):
    path = os.path.join(TESTDATA_DIR, name)
    path_ll = f"{path}.ll"
    build(path_ll, tmpdir)

    # Execute runner and compare result with expected.
    with open(os.path.join(TESTDATA_DIR, f"{name}.yml")) as f:
        expected = yaml.safe_load(f.read())
    rc, output = run_command_and_get_output(["./run"], cwd=tmpdir)
    assert rc == 0
    assert output.rstrip("\n") == "\n".join([str(i) for i in expected])


def test_codegen_queue(tmpdir):
    path = os.path.join(TESTDATA_DIR, "queue.cpp")
    build(path, tmpdir, "no_trace")

    rc, output = run_command_and_get_output(["./run"], cwd=tmpdir)
    assert rc == 0
    assert output == """\
Push: 1
Push: 2
Push: 3
Push: 4
Push: 5
Got: 1
Got: 2
Got: 3
Got: 4
Got: 5
"""


def test_codegen_flow(tmpdir):
    path = os.path.join(TESTDATA_DIR, "flow.cpp")
    build(path, tmpdir, "no_trace")
    rc, output = run_command_and_get_output(["./run"], cwd=tmpdir)
    assert rc == 0
    assert output == """\
bar(5)
bar res: 12
bar(1)
bar res: 1
done
foo(2)
foo res: 1
bar(1)
bar res: 1
done
foo(4)
foo res: 43
bar(3)
bar res: 4
bar(1)
bar res: 1
done
foo(2)
foo res: 1
bar(1)
bar res: 1
done
"""
