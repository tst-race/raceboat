# RACE communications libraries
This repository contains a library that can be used to communicate with another user of the library over RACE Comms channels.

## Creating `raceboat` docker image
Build, create package, and build the docker image.  Use a `race-compile` container to build and create the package.  The following demonstrates building for an aarch64 target platform.  The flags will need to change for other target platforms, and is documented in the scripts.  

```bash
cd raceboat
docker run  -it --rm  -v $(pwd):/code/ -w /code race-compile:main bash
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
