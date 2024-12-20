!/bin/bash

cmake --build build -- -j

if [ "$1" == "ng" ]; then
	export GR=ng
elif [ "$1" == "gr" ]; then
	export GR=gr
else
	export GR=all
fi
export PFX=l0  ; ./build/bin/graph_api_benchmark_$PFX --gtest_filter=SinKernelGraph*
export PFX=ur  ; ./build/bin/graph_api_benchmark_$PFX --gtest_filter=SinKernelGraph*
export PFX=sycl  ; ./build/bin/graph_api_benchmark_$PFX --gtest_filter=SinKernelGraph*
