// SPDX-License-Identifier: GPL-2.0
// @category SP11
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
public class ListSymbolsMatching extends GhidraScript {
  public void run() throws Exception {
    String n=getScriptArgs().length>0?getScriptArgs()[0]:"AIDEModule";
    String q=n.toLowerCase(); SymbolTable st=currentProgram.getSymbolTable();
    for(Symbol s:st.getAllSymbols(true)){
      String x=s.getName(true); if(!x.toLowerCase().contains(q))continue;
      Function f=currentProgram.getFunctionManager().getFunctionAt(s.getAddress());
      println(s.getAddress()+" "+s.getSymbolType()+" "+x+(f==null?"":" fn="+f.getName()));
    }
    for(Function f:currentProgram.getFunctionManager().getFunctions(true)){
      String x=f.getName(true); if(x.toLowerCase().contains(q)) println(f.getEntryPoint()+" FUNCTION "+x);
    }
  }
}
