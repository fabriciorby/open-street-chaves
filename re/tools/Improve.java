// Cleans up Delphi-specific noise before export:
//  * defines Delphi long-string literals (refcount -1 header) found in CODE
//  * marks the DATA pointer table (pointers to unit globals) as constant so
//    the decompiler resolves *PTR_xxx to the global itself
//  * applies global names from globals.txt ("<hexaddr> <name> [type]")
//@category StreetChaves

import ghidra.app.script.GhidraScript;
import ghidra.docking.settings.SettingsDefinition;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;

import java.nio.file.*;
import java.util.*;

public class Improve extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        Memory mem = currentProgram.getMemory();
        Listing listing = currentProgram.getListing();
        SymbolTable st = currentProgram.getSymbolTable();
        MemoryBlock code = mem.getBlock("CODE");
        MemoryBlock data = mem.getBlock("DATA");
        MemoryBlock bss = mem.getBlock("BSS");

        // 1. Delphi string literals: [-1][len][chars][0], 4-byte aligned header.
        int nstr = 0;
        for (Address a = code.getStart().add(8); a.compareTo(code.getEnd().subtract(8)) < 0; a = a.add(4)) {
            int rc = mem.getInt(a.subtract(8));
            if (rc != -1) continue;
            int len = mem.getInt(a.subtract(4));
            if (len <= 0 || len > 512) continue;
            byte[] b = new byte[len + 1];
            if (mem.getBytes(a, b) != b.length || b[len] != 0) continue;
            boolean ok = true;
            for (int i = 0; i < len; i++) {
                int c = b[i] & 0xff;
                if (c < 0x20 && c != 9 && c != 10 && c != 13) { ok = false; break; }
            }
            if (!ok) continue;
            try {
                listing.clearCodeUnits(a, a.add(len), false);
                listing.createData(a, new StringDataType(), len + 1);
                nstr++;
            } catch (Exception e) { }
        }
        println("strings defined: " + nstr);

        // 2. Pointer table entries in DATA that point into DATA/BSS: make constant.
        int nptr = 0;
        for (Address a = data.getStart(); a.compareTo(data.getEnd().subtract(4)) <= 0; a = a.add(4)) {
            long v = mem.getInt(a) & 0xffffffffL;
            Address t = toAddr(v);
            if (!(bss.contains(t) || data.contains(t))) continue;
            if (currentProgram.getReferenceManager().getReferenceCountTo(a) == 0) continue;
            try {
                listing.clearCodeUnits(a, a.add(3), false);
                Data d = listing.createData(a, new PointerDataType());
                MutabilitySettingsDefinition.DEF.setChoice(d, MutabilitySettingsDefinition.CONSTANT);
                nptr++;
            } catch (Exception e) { }
        }
        println("constant pointers: " + nptr);

        // 3. Global names (and optional types).
        if (args.length > 0 && Files.exists(Paths.get(args[0]))) {
            DataTypeManager dtm = currentProgram.getDataTypeManager();
            for (String line : Files.readAllLines(Paths.get(args[0]))) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                String[] p = line.split("\\s+");
                Address a = toAddr(Long.parseLong(p[0], 16));
                for (Symbol s : st.getSymbols(a)) {
                    if (s.getSource() == SourceType.USER_DEFINED) s.delete();
                }
                st.createLabel(a, p[1], SourceType.USER_DEFINED).setPrimary();
                if (p.length > 2) {
                    DataType dt = switch (p[2]) {
                        case "u8" -> ByteDataType.dataType;
                        case "i8" -> SignedByteDataType.dataType;
                        case "bool" -> BooleanDataType.dataType;
                        case "u16" -> WordDataType.dataType;
                        case "i16" -> ShortDataType.dataType;
                        case "i32" -> IntegerDataType.dataType;
                        case "u32" -> UnsignedIntegerDataType.dataType;
                        case "f64" -> DoubleDataType.dataType;
                        case "str" -> new PointerDataType(CharDataType.dataType);
                        default -> null;
                    };
                    if (dt != null) {
                        try {
                            listing.clearCodeUnits(a, a.add(dt.getLength() - 1), false);
                            listing.createData(a, dt);
                        } catch (Exception e) { println("type fail " + line + ": " + e); }
                    }
                }
            }
        }
    }
}
