
build/libloop-opt-pass.so: mb64566-loop-opt-pass.cpp
	make -C ./build

build/libloop-analysis-pass.so: mb64566-loop-analysis-pass.cpp
	make -C ./build


gen-ir: source.cpp
	clang++ -S -emit-llvm -g0 -O0 -Xclang -disable-O0-optnone source.cpp > source.ll
	opt -S -passes=mem2reg < source.ll > source.ll.tmp
	mv source.ll.tmp source.ll

run-analysis: gen-ir build/libloop-analysis-pass.so
	opt -load-pass-plugin build/libloop-analysis-pass.so -passes="mb64566-loop-analysis-pass" <source.ll >/dev/null 

run-opt: gen-ir build/libloop-opt-pass.so
	opt -load-pass-plugin build/libloop-opt-pass.so -passes="mb64566-loop-opt-pass" <source.ll >/dev/null