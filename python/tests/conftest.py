#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""The Python module's pytest suite (python/tests/tests.cmake runs it).

It imports the package the build staged (<tree>/python/gweni/, on PYTHONPATH
as GWENI_PYTHON_STAGE), never python/gweni/ of the source tree, which has no
extension. Tests that make a Headless or an App are marked gpu: CTest runs
them as python_pytest_gpu, under the render lock, and the rest as
python_pytest.
"""

import os
import pathlib

import pytest


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line("markers", "gpu: makes a GL context (a Headless or an App)")


@pytest.fixture(scope="session", autouse=True)
def _staged_package() -> None:
    import gweni

    stage = os.environ.get("GWENI_PYTHON_STAGE")
    assert stage, "GWENI_PYTHON_STAGE is not set: run the suite through ctest"
    assert pathlib.Path(gweni.__file__).resolve().parent == (pathlib.Path(stage) / "gweni").resolve()


@pytest.fixture
def headless():
    import gweni

    with gweni.Headless() as h:
        yield h
