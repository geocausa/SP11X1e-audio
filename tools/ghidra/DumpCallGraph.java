// SPDX-License-Identifier: GPL-2.0
// @category SP11

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayDeque;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

public class DumpCallGraph extends GhidraScript {
    private static final class WorkItem {
        final Function function;
        final int depth;

        WorkItem(Function function, int depth) {
            this.function = function;
            this.depth = depth;
        }
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) {
            throw new IllegalArgumentException(
                "usage: <output-file> <depth> <address> [address ...]");
        }

        int maxDepth = Integer.decode(args[1]);
        FunctionManager functions = currentProgram.getFunctionManager();
        ArrayDeque<WorkItem> queue = new ArrayDeque<>();
        Map<Function, Integer> discovered = new LinkedHashMap<>();

        for (int index = 2; index < args.length; index++) {
            Address address = toAddr(Long.decode(args[index]));
            Function function = functions.getFunctionContaining(address);
            if (function == null) {
                println("no function contains " + address);
                continue;
            }
            if (!discovered.containsKey(function)) {
                discovered.put(function, 0);
                queue.add(new WorkItem(function, 0));
            }
        }

        while (!queue.isEmpty() && !monitor.isCancelled()) {
            WorkItem item = queue.removeFirst();
            if (item.depth >= maxDepth) {
                continue;
            }
            Set<Function> neighbours = new LinkedHashSet<>();
            neighbours.addAll(item.function.getCallingFunctions(monitor));
            neighbours.addAll(item.function.getCalledFunctions(monitor));
            for (Function neighbour : neighbours) {
                if (neighbour.isExternal() || discovered.containsKey(neighbour)) {
                    continue;
                }
                int depth = item.depth + 1;
                discovered.put(neighbour, depth);
                queue.addLast(new WorkItem(neighbour, depth));
            }
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter writer = new PrintWriter(new File(args[0]))) {
            writer.println("program=" + currentProgram.getName());
            writer.println("sha256=" + currentProgram.getExecutableSHA256());
            writer.println("max_depth=" + maxDepth);
            writer.println("function_count=" + discovered.size());

            for (Map.Entry<Function, Integer> entry : discovered.entrySet()) {
                Function function = entry.getKey();
                writer.println("\n//// " + function.getName() + " @"
                    + function.getEntryPoint() + " depth=" + entry.getValue()
                    + " ////");
                writer.print("callers:");
                for (Function caller : function.getCallingFunctions(monitor)) {
                    if (!caller.isExternal()) {
                        writer.print(" " + caller.getName() + "@"
                            + caller.getEntryPoint());
                    }
                }
                writer.println();
                writer.print("callees:");
                for (Function callee : function.getCalledFunctions(monitor)) {
                    if (!callee.isExternal()) {
                        writer.print(" " + callee.getName() + "@"
                            + callee.getEntryPoint());
                    }
                }
                writer.println();

                DecompileResults result =
                    decompiler.decompileFunction(function, 120, monitor);
                if (result.decompileCompleted()) {
                    writer.println(result.getDecompiledFunction().getC());
                } else {
                    writer.println("// decompile failed: "
                        + result.getErrorMessage());
                }
            }
        }
        println("wrote " + args[0] + " with " + discovered.size()
            + " functions");
    }
}
