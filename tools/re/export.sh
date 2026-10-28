#!/bin/zsh
# Re-run cleanup + full decompile export on the saved Ghidra project.
T=${0:A:h}                 # tools/re (scripts + data/)
R=${0:A:h:h:h}/re         # local RE workspace (ghidra/, out/, game/), not tracked
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
export PATH=$JAVA_HOME/bin:$PATH
cat $T/data/names.txt $T/data/rtl_names.txt > $T/data/all_names.txt
/opt/homebrew/opt/ghidra/libexec/support/analyzeHeadless $R/ghidra/proj chaves -process Chaves.exe -noanalysis \
  -scriptPath $T -postScript Improve.java $T/data/globals.txt \
  -postScript ExportAll.java $T/data/all_names.txt $R/out/decomp 2>&1 | grep -E 'ERROR|Exception|Improve.java>|ExportAll.java>'
awk '/^\/\/ ===== /{p=($4 ~ /^TForm1_/)} p' $R/out/decomp/all.c > $R/out/decomp/tform1.c
