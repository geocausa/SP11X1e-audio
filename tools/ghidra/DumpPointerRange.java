// SPDX-License-Identifier: GPL-2.0
// @category SP11

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;

public class DumpPointerRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException(
                "usage: <output-file> <start-address> <byte-length>");
        }

        Address start = toAddr(Long.decode(args[1]));
        long length = Long.decode(args[2]);
        try (PrintWriter writer = new PrintWriter(new File(args[0]))) {
            writer.println("program=" + currentProgram.getName());
            writer.println("range=" + start + "+0x" + Long.toHexString(length));
            for (long offset = 0; offset + 8 <= length; offset += 8) {
                Address slot = start.add(offset);
                long value = getLong(slot);
                Address target = toAddr(value);
                Function function = getFunctionAt(target);
                Symbol symbol = getSymbolAt(target);
                Data data = getDataAt(target);
                String description = "";
                if (function != null) {
                    description = "function " + function.getName();
                } else if (symbol != null) {
                    description = "symbol " + symbol.getName(true);
                } else if (data != null) {
                    description = "data " + data;
                }
                writer.printf("%s 0x%016x %s%n", slot, value, description);
            }
        }
        println("wrote " + args[0]);
    }
}
