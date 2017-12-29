
# How to build & run distributed-chrome


### Install depot_tools


Clone the depot_tools repository:

```sh
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add depot_tools to the end of your PATH (you will probably want to put this in your ~/.bashrc or ~/.zshrc). Assuming you cloned depot_tools to /path/to/depot_tools:

```sh
$ export PATH="$PATH:/path/to/depot_tools"
```


### Get the code


Create a chromium directory for the checkout and change to it (you can call this whatever you like and put it wherever you like, as long as the full path has no spaces):

```sh
$ mkdir path/to/distributed_chrome && cd path/to/distributed_chrome
```

Download the code using the command below.

```sh
$ git clone https://github.sec.samsung.net/RS7-HighPerformanceWeb/distributed_chrome.git src
```

If you did not specify the 'src' directory name at the end of the command, the source code would have been downloaded to the 'distributed_chrome' directory. In this case, change the directory name.

```sh
$ mv distributed_chrome src
```

Install additional build dependencies

```sh
$ build/install-build-deps.sh
```


### Run the sync

We need a gclient configuration. To create a .gclient file, run:

```sh
$ build/create_gclient.sh
```

Once you've run install-build-deps at least once, you can now run the Chromium-specific sync, which will download additional binaries and other things you might need:

```sh
$ gclient sync --with_branch_head
```

If you get an SSL certificate error at Seoul R&D center, follow the directions below.

```sh
$ cd path/to/depot_tools
$ git checkout 3beabd0aa40ca39216761418587251297376e6aa
$ git apply path/to/distributed_chrome/src/build/SRnD_depot_tools.patch
```

### Setting up the build


Chromium uses Ninja as its main build tool along with a tool called GN to generate .ninja files. You can create any number of build directories with different configurations. To create a build directory, run:

```sh
$ gn gen out/Default
```

You set build arguments on a build directory by typing:

```sh
$ gn args out/Default
```

This will bring up an editor. Type build args into that file like this:

```
enable_chromie=true
enable_nacl=false
```


### Build distributed-chrome


Build Distributed Chromium (the “chrome” target) with Ninja using the command:

```sh
$ ninja -C out/Default chrome
```


### Run distributed-chrome


Device A: Browser Process

```sh
$ ./chrome <URL>
```

Device B: Renderer Process

```sh
$ ./chrome --type=renderer --server-address=<IP ADDR>
```
