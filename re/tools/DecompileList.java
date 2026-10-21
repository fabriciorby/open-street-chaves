// Decompile a list of addresses (creating functions where Ghidra has none).
// Script args: <outdir> <hexaddr>...
//@category StreetChaves

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.nio.file.*;

public class DecompileList extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Path out = Paths.get(args[0]);
        Files.createDirectories(out);
        DecompInterface di = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        opts.setMaxPayloadMBytes(1024);
        opts.setMaxInstructions(5000000);
        di.setOptions(opts);
        di.openProgram(currentProgram);
        for (int i = 1; i < args.length; i++) {
            Address a = toAddr(Long.parseLong(args[i].replace("0x", ""), 16));
            Function f = getFunctionAt(a);
            if (f == null) {
                disassemble(a);
                f = createFunction(a, "sub_" + args[i].replace("0x", ""));
            }
            if (f == null) { println("no function at " + a); continue; }
            try { f.setCallingConvention("__register"); } catch (Exception e) { }
            DecompileResults r = di.decompileFunction(f, 600, monitor);
            String c = r.decompileCompleted() ? r.getDecompiledFunction().getC()
                    : "/* decompile failed: " + r.getErrorMessage() + " */\n";
            Files.writeString(out.resolve(a + "_" + f.getName() + ".c"), "// ===== " + a + " " + f.getName() + "\n" + c);
            println("wrote " + a + " " + f.getName());
        }
    }
}
