// SPDX-License-Identifier: GPL-2.0
// @category SP11
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;
public class FindCallers extends GhidraScript {
  public void run() throws Exception {
    FunctionManager fm=currentProgram.getFunctionManager(); ReferenceManager rm=currentProgram.getReferenceManager();
    for(String s:getScriptArgs()){
      Address a=toAddr(s); Function f=fm.getFunctionContaining(a);
      Address target=(f!=null)?f.getEntryPoint():a;
      println("TARGET "+a+" containing="+(f==null?"<none>":f.getName()+"@"+f.getEntryPoint())+" refs-to-entry="+target);
      ReferenceIterator it=rm.getReferencesTo(target);
      while(it.hasNext()){
        Reference r=it.next(); Address from=r.getFromAddress(); Function cf=fm.getFunctionContaining(from);
        if(r.getReferenceType().isCall() || r.getReferenceType().isJump())
          println(" CALLER "+from+" type="+r.getReferenceType()+" fn="+(cf==null?"<none>":cf.getName()+"@"+cf.getEntryPoint()));
      }
    }
  }
}
