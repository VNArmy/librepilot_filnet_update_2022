# We use python to extract git version info and generate some other files.
# The top level Makefile (make/tools.mk) exports the interpreter it uses as
# PYTHON, so use the same one. The scripts run with Python 3.
PYTHON = $$(PYTHON)

isEmpty(PYTHON) {
    # not started from the top level Makefile (e.g. Qt Creator), search the path
    PYTHON_VER = "$$system(python3 --version 2>&1)"
    contains(PYTHON_VER, "Python 3.*") {
        PYTHON = python3
    } else {
        PYTHON = python
    }
}

PYTHON = $$replace(PYTHON, \\\\, /)
message(Using python interpreter: $$PYTHON)
