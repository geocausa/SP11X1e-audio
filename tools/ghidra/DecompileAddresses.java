// SPDX-License-Identifier: GPL-2.0
// @category SP11

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

public class DecompileAddresses extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException(
                "usage: <output-file> <address> [address ...]");
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        FunctionManager functions = currentProgram.getFunctionManager();
        StringBuilder output = new StringBuilder();

        for (int index = 1; index < args.length; index++) {
            Address address = toAddr(Long.decode(args[index]));
            Function function = functions.getFunctionContaining(address);
            output.append("\n//// ").append(args[index]).append(" -> ");
            if (function == null) {
                output.append("<no function> ////\n");
                continue;
            }
            output.append(function.getName()).append(" @")
                .append(function.getEntryPoint()).append(" ////\n");
            DecompileResults result =
                decompiler.decompileFunction(function, 120, monitor);
            if (result.decompileCompleted()) {
                output.append(result.getDecompiledFunction().getC());
            } else {
                output.append("// decompile failed: ")
                    .append(result.getErrorMessage()).append("\n");
            }
        }

        File destination = new File(args[0]);
        try (PrintWriter writer = new PrintWriter(destination)) {
            writer.print(output.toString());
        }
        println("wrote " + destination);
    }
}
