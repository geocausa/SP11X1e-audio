// SPDX-License-Identifier: GPL-2.0
// @category SP11
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.*;
import java.nio.charset.StandardCharsets;

public class FindStringXrefs extends GhidraScript {
  public void run() throws Exception {
    String needle = getScriptArgs().length > 0 ? getScriptArgs()[0] : "graphic-equalizer";
    Listing l=currentProgram.getListing(); ReferenceManager rm=currentProgram.getReferenceManager();
    FunctionManager fm=currentProgram.getFunctionManager();
    println("PROGRAM "+currentProgram.getName()+" needle="+needle);
    for(Data d:l.getDefinedData(true)) {
      Object v=d.getValue(); if(v==null) continue; String s=v.toString();
      if(!s.toLowerCase().contains(needle.toLowerCase())) continue;
      Address a=d.getAddress(); println("STRING "+a+" "+s.replace('\n',' '));
      ReferenceIterator it=rm.getReferencesTo(a);
      while(it.hasNext()) {
        Reference r=it.next(); Address f=r.getFromAddress(); Function fn=fm.getFunctionContaining(f);
        println(" REF "+f+" type="+r.getReferenceType()+" fn="+(fn==null?"<none>":fn.getName()+"@"+fn.getEntryPoint()));
      }
    }
  }
}
