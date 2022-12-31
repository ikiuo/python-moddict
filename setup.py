#!/usr/bin/env python3

import os
from distutils.core import setup, Extension

def getenv(name, defval=None):
    if name in os.environ:
        return os.environ[name]
    return defval

DEBUG = getenv('DEBUG') in ('true', 'yes')

MAJOR_VERSION = 0
MINOR_VERSION = 1
DEBUG_VERSION = 0
VERSION = '%d.%d.%d' % (MAJOR_VERSION, MINOR_VERSION, DEBUG_VERSION)

DEFINE_MACROS = [
    ('MAJOR_VERSION', MAJOR_VERSION),
    ('MINOR_VERSION', MINOR_VERSION),
    ('DEBUG_VERSION', DEBUG_VERSION),
]
UNDEF_MACROS = []

EXTRA_COMPILE_ARGS = [
    '-W',
    '-Wall',
    '-Wno-invalid-offsetof',
    '-Wno-deprecated-declarations',
]

if DEBUG:
    DEFINE_MACROS.append(('DEBUG', 1))
    UNDEF_MACROS.append('NDEBUG')
    EXTRA_COMPILE_ARGS.append('-O0')
    pass

setup(name='ModDict',
      version=VERSION,
      description='',
      ext_modules=[Extension(
          name='ModDict',
          define_macros=DEFINE_MACROS,
          undef_macros=UNDEF_MACROS,
          extra_compile_args=EXTRA_COMPILE_ARGS,
          sources=['ModDict.c'])])
