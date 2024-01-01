import tempfile

import py
import pytest


# ######## #
# Fixtures #
# ######## #
def get_tmpdir(request):
    tmpdir = py.path.local(tempfile.mkdtemp())
    request.addfinalizer(lambda: tmpdir.remove(rec=1))
    return str(tmpdir)


@pytest.fixture(scope="function")
def tmpdir(request):
    return get_tmpdir(request)
