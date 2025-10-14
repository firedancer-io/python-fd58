.PHONY: build dist redist install install-from-source clean uninstall check-python-headers

# Allow overriding the Python interpreter from the command line:
#   make PYTHON=python3.11 build
PYTHON ?= $(shell command -v python3 || command -v python)
PIP    ?= $(PYTHON) -m pip

# Detect Python version (e.g. 3.11) and pkg-config name
PYTHON_VERSION := $(shell $(PYTHON) -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
PKG_CONFIG_NAME := python-$(PYTHON_VERSION)

# Compiler/linker flags from pkg-config (requires pythonX.Y-dev package)
PY_CFLAGS  := $(shell pkg-config --cflags $(PKG_CONFIG_NAME) 2>/dev/null)
PY_LDFLAGS := $(shell pkg-config --libs   $(PKG_CONFIG_NAME) 2>/dev/null)

# Check if Python.h exists, else error out
check-python-headers:
	@if ! pkg-config --exists python-$(PYTHON_VERSION); then \
		echo "Error: pkg-config entry for python-$(PYTHON_VERSION) not found. Install python$(PYTHON_VERSION)-dev."; \
		exit 1; \
	fi
	@if [ ! -f "$$(pkg-config --variable=includedir python-$(PYTHON_VERSION))/python$(PYTHON_VERSION)/Python.h" ] && \
		[ ! -f "/usr/include/python$(PYTHON_VERSION)/Python.h" ]; then \
		echo "Error: Python.h not found in expected include dir."; \
		exit 1; \
	fi

build: check-python-headers
	CFLAGS="$(PY_CFLAGS) $(CFLAGS)" LDFLAGS="$(PY_LDFLAGS) $(LDFLAGS)" CYTHONIZE=1 $(PYTHON) setup.py build

dist: check-python-headers
	CFLAGS="$(PY_CFLAGS) $(CFLAGS)" LDFLAGS="$(PY_LDFLAGS) $(LDFLAGS)" CYTHONIZE=1 $(PYTHON) setup.py sdist bdist_wheel

redist: clean dist

install: check-python-headers
	CFLAGS="$(PY_CFLAGS) $(CFLAGS)" LDFLAGS="$(PY_LDFLAGS) $(LDFLAGS)" CYTHONIZE=1 $(PIP) install .

install-from-source: dist
	$(PIP) install dist/fd58-0.1.0.tar.gz

clean:
	$(RM) -r build dist src/*.egg-info
	$(RM) -r src/fd58/fd58.c
	$(RM) -r .pytest_cache
	find . -name '__pycache__' -type d -exec rm -r {} +

uninstall:
	$(PIP) uninstall -y fd58
