// Headless export for Chaves.exe: applies names, sets Delphi __register on
// every function, then writes decompiled C for every function plus string
// and global cross-reference listings.
//
// Script args: <data/names.txt> <outdir>
//   data/names.txt lines: "<hexaddr> <name>"
//@category StreetChaves

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class ExportAll extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Path names = Paths.get(args[0]);
        Path out = Paths.get(args[1]);
        Files.createDirectories(out.resolve("funcs"));

        FunctionManager fm = currentProgram.getFunctionManager();
        SymbolTable st = currentProgram.getSymbolTable();

        for (String line : Files.readAllLines(names)) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] p = line.split("\\s+", 2);
            Address a = toAddr(Long.parseLong(p[0], 16));
            Function f = fm.getFunctionAt(a);
            if (f == null) {
                disassemble(a);
                f = createFunction(a, p[1]);
            }
            if (f != null) f.setName(p[1], SourceType.USER_DEFINED);
            else st.createLabel(a, p[1], SourceType.USER_DEFINED);
        }

        for (Function f : fm.getFunctions(true)) {
            if (!f.isExternal() && !f.isThunk()) {
                try { f.setCallingConvention("__register"); } catch (Exception e) { }
            }
        }

        DecompInterface di = new DecompInterface();
        ghidra.app.decompiler.DecompileOptions opts = new ghidra.app.decompiler.DecompileOptions();
        opts.setMaxPayloadMBytes(1024);   // DXTimer1Timer alone exceeds the 50 MB default
        opts.setMaxInstructions(5000000);
        di.setOptions(opts);
        di.openProgram(currentProgram);
        try (PrintWriter all = new PrintWriter(Files.newBufferedWriter(out.resolve("all.c")));
             PrintWriter idx = new PrintWriter(Files.newBufferedWriter(out.resolve("functions.tsv")))) {
            idx.println("addr\tname\tsize\tcallers\tcallees");
            for (Function f : fm.getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isExternal()) continue;
                DecompileResults r = di.decompileFunction(f, f.getBody().getNumAddresses() > 20000 ? 3000 : 120, monitor);
                String c = r.decompileCompleted() ? r.getDecompiledFunction().getC()
                        : "/* decompile failed: " + r.getErrorMessage() + " */\n";
                String hdr = "// ===== " + f.getEntryPoint() + " " + f.getName() + "\n";
                all.print(hdr);
                all.print(c);
                if (!f.getName().startsWith("FUN_")) {
                    Files.writeString(out.resolve("funcs").resolve(f.getEntryPoint() + "_" + f.getName() + ".c"), hdr + c);
                }
                StringBuilder callers = new StringBuilder();
                for (Function g : f.getCallingFunctions(monitor)) callers.append(g.getName()).append(',');
                StringBuilder callees = new StringBuilder();
                for (Function g : f.getCalledFunctions(monitor)) callees.append(g.getName()).append(',');
                idx.println(f.getEntryPoint() + "\t" + f.getName() + "\t" + f.getBody().getNumAddresses()
                        + "\t" + callers + "\t" + callees);
            }
        }

        // Strings with referencing functions.
        ReferenceManager rm = currentProgram.getReferenceManager();
        try (PrintWriter sw = new PrintWriter(Files.newBufferedWriter(out.resolve("strings.tsv")))) {
            sw.println("addr\tvalue\treferenced_by");
            DataIterator it = currentProgram.getListing().getDefinedData(true);
            while (it.hasNext()) {
                Data d = it.next();
                StringDataInstance s = StringDataInstance.getStringDataInstance(d);
                if (s == StringDataInstance.NULL_INSTANCE) continue;
                String v = s.getStringValue();
                if (v == null) continue;
                Set<String> refs = new TreeSet<>();
                for (Reference ref : rm.getReferencesTo(d.getAddress())) {
                    Function f = fm.getFunctionContaining(ref.getFromAddress());
                    refs.add(f == null ? ref.getFromAddress().toString() : f.getName());
                }
                sw.println(d.getAddress() + "\t" + v.replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r") + "\t" + String.join(",", refs));
            }
        }

        // Globals in writable data: who reads/writes them.
        try (PrintWriter gw = new PrintWriter(Files.newBufferedWriter(out.resolve("globals.tsv")))) {
            gw.println("addr\treaders\twriters");
            for (var block : currentProgram.getMemory().getBlocks()) {
                if (!block.isWrite() || block.isExecute()) continue;
                AddressIterator ai = rm.getReferenceDestinationIterator(block.getStart(), true);
                while (ai.hasNext()) {
                    Address a = ai.next();
                    if (!block.contains(a)) break;
                    Set<String> rd = new TreeSet<>(), wr = new TreeSet<>();
                    for (Reference ref : rm.getReferencesTo(a)) {
                        Function f = fm.getFunctionContaining(ref.getFromAddress());
                        String n = f == null ? ref.getFromAddress().toString() : f.getName();
                        if (ref.getReferenceType().isWrite()) wr.add(n); else rd.add(n);
                    }
                    gw.println(a + "\t" + String.join(",", rd) + "\t" + String.join(",", wr));
                }
            }
        }
        println("ExportAll done");
    }
}
