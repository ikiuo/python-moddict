# -*- Makefile -*-

DEBUG ?= false

ifndef DEBUG
DEBUG = true
endif

ifeq ($(DEBUG), true)
BUILD_OPT = --debug
endif

CXXARCH = -arch x86_64

PYTHON ?= python3

SETUP = $(PYTHON) -m setup

ENVPARAM = STDCXX="$(STDCXX)" ARCHFLAGS="$(CXXARCH)" DEBUG=$(DEBUG)

.PHONY: all build clean

all:
	@echo "Usage make (build|clean|install)"

build:
	env $(ENVPARAM) $(SETUP) build $(BUILD_OPT)

install:
	env $(ENVPARAM) $(SETUP) install --user --old-and-unmanageable

clean:
	$(SETUP) clean -a
	rm -rf ModDict.egg-info
	rm -rf build __pycache__
	rm -f *.pyc *~

# end:
