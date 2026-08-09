"""Builds the C++ engine into a shared library that ships inside the package.

The Python API loads it with ctypes, so this is a plain shared library rather than a
CPython extension module — no PyInit, no ABI coupling, and the same artifact works for
any Python version.
"""
from __future__ import annotations

import os
import sys

from setuptools import setup
from setuptools.extension import Extension
from setuptools.command.build_ext import build_ext

HERE = os.path.dirname(os.path.abspath(__file__))

# setuptools requires source paths relative to setup.py, /-separated. setup.py lives at
# the repo root precisely so cpp/ is inside the build tree — pip builds in an isolated
# copy of the project directory, so anything above it would not exist at build time.
CPP = "cpp"

SOURCES = [f"{CPP}/src/{f}" for f in ("soul.cpp", "bonds.cpp", "world.cpp")]
INCLUDE = f"{CPP}/include"


class SharedLibExt(Extension):
    """Marker so build_ext knows to emit a plain shared library."""


class BuildSharedLib(build_ext):
    def get_export_symbols(self, ext):
        # A plain shared library has no PyInit_* — suppress setuptools adding one.
        return []

    def get_ext_filename(self, ext_name):
        base = ext_name.split(".")[-1]
        if sys.platform == "win32":
            return os.path.join(*ext_name.split(".")[:-1], base + ".dll")
        if sys.platform == "darwin":
            return os.path.join(*ext_name.split(".")[:-1], base + ".dylib")
        return os.path.join(*ext_name.split(".")[:-1], base + ".so")


ext = SharedLibExt(
    "samskriti.libsamskriti",
    sources=SOURCES,
    include_dirs=[INCLUDE],
    language="c++",
    extra_compile_args=(["/std:c++17", "/O2"] if sys.platform == "win32"
                        else ["-std=c++17", "-O2", "-fPIC"]),
)

setup(
    ext_modules=[ext],
    cmdclass={"build_ext": BuildSharedLib},
)
