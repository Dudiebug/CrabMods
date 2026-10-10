// Tune analyzers for a 73MB UE4 monolithic binary.
import ghidra.app.script.GhidraScript;
public class PreAnalysis extends GhidraScript {
  @Override public void run() throws Exception {
    setAnalysisOption(currentProgram, "Decompiler Parameter ID", "false");
    setAnalysisOption(currentProgram, "Non-Returning Functions - Discovered", "true");
    setAnalysisOption(currentProgram, "Windows x86 PE RTTI Analyzer", "true");
    setAnalysisOption(currentProgram, "Scalar Operand References", "true");
    println("PreAnalysis options set");
  }
}
