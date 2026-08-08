// SPDX-License-Identifier: GPL-2.0
// @category SP11
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
public class ListClassSymbols extends GhidraScript {
  public void run() throws Exception {
    String q=getScriptArgs().length>0?getScriptArgs()[0]:"CDolbyAPO";
    String lq=q.toLowerCase(); SymbolTable st=currentProgram.getSymbolTable();
    println("PROGRAM "+currentProgram.getName()+" query="+q);
    for(Symbol s: st.getAllSymbols(true)){
      String fn=s.getName(true);
      String ns=s.getParentNamespace()==null?"":s.getParentNamespace().getName(true);
      if(fn.toLowerCase().contains(lq)||ns.toLowerCase().contains(lq)){
        Function f=currentProgram.getFunctionManager().getFunctionAt(s.getAddress());
        Data d=currentProgram.getListing().getDataAt(s.getAddress());
        println(s.getAddress()+" type="+s.getSymbolType()+" name="+fn+" ns="+ns+
          (f==null?"":" function="+f.getName(true))+(d==null?"":" data="+d.getDataType().getName()+" value="+d.getValue()));
      }
    }
  }
}
