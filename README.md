
# What is CASTANETS?
Castanets is an evolutionary **web browser for next generation networks** (5G and beyond).  

The proliferation of smart electronic devices and the advancement of high-speed wireless networks have been driving the exponential growth of web users and web applications. As specifications of the web technologies expand, the memory consumption and computing power requirements of the browsers are increasing significantly. However, the web browsers are becoming unsuitable for embedded devices and consumer electronics devices. Unlike PCs and smartphones, both computing capabilities and memory capacities are extremely limited in IoT-enabled devices. Recent Chromium, in general, consume more than 150MB of memories to view popular websites. When the number of tabs increases, the memory usages increase proportionally and turn out to be unacceptable for embedded devices.  

Considering aforementioned aspects of browser overheads, we propose an in-home edge-distributed split web browser; Castanets. The split rendering processes run memory intensive component on the distributed in-home edge devices while the browser process runs GPU intensive and composites results on the screen.  

If you are interested to join the project, _don't hesitate to contact us!_  
For any inquiries or questions regarding Castanets, we will gladly reply!  
**castanets _atsign_ samsung.com**

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
$ mkdir $PWD/castanets && cd $PWD/castanets
```

Download the code using the command below.

```sh
$ git clone https://github.com/Samsung/castanets src
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


### Setting up the build


Chromium uses Ninja as its main build tool along with a tool called GN to generate .ninja files. You can create any number of build directories with different configurations. To create a build directory, run:

```sh
$ gn gen --args='enable_castanets=true enable_nacl=false is_debug=false is_component_build=true' out/Default
```


### Build castanets


Build castanets (the “chrome” target) with Ninja using the command:

```sh
$ ninja -C out/Default chrome
```


### Run castanets in a local machine (test only)

Start first chrome instance: Browser Process

```sh
$ out/Default/chrome <URL>
```

Start second chrome instance: Renderer Process

```sh
$ out/Default/chrome --type=renderer --server-address=127.0.0.1
```

### Run castanets in a distributed environment


Device A: Browser Process

```sh
$ out/Default/chrome <URL>
```

Device B: Renderer Process

```sh
$ out/Default/chrome --type=renderer --enable-castanets=<IP ADDR>
```
