[How to] How to build & run distributed-chrome

1. Repository

https://github.sec.samsung.net/RS7-HighPerformanceWeb/distributed_chrome


2. Build

https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md


3. Run

Device A: Browser Process
$ ./chrome <URL>

Device B: Renderer Process
$ ./chrome --type=renderer --server-address=<IP ADDR>
