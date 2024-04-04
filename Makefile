.PHONY: build dist redist install install-from-source clean uninstall

PYTHON?=python
PIP?=$(PYTHON) -m pip

build:
	CYTHONIZE=1 $(PYTHON) setup.py build

dist:
	CYTHONIZE=1 $(PYTHON) setup.py sdist bdist_wheel

redist: clean dist

install:
	CYTHONIZE=1 $(PIP) install .

install-from-source: dist
	$(PIP) install dist/fd58-0.1.0.tar.gz

clean:
	$(RM) -r build dist src/*.egg-info
	$(RM) -r src/fd58/fd58.c
	$(RM) -r .pytest_cache
	find . -name __pycache__ -exec rm -r {} +
	#git clean -fdX

uninstall:
	$(PIP) uninstall fd58
