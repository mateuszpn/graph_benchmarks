# memory_benchmark_embargo
Memory Benchmark is a set of tests aimed at measuring bandwidth of memory transfers.
| Test name | Description | Params | L0 | OCL |
|-----------|-------------|--------|----|-----|
SharedLocalMemoryEmbargo|Writes to SLM, then summing local memory and writes its value to a buffer. Uses different sizes of SLM to measure throughput.|<ul><li>--slmSize Size of the local memory per thread group.</li><li>--useEvents Perform GPU-side measurements using events (0 or 1)</li></ul>|:heavy_check_mark:|:heavy_check_mark:|
StreamMemoryEmbargo|Streams memory inside of kernel in a fashion described by 'type'. Copy means one memory location is read from and the second one is written to. Triad means two buffers are read and one is written to. In read and write memory is only read or written to.|<ul><li>--size Size of the memory to stream. Must be divisible by datatype size.</li><li>--type Memory streaming type (stream_3bytesRGBtoY or stream_3BytesAlignedRGBtoY)</li><li>--useEvents Perform GPU-side measurements using events (0 or 1)</li></ul>|:heavy_check_mark:|:heavy_check_mark:|
StreamMemoryImmediateEmbargo|Streams memory inside of kernel in a fashion described by 'type' using immediate command list. Copy means one memory location is read from and the second one is written to. Triad means two buffers are read and one is written to. In read and write memory is only read or written to.|<ul><li>--size Size of the memory to stream. Must be divisible by datatype size.</li><li>--type Memory streaming type (stream_3bytesRGBtoY or stream_3BytesAlignedRGBtoY)</li><li>--useEvents Perform GPU-side measurements using events (0 or 1)</li></ul>|:heavy_check_mark:|:x:|



# ulls_benchmark_embargo
Ulls Benchmark is a set of tests aimed at measuring Ultra Low Latency Submission (ULLS) performance impact.
| Test name | Description | Params | L0 | OCL |
|-----------|-------------|--------|----|-----|
EmptyKernelEmbargo|enqueues empty kernel and measures time to launch it and wait for it on CPU, thus measuring walker spawn time.|<ul><li>--wgc Workgroup count</li><li>--wgs Workgroup size (aka local work size)</li></ul>|:heavy_check_mark:|:heavy_check_mark:|



