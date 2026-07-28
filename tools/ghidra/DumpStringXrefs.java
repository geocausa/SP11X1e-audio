// SPDX-License-Identifier: GPL-2.0
// @category SP11

import java.io.File;
import java.io.PrintWriter;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.regex.Pattern;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class DumpStringXrefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) {
            throw new IllegalArgumentException("usage: <output-file> <regex>");
        }

        Pattern wanted = Pattern.compile(args[1], Pattern.CASE_INSENSITIVE);
        FunctionManager functions = currentProgram.getFunctionManager();
        Map<Function, StringBuilder> matches = new LinkedHashMap<>();
        DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);

        while (dataIterator.hasNext() && !monitor.isCancelled()) {
            Data data = dataIterator.next();
            Object value = data.getValue();
            if (!(value instanceof String)) {
                continue;
            }
            String text = (String)value;
            if (!wanted.matcher(text).find()) {
                continue;
            }

            ReferenceIterator references =
                currentProgram.getReferenceManager().getReferencesTo(data.getAddress());
            while (references.hasNext()) {
                Reference reference = references.next();
                Function function = functions.getFunctionContaining(reference.getFromAddress());
                if (function == null) {
                    continue;
                }
                matches.computeIfAbsent(function, unused -> new StringBuilder())
                    .append("string @").append(data.getAddress())
                    .append(" = ").append(text)
                    .append("; xref @").append(reference.getFromAddress()).append("\n");
            }
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter writer = new PrintWriter(new File(args[0]))) {
            writer.println("program=" + currentProgram.getName());
            writer.println("sha256=" + currentProgram.getExecutableSHA256());
            writer.println("regex=" + args[1]);
            for (Map.Entry<Function, StringBuilder> entry : matches.entrySet()) {
                Function function = entry.getKey();
                writer.println("\n//// " + function.getName() + " @"
                    + function.getEntryPoint() + " ////");
                writer.print(entry.getValue());
                DecompileResults result =
                    decompiler.decompileFunction(function, 120, monitor);
                if (result.decompileCompleted()) {
                    writer.println(result.getDecompiledFunction().getC());
                } else {
                    writer.println("// decompile failed: " + result.getErrorMessage());
                }
            }
        }
        println("wrote " + args[0] + " with " + matches.size() + " functions");
    }
}
