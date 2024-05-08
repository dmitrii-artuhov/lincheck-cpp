import os
import subprocess


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
    print(out)
    out = out.decode('utf-8')

    # This print is here to make running tests with -s flag more verbose
    print(out)

    return process.returncode, out


me_dir = os.path.dirname(__file__)
verifying_dir = os.path.join(me_dir, "..", "..", "verifying")


def build(dst_dir, path):
    cmd = ["./verify.py", "build", "--src", path, "-a", dst_dir]
    run_command_and_get_output(cmd, cwd=verifying_dir)
