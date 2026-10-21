#!/bin/zsh
# Re-run cleanup + full decompile export on the saved Ghidra project.
R=${0:A:h:h}
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
export PATH=$JAVA_HOME/bin:$PATH
cat $R/tools/names.txt $R/tools/rtl_names.txt > $R/tools/all_names.txt
/opt/homebrew/opt/ghidra/libexec/support/analyzeHeadless $R/ghidra/proj chaves -process Chaves.exe -noanalysis \
  -scriptPath $R/tools -postScript Improve.java $R/tools/globals.txt \
  -postScript ExportAll.java $R/tools/all_names.txt $R/out/decomp 2>&1 | grep -E 'ERROR|Exception|Improve.java>|ExportAll.java>'
awk '/^\/\/ ===== /{p=($4 ~ /^TForm1_/)} p' $R/out/decomp/all.c > $R/out/decomp/tform1.c
