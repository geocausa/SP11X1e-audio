// SPDX-License-Identifier: GPL-2.0
// @category SP11
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
public class ListRefsToAddress extends GhidraScript {
  public void run() throws Exception {
    if(getScriptArgs().length<1) throw new IllegalArgumentException("address");
    Address a=currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(getScriptArgs()[0].replace("0x",""));
    println("TARGET "+a);
    ReferenceManager rm=currentProgram.getReferenceManager();
    ReferenceIterator it=rm.getReferencesTo(a);
    while(it.hasNext()){
      Reference r=it.next(); Address from=r.getFromAddress();
      Function f=currentProgram.getFunctionManager().getFunctionContaining(from);
      println("REF "+from+" type="+r.getReferenceType()+" fn="+(f==null?"<none>":f.getName()+"@"+f.getEntryPoint()));
    }
  }
}
