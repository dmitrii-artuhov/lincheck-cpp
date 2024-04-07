import os

from utils import build, run_command_and_get_output

me_dir = os.path.dirname(__file__)


def test_verifying_args(tmpdir):
    path = os.path.join(me_dir, "testdata", "args.cpp")
    build(tmpdir, path)
    rc, out = run_command_and_get_output(["./run"], cwd=tmpdir)
    assert rc == 0
    assert """\
fatty_sum(42, 28, 30, abacaba, 1.5, 36.6)
Returned: 144
No args!
Returned: 0
hello,  world !
Returned: 0
Returned: 10
Returned: 85
""" == out
