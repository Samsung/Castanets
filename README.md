
# How to build & run castanets


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
$ mkdir path/to/castanets && cd path/to/castanets
```

Download the code using the command below.

```sh
$ git clone -b castanets_69 https://github.com/Samsung/castanets src
```

If you did not specify the 'src' directory name at the end of the command, the source code would have been downloaded to the 'castanets' directory. In this case, change the directory name.

```sh
$ mv castanets src
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
$ git apply path/to/castanets/src/build/SRnD_depot_tools.patch
```
If you get SSL3_GET_SERVER_CERTIFICATE error, follow the directions below.


Add below line to .bashrc file.

```sh
export NO_AUTH_BOTO_CONFIG=~/.boto
```


Create ~/.boto file for the following content. 

```sh
[Boto]
proxy = 10.112.1.178
proxy_port = 8080
https_validate_certificates = False
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
enable_castanets=true
enable_nacl=false
```

For faster builds, the below can also be added in out/Default/args.gn

```
is_debug=false
remove_webcore_debug_symbols=true
is_component_build=true
use_jumbo_build=true
```


### Build castanets


Build castanets (the “chrome” target) with Ninja using the command:

```sh
$ autoninja -C out/Default chrome
```


### Run castanets in a local machine (test only)

Start first chrome instance: Browser Process

```sh
$ out/Default/chrome <URL>
```

Start second chrome instance: Utility Process (Network Service)

```sh
$ out/Default/chrome --type=utility --server-address=127.0.0.1
```

Start third chrome instance: Renderer Process

```sh
$ out/Default/chrome --type=renderer --server-address=127.0.0.1
```

### Run castanets in a distributed environment


Device A: Browser Process

```sh
$ out/Default/chrome <URL>
```

Device B: Utility Process (Network Service)

```sh
$ out/Default/chrome --type=utility --server-address=<IP ADDR>
```
Device B: Renderer Process

```sh
$ out/Default/chrome --type=renderer --server-address=<IP ADDR>
```

