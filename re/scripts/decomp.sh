#!/bin/bash
# Decompiles the functions listed in addrs.txt (lines: "0xADDR [name]") into out.c with headless Ghidra.
# Usage (from re/): scripts/decomp.sh addrs.txt out.c
# Needs the un-analyzed Ghidra project ghidra_quick/Quick (see re/README.md).
RE=$(cd "$(dirname "$0")/.." && pwd)
GH=${GHIDRA:-~/.local/opt/ghidra_12.1.4_PUBLIC}
JDK=${JAVA_HOME:-~/.local/opt/jdk-21.0.12.1+1}
export JAVA_HOME=$JDK PATH=$JDK/bin:$PATH
export JAVA_TOOL_OPTIONS="-Xlog:disable -Xlog:all=error:stderr"
export DECOMP_IN=$(realpath "$1") DECOMP_OUT=$(realpath -m "$2") GHIDRA_HEADLESS_MAXMEM=6G
"$GH/support/analyzeHeadless" "$RE/ghidra_quick" Quick -process CrabChampions-Win64-Shipping.exe -noanalysis \
  -scriptPath "$RE/scripts" -postScript DecompileAt.java 2>&1 | grep -E "ERROR|Exception" | head
