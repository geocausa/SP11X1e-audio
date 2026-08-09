// @category SP11
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.listing.Function;
public class RefsToAddresses extends GhidraScript {
 public void run() throws Exception {
  String[] a=getScriptArgs();
  for(String s:a){Address dst=toAddr(Long.decode(s));println("TARGET "+s+" "+dst);
   ReferenceIterator it=currentProgram.getReferenceManager().getReferencesTo(dst);
   while(it.hasNext()){Reference r=it.next();Function f=currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());println("  "+r.getFromAddress()+" type="+r.getReferenceType()+" fn="+(f==null?"<none>":f.getName()+"@"+f.getEntryPoint()));}
  }
 }
}
