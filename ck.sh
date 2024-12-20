pushd ./source/benchmarks/graph_api_benchmark/kernels
clang -c -target spir64 -O0 -emit-llvm -o graph_api_benchmark_kernel_assign.bc graph_api_benchmark_kernel_assign.cl
clang -c -target spir64 -O0 -emit-llvm -o graph_api_benchmark_kernel_sin.bc graph_api_benchmark_kernel_sin.cl
llvm-spirv graph_api_benchmark_kernel_assign.bc -o graph_api_benchmark_kernel_assign.spv
llvm-spirv graph_api_benchmark_kernel_sin.bc -o graph_api_benchmark_kernel_sin.spv
rm *.bc
ls -la
popd

