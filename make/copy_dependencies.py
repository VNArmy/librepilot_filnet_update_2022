#!/usr/bin/env python3

 ###############################################################################
 #
 # The LibrePilot Project, http://www.librepilot.org Copyright (C) 2017.
 # Script to find and copy dependencies (msys2 only).
 # ./copy_dependencies.py -h for usage.
 #
 ###############################################################################

import argparse
import fnmatch
import os
import re
import shutil
import subprocess
import sys

# MSYS2 environments whose DLLs get bundled (see MINGW_DIR in make/tools.mk)
MSYS2_ENVS = "mingw32|mingw64|ucrt64|clang32|clang64|clangarm64"

def run(cmd):
	return subprocess.check_output(cmd, universal_newlines=True)

def cygpath(file):
	return run(["cygpath", "-m", file]).strip()

# ldd does not work well on Windows 10 and is not supported on mingw32
def ldd(files):
	ldd_output = run(["ntldd", "-R"] + files)
	# sanitize output
	ldd_output = ldd_output.strip().replace('\\', '/')
	# parse lines that match this format : <file name> => <file path> (<memory address>)
	file_pattern = re.escape(cygpath("/")) + "(?:" + MSYS2_ENVS + ")/bin/.*"
	print(file_pattern)
	regex = re.compile(r"(.*) => (" + file_pattern + r") \((.*)\)", re.IGNORECASE)
	matches = [regex.match(line.strip()) for line in ldd_output.splitlines()]
	return {m.group(2) for m in matches if m}

# excludes are file names or wildcard patterns (e.g. libLLVM-*.dll), case insensitive
def is_excluded(file, excludes):
	name = os.path.basename(file).lower()
	return any(fnmatch.fnmatchcase(name, pattern.lower()) for pattern in excludes or [])

parser = argparse.ArgumentParser(description='Find and copy dependencies to a destination directory.')
parser.add_argument('-n', '--no-copy', action='store_true', help='don\'t copy dependencies to destination dir')
parser.add_argument('-v', '--verbose', action='store_true', help='enable verbose mode')
parser.add_argument('-d', '--dest', type=str, default='.', help='destination directory (defaults to current directory)')
parser.add_argument('-f', '--files', metavar='FILE', nargs='+', required=True, help='a file')
parser.add_argument('-e', '--excludes', metavar='FILE', nargs='+', help='a file name or wildcard pattern to ignore')

args = parser.parse_args()

# check that args.dest exists and is a directory
if not os.path.isdir(args.dest):
	print("Error: destination " + str(args.dest) + " is not a directory")
	sys.exit(1)

# find dependencies
dependencies = ldd(args.files)
print("Found " + str(len(dependencies)) + " new dependencies")

# no copy, exit now
if args.no_copy:
	sys.exit(0)

# copy dependencies to destination dir
copy_count = 0
for file in sorted(dependencies):
	dest_file = os.path.join(args.dest, os.path.basename(file))
	if is_excluded(file, args.excludes):
		print("Ignoring " + file)
		continue
	if not os.path.isfile(dest_file):
		print("Copying " + file + " to " + args.dest)
		shutil.copy2(file, args.dest)
		copy_count += 1
print("Copied " + str(copy_count) + " dependencies to " + str(args.dest))

sys.exit(0)
