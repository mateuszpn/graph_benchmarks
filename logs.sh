!/bin/bash

cmake --build build -- -j

if [ "$1" == "ng" ]; then
	export GR=ng
elif [ "$1" == "gr" ]; then
	export GR=gr
else
	export GR=all
fi
export PFX=l0  ; echo $PFX ; ze_tracer -c -s ./build/bin/graph_api_benchmark_$PFX --gtest_filter=SinKernelGraph* &> ze_skg_${PFX}_${GR}.log 
export PFX=ur  ; echo $PFX ; ze_tracer -c -s ./build/bin/graph_api_benchmark_$PFX --gtest_filter=SinKernelGraph* &> ze_skg_${PFX}_${GR}.log
export PFX=sycl; echo $PFX ; ze_tracer -c -s ./build/bin/graph_api_benchmark_$PFX --gtest_filter=SinKernelGraph* &> ze_skg_${PFX}_${GR}.log
