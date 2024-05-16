# RACE communications libraries
This repository contains a library that can be used to communicate with another user of the library over RACE Comms channels.

## Creating `raceboat` docker image
Build, create package, and build the docker image.  Use a `race-compile` container to build and create the package.  The following demonstrates building for an aarch64 target platform.  The flags will need to change for other target platforms, and is documented in the scripts.  

```bash
cd raceboat
docker run  -it --rm  -v $(pwd):/code/ -w /code ghcr.io/tst-race/race-images/race-compile:main bash
./build_it_all.sh
./create-package.sh --linux<-arm64>
exit
./docker-image/build_image.sh --platform-arm64
```

Building results in several libraries.  Two of which are suitable for real-world use.  The others support tests.
`./racesdk/package/LINUX*/lib/libraceboat.so`
    - mandatory core functionality with C++ APIs
    - could be copied to /usr/local/lib/ or somewhere in `$PATH`
`./build/LINUX*/language-shims/source/_commsPluginBindings.so`
    - optional library with python bindings
    - should go into shims path (see `FileSystem::makeShimsPath()`)
    - see [Using Plugin Bindings](./language-shims/README.md#using-python-bindings)
  
As with any C++ API, the header files are necessary for use.  
`./racesdk/package/LINUX*/include/*.h`
    - C++ API headers
    - could copied to /usr/local/include/ or somewhere in `$PATH`

`./racesdk/package/LINUX*/` will be `./racesdk/package/ANDROID*/` when building for Android.  

## Testing using race-cli
See race-email channel README.md for examples

The following demonstrates how to use race-cli client-connect and server-connect functions with direct twosix cpp exemplar channel in a race-sdk docker container.  Some of these paths will change slightly when not building on aarch64 host.  

NOTE: see `FileSystem::makePluginInstallBasePath()` for path requirements of where to copy the plugins to.  `find /code/private-race-core -name libraceboat.so` for raceboat.  

NOTE: `--param hostname=localhost` does not work in this test.  You can use the ip addresses reported from ifconfig on both nodes.  
### server
```bash

docker run -it -v $(pwd):/code/ -v /tmp/:/tmp/ race-sdk:latest bash 
cp -r /code/private-race-core/plugin-comms-twosix-cpp/kit/artifacts/linux-arm64-v8a-server/PluginCommsTwoSixStub/* /tmp/race/plugins/unix/arm64-v8a/PluginCommsTwoSixStub
ln -s /code/private-raceboat/build/LINUX_arm64-v8a/source/libraceboat.so /usr/local/lib/raceSdkCommon.so
cd /code/private-raceboat/build/LINUX_arm64-v8a/app/race-cli/
./race-cli --debug --dir /tmp/race -m --server-connect --send-channel twoSixDirectCpp --recv-channel twoSixDirectCpp --param hostname="172.17.0.4" --param PluginCommsTwoSixStub.startPort=26262 --param PluginCommsTwoSixStub.endPort=26264
```

### client
```bash
docker run -it -v $(pwd):/code/ -v /tmp/:/tmp/ race-sdk:latest bash
cp -r /code/private-race-core/plugin-comms-twosix-cpp/kit/artifacts/linux-arm64-v8a-client/PluginCommsTwoSixStub/* /tmp/race/plugins/unix/arm64-v8a/PluginCommsTwoSixStub
ln -s /code/private-raceboat/build/LINUX_arm64-v8a/source/libraceboat.so /usr/local/lib/raceSdkCommon.so
cd /code/private-raceboat/build/LINUX_arm64-v8a/app/race-cli/
./race-cli --debug --dir /tmp/race -m --client-connect --send-channel twoSixDirectCpp --recv-channel twoSixDirectCpp --param hostname="172.17.0.5" --param PluginCommsTwoSixStub.startPort=26262 --param PluginCommsTwoSixStub.endPort=26264 --send-address="{\"hostname\":\"172.17.0.4\",\"port\":26262}"
```


## Build and Run Unit Tests

Make sure the main project has already been built using the above commands

```bash
cd build/LINUX*/test/source/
make
./testStandaloneLibrary
```

## Formatting
```bash
cmake --build --preset=LINUX_x86_64 --target check_format -j
```

## APIs
See CommPluginDeveloperGuide.md in the `race-docs` repository for information about working with Comms plugins.  


## Updated Running Instructions
Work-in-progress as we make small tweaks/improvements to the race-cli user experience.

___Preliminaries:___
This approach volume-mounts plugin(s) of the appropriate architecture, so "path/to/plugin" would look something like: `/your/path/to/race-core/plugin-comms-twosix-cpp/kit/artifacts/linux-arm64-v8a-server/` to use channels from PluginCommsTwoSixStub on an arm64-v8a host.

### Server:

```bash
docker run --rm -it --name=rbserv \
--network=rib-overlay-network --ip=10.11.1.2 \
 -v /path/to/plugin:/kits \
raceboat:latest bash

echo "Hello from the Raceboat Server!" | race-cli --dir /kits -m --recv-reply --send-channel twoSixDirectCpp --recv-channel twoSixDirectCpp --param hostname="10.11.1.2" --param PluginCommsTwoSixStub.startPort=26262 --param PluginCommsTwoSixStub.endPort=26264 
```

### Client:

```bash
docker run --rm -it --name=raceboat-client \
--network=rib-overlay-network --ip=10.11.1.3 \
 -v /path/to/plugin:/kits \
raceboat:latest bash

echo "Hi, I'm the client!" | race-cli --dir /kits -m --send-recv --send-channel twoSixDirectCpp --recv-channel twoSixDirectCpp --param hostname="10.11.1.3" --param PluginCommsTwoSixStub.startPort=26262 --param PluginCommsTwoSixStub.endPort=26264 --send-address="{\"hostname\":\"10.11.1.2\",\"port\":26262}"

```

## TCP Socket Connection Testing

### Server:
```bash

#!/bin/bash

docker run --rm -it --name=rbserver --network=rib-overlay-network --ip=10.11.1.2 \
       -v $(pwd)/../plugin-comms-twosix-cpp/artifacts/linux-arm64-v8a-server/PluginCommsTwoSixStub:/server-kits/PluginCommsTwoSixStub \
       -v $(pwd):/code -w /code \
       -v $(pwd)/scripts/:/scripts/ \
       raceboat:latest bash
```

Start another terminal on the server before running `race-cli` to start `netcat`
```
docker exec -it <server container id> bash 
nc -l localhost 7777
```

back in the first server container terminal
```
build/LINUX_arm64-v8a/app/race-cli/race-cli --dir /server-kits --server-bootstrap-connect --recv-channel=twoSixDirectCpp --send-channel=twoSixDirectCpp --final-send-channel=twoSixDirectCpp --final-recv-channel=twoSixDirectCpp --param hostname="10.11.1.2" --param PluginCommsTwoSixStub.startPort=26262 --param PluginCommsTwoSixStub.endPort=26264 --debug | tee rrlog | grep ERROR
```


### Client:
```bash

#!/bin/bash

docker run --rm -it --name=rbclient --network=rib-overlay-network --ip=10.11.1.3 \
       -v $(pwd)/../plugin-comms-twosix-cpp/artifacts/linux-arm64-v8a-server/PluginCommsTwoSixStub:/client-kits/PluginCommsTwoSixStub \
       -v $(pwd):/code -w /code \
       -v $(pwd)/scripts/:/scripts/ \
       raceboat:latest bash

build/LINUX_arm64-v8a/app/race-cli/race-cli --dir /client-kits --client-bootstrap-connect --send-channel=twoSixDirectCpp --send-address="{\"hostname\":\"10.11.1.2\",\"port\":26262}" --recv-channel=twoSixDirectCpp --final-send-channel=twoSixDirectCpp --final-recv-channel=twoSixDirectCpp --param hostname="10.11.1.3" --param PluginCommsTwoSixStub.startPort=26262 --param PluginCommsTwoSixStub.endPort=26264 --debug | tee srlog | grep ERROR &

nc localhost 9999

```