# Pluggable Transport
This project implements the PT interface as defined [here](https://www.pluggabletransports.info/spec/), and calls into `libraceboat.so` in order to use RACE channels (see the `race-docs` repository).  


## Building in a `raceboat` docker container
The `raceboat` docker image must first be built (see ../docker-image/README.md) [built](../README.md#creating-raceboat-docker-image).  Then the golang shims must be initialized.  Afterwards this can be built with the `./build.sh` script.  For example:

```bash
cd ../
docker run  -it --rm  -v $(pwd):/code/ raceboat:latest bash
export MAKEFLAGS="-j"
./build_it_all.sh
cd pluggable-transport
/build.sh -p=/code/build/LINUX_arm64-v8a/language-shims/golang/include/src/core
```

The core shims install path is where corePluginBindingsGolang.go and go.mod reside after building raceboat.  This path may differ depending on host architecture, and will be reported in stdout during the install phase of `raceboat/build_it_all.sh`.

## Testing in a `raceboat` docker container
Running the built-in test requires the target Comms lib artifacts (binary, manifest.json) to exist where the `raceboat` docker container can find these artifacts.  These Comms libs are built as part of race-core.  See race-core/racesdk/README.md to build the Comms plugins.  If one wanted to use the twoSixDirectCpp Comms plugin in an arm-based race-sdk image to run the built in tests
1. build race-core
2. copy or `docker mount` race-core/plugin-comms-twosix-cpp into raceboat/
3. create link from libraceSdkCommon.so to libraceboat.so.  Note that creating the link works because the Core and Comms API signatures are consistent.
4. run the race-pt3 tests

For example:
```bash
cd race-comms-lib/pluggable-transport/
mkdir -p /etc/race/plugins/unix/arm64-v8a/
cp -r ../../race-core/plugin-comms-twosix-cpp/kit/artifacts/linux-arm64-v8a-client/* /etc/race/plugins/unix/arm64-v8a/
ln -s /linux/arm64-v8a/lib/libraceboat.so /linux/arm64-v8a/lib/libraceSdkCommon.so
./build/LINUX_arm64-v8a/source/tests/tests
```

## Testing the dispatcher with `ptadapter` to interface Pluggable Transports (PTv1)
[`ptadapter`](https://github.com/twisteroidambassador/ptadapter) is designed to run a server and one or more clients.  Each instance requires a [config script](https://ptadapter.readthedocs.io/en/latest/console_script.html).  

Start a `raceboat` container and install [ptadapter](https://github.com/twisteroidambassador/ptadapter).  Build the pluggable transport build and verify the successful operation of the [testing](#testing-in-a-raceboat-docker-container).  

Start server and client instances of ptadapter, then use netcat to send a message from the client to the server by typing the desired message followed by <enter>.  Confirm "hello from client" is seem by the server.  Confirm subsequent messages to and from the server.  

```bash
cd raceboat
docker run  -it --rm  -v $(pwd):/code/ raceboat:latest bash
cd code/pluggable-transport
pip install ptadapter
mkdir /log
apt-get install -y netcat
ptadapter --server --verbose ./example_config.ini &
nc -l 127.0.0.1 7000
```

Start the client in another docker container:
```bash
docker exec  -it <container name from `docker container list`> bash
cd code/pluggable-transport
ptadapter --client --verbose ./example_config.ini &
nc 127.0.0.1 8000
hello from client
```

NOTE: the path to raceDispatcher in example_config.ini may need to be adjusted depending on host architecture

## To view the race logs:
```bash
 cat /etc/race/logging/core/<target>.log
```
