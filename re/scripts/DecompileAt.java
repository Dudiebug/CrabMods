// Decompile functions at addresses listed (one hex per line) in $DECOMP_IN; writes C to $DECOMP_OUT.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.nio.file.*;
import java.util.*;

public class DecompileAt extends GhidraScript {
    @Override
    public void run() throws Exception {
        List<String> lines = Files.readAllLines(Paths.get(System.getenv("DECOMP_IN")));
        StringBuilder sb = new StringBuilder();
        DecompInterface di = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        di.setOptions(opts);
        di.openProgram(currentProgram);
        for (String l : lines) {
            l = l.trim();
            if (l.isEmpty()) continue;
            String[] parts = l.split("\\s+", 2);
            Address a = toAddr(Long.parseUnsignedLong(parts[0].replace("0x", ""), 16));
            Function f = getFunctionAt(a);
            if (f == null) {
                new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
                new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
                f = getFunctionAt(a);
            }
            if (parts.length > 1 && f != null) {
                try { f.setName(parts[1], ghidra.program.model.symbol.SourceType.USER_DEFINED); } catch (Exception e) {}
            }
            sb.append("// ===== ").append(l).append("\n");
            if (f == null) { sb.append("// no function\n"); continue; }
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            sb.append(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed: " + r.getErrorMessage()).append("\n");
        }
        Files.writeString(Paths.get(System.getenv("DECOMP_OUT")), sb.toString());
    }
}
