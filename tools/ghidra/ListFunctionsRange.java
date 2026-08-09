// SPDX-License-Identifier: GPL-2.0
// @category SP11
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
public class ListFunctionsRange extends GhidraScript {
 public void run() throws Exception {
   if(getScriptArgs().length<2) throw new IllegalArgumentException("start end");
   AddressSpace s=currentProgram.getAddressFactory().getDefaultAddressSpace();
   Address a=s.getAddress(getScriptArgs()[0].replace("0x",""));
   Address b=s.getAddress(getScriptArgs()[1].replace("0x",""));
   FunctionIterator it=currentProgram.getFunctionManager().getFunctions(a,true);
   while(it.hasNext()){ Function f=it.next(); if(f.getEntryPoint().compareTo(b)>=0)break; println(f.getEntryPoint()+" "+f.getName()); }
 }
}
