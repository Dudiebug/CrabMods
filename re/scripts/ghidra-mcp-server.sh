#!/bin/bash
# Launches the bethington/ghidra-mcp headless server (port 8089) for the analyzed project, e.g.
#   scripts/ghidra-mcp-server.sh --project "$PWD/ghidra/CrabChampions.gpr" --program CrabChampions-Win64-Shipping.exe
# scripts/gm.py talks to it.
GH=${GHIDRA:-~/.local/opt/ghidra_12.1.4_PUBLIC}
MCP=${GHIDRA_MCP:-~/.local/opt/ghidra-mcp}
JDK=${JAVA_HOME:-~/.local/opt/jdk-21.0.12.1+1}
CP=$MCP/build/libs/GhidraMCP-6.0.0.jar
for j in $GH/Ghidra/Framework/*/lib/*.jar $GH/Ghidra/Features/*/lib/*.jar $GH/Ghidra/Processors/*/lib/*.jar $MCP/lib/*.jar; do CP="$CP:$j"; done
export JAVA_TOOL_OPTIONS="-Xlog:disable -Xlog:all=error:stderr"
export GHIDRA_MCP_ALLOW_SCRIPTS=1
exec "$JDK/bin/java" -Xmx14g -XX:+UseG1GC -Dghidra.home=$GH -Dapplication.name=GhidraMCP \
  -classpath "$CP" com.xebyte.headless.GhidraMCPHeadlessServer --port 8089 --bind 127.0.0.1 "$@"
