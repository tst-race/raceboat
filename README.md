# **Raceboat**

## **Introduction**
Raceboat is a framework for censorship circumvention channels. It provides two distinct interfaces, an _application interface_ for applications that need to circumvent censorship (akin to Pluggable Transports), and a _channel interface_ for different methods for circumventing censorship. You can read the [research paper](https://petsymposium.org/popets/2024/popets-2024-0027.pdf) here.

Raceboat differs from Pluggable Transports in 1) enabling using multiple channels simultaneously for different parts of communication (e.g. combining undirectional channels to provide bidirectionality, bootstrapping private links from public links); and 2) providing multiple modes of application use, including push, request-response, socket connections, and bootstrapping socket connections.

Raceboat provides two different interfaces for censorship circumvention channels: a _unified interface_ similar to a pluggable transport, namely support for establishing links and then sending and receiving packages on them; Raceboat also provides a _decomposed interface_ that enables composing a channel at runtime by assembling it from User Model, Transport, and Encoding components.


## **Building**
Raceboat uses a docker-based build process, running the below may take some time to download the compilation image. This build command also creates a `raceboat` runtumie docker image with the shared objects and executable built into it for easy testing and execution. **These are all built in GitHub CI and available from the `ghcr.io/tst-race/raceboat` namespace.

- `ghcr.io/tst-race/raceboat/raceboat-builder:main`: an image for building the raceboat framework itself (i.e. the code in this repository)

- `ghcr.io/tst-race/raceboat/raceboat-plugin-builder:main`: extension of the raceboat-builder with prebuilt framework binaries, used to compile plugins that provide channels or components for raceboat to use.

- `ghcr.io/tst-race/raceboat/raceboat:main`: runtime image which contains both prebuilt framework binaries and runtime dependencies.

Use the following commands to build a local set of x86_64 images:

```bash
pushd raceboat-builder-image && \
./build_image.sh -n ghcr.io/tst-race/raceboat --platform-x86_64 && \
popd && \
docker run -it --rm --name=build-pt \
       -e MAKEFLAGS="-j" \
       -v $(pwd)/:/code/ \
       -w /code \
       raceboat-builder:latest \
       ./build.sh && \
pushd raceboat-plugin-builder-image && \
./build_image.sh -n ghcr.io/tst-race/raceboat --platform-x86_64 && \
popd && \
pushd raceboat-runtime-image && \
./build_image.sh -n ghcr.io/tst-race/raceboat --platform-x86_64 && \
popd
```

To build an aarch64 raceboat image locally:

```bash
./raceboat-builder-image/build_image.sh --platform-arm64
./raceboat-plugin-builder-image/build_image.sh --platform-arm64
docker run  -it --rm  -v $(pwd):/code/ -w /code raceboat-builder:latest bash 
rm -fr build 
export MAKEFLAGS="-j" 
./build_it_all.sh 
./create-package.sh --linux-arm64
exit
./docker-image/build_image.sh --platform-arm64
```

(Some) host dependencies:
- golang v1.23.1 (change in build files if another version is preferred)

## **Building Plugins**

Users can compile plugins/channels with the resulting raceboat-plugin-builder:<tag>.  For example:

```bash
cd ../race-obfs
rm -fr build
rm -fr kit
./build_artifacts_in_docker_image.sh -l -v latest
cp -fr kit/artifacts/linux-<arch>-server/PluginObfs ../raceboat/kits
```

## **Running**

The following commands run the different Raceboat modes using a simple TCP socket called `twoSixDirectCpp` provided by `PluginCommsTwoSixStub` and assumes you have downloaded or built that plugin and placed it in a `kits` directory that is volume-mounted into the docker container (this is the easiest development process for testing your own plugins/channels). _You can get a copy of some prebuilt kits for both arm64-v8a and x86_64 processors [here](https://github.com/tst-race/raceboat/releases/download/pets24/kits.tgz).  Download the prebuilt kits, extract the compressed folder, then copy the contents of the architecture specific files to ./kits (e.g ./kits/PluginCommsTwoSixStub).  

In each case, you need to run two raceboat instances, we conventionally refer to these as a "client" and a "server"; generally the server should be started first (this isn't _necessary_ for some channels, but _is_ necessary when using a direct IP connection like `twoSixDirectCpp`). 

```bash
docker network create raceboat-network --subnet=10.11.1.0/24
```

### Basic Push

This is a unidirectional single-message push from the client to the server.

**Server**

```bash
docker run --rm -it --name=rbserver \
       --network=raceboat-network \
       --ip=10.11.1.3 \
       -v $(pwd)/kits:/server-kits \
       raceboat:latest bash -c \
       'race-cli -m --recv --quiet \
       --dir /server-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.3" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269'
```

__NOTE__: This outputs a line `Listening on {"hostname":"10.11.1.3","port":26262}` which is the _link address_ for the link the server is listening on. This value (`{"hostname":"10.11.1.3","port":26262}`) is created by the recv-channel chosen (twoSixDirectCpp in this case) _and is passed in as the --send-address argument of the client command_. In this case, the link address looks equivalent to a TCP socket address because twoSixDirectCpp is just a TCP socket. Other channels can produce wildly different link addresses, but they will always be stringified JSON.

**Client**
```bash
docker run --rm -it --name=rbclient \
       --network=raceboat-network \
       --ip=10.11.1.4 \
       -v $(pwd)/kits:/client-kits -v $(pwd):/code -w /code \
       raceboat:latest bash -c \
       'echo "Raceboat Client says Hello " \
       | race-cli -m --send --quiet \
       --dir /client-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.4" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269 \
       --send-address="{\"hostname\":\"10.11.1.3\",\"port\":26262}"'
```

### Request-Reply

The request-reply mode supports the client sending a single message and the server replying with a single message. The server message is pre-specified (if the below pipe-formulation is not used, the message must be entered and terminated with `^D` before the server starts listening). The default behavior is for the server to stay alive and respond to requests indefinitely. If `--num-packages` is passed, the server will shutdown after that number of messages.

There is a "fork" of the race-cli called `bridge-distro` which demonstrates adapting this mechanism for bridge distribution: a list of bridges and a passphrase are provided as arguments - the bridge lines are rotated through as responses each time a client messages with the correct passphrase. [See brige-distro command]()

**Server**

```bash
docker run --rm -it --name=rbserver \
       --network=raceboat-network \
       --ip=10.11.1.3 \
       -v $(pwd)/kits:/server-kits \
       raceboat:latest bash -c \
       'echo "Welcome, I am the Raceboat Server" \
       | race-cli -m --recv-reply --quiet \
       --dir /server-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.3" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269'
```

**Client**
```bash
docker run --rm -it --name=rbclient \
       --network=raceboat-network \
       --ip=10.11.1.4 \
       -v $(pwd)/kits:/client-kits \
       raceboat:latest bash -c \
       'echo "Raceboat Client says Hello " \
       | race-cli -m --send-recv --quiet \
       --dir /client-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.4" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269 \
       --send-address="{\"hostname\":\"10.11.1.3\",\"port\":26262}"'
```

### Bootstrapping Socket Connection

The bootstrapping socket connection mode is more complex - it uses an _initial_ set of links to bootstrap a continuous connection over a _final_ set of links. These links can be entirely different channels, so this mode is analogous to performing a bridge request and then using that bridge, just wrapped together in a single execution.

Additionally, raceboat uses local sockets to provide the connection functionality. On the server-side, the raceboat server makes a TCP connection to the specified `hostname:port`, so the application server needs to be already running and available at that TCP address. On the client-side, the raceboat client listens on a specified localhost port and the client application connecting to this port triggers starting the raceboat connection.

For exemplary purposes, we use `netcat` as the server application.

**Server**

```bash
docker run --rm --name=rbserver -d \
       --network=raceboat-network \
       --ip=10.11.1.3 \
       -v $(pwd)/kits:/server-kits \
       raceboat:latest bash -c \
       'race-cli -m --server-bootstrap-connect --quiet \
       --dir /server-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --final-recv-channel=twoSixDirectCpp \
       --final-send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.3" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269 --timeout=15'

docker exec -it rbserver bash -c 'nc -l localhost 7777'
```

**Client**

```bash
docker run --rm -it --name=rbclient -d \
       --network=raceboat-network \
       --ip=10.11.1.4 \
       -v $(pwd)/kits:/client-kits \
       raceboat:latest bash -c \
       'race-cli -m --client-bootstrap-connect --quiet \
       --dir /client-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --final-recv-channel=twoSixDirectCpp \
       --final-send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.4" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269 \
       --send-address="{\"hostname\":\"10.11.1.3\",\"port\":26262}" --timeout=15'

docker exec -it rbclient bash -c 'nc localhost 9999'
```

After running the above, the client and server should be able to chat back-and-forth using netcat. If both remain silent for five minutes the raceboat connection will be torn down (this is configurable via a --timeout argument, which should be the same for both sides).

## Bridge Distribution Application
One of the motivating use-cases for Raceboat is for bridge distribution. We have created a "fork" of the main race-cli that provides a simple implementation of a password-protected bridge request (password: "bridge-please"). 

**Server**
```bash
docker run --rm -it --name=rbserver \
       --network=raceboat-network \
       --ip=10.11.1.3 \
       -v $(pwd)/kits:/server-kits \
       -v $(pwd)/scripts:/scripts \
       raceboat-runtime:latest bash -c \
       'echo "Welcome, I am the Raceboat Server" \
       | bridge-distro --quiet \
       --passphrase bridge-please \
       --responses-file /scripts/example-responses.txt \
       --dir /server-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.3" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269'
```

**Client**

```bash
docker run --rm -it --name=rbclient \
       --network=raceboat-network \
       --ip=10.11.1.4 \
       -v $(pwd)/kits:/client-kits \
       raceboat-runtime:latest bash -c \
       'echo "bridge-please" \
       | race-cli -m --send-recv --quiet \
       --dir /client-kits \
       --recv-channel=twoSixDirectCpp \
       --send-channel=twoSixDirectCpp \
       --param hostname="10.11.1.4" \
       --param PluginCommsTwoSixStub.startPort=26262 \
       --param PluginCommsTwoSixStub.endPort=26269 \
       --send-address="{\"hostname\":\"10.11.1.3\",\"port\":26262}"'
```

## Decomposed Channel Development
For more information on developing decomposed channel _components_ (i.e. user models, transports, and encodings) see the [Decomposed Developer Guide](./documentaiton/DecomposedDeveloperGuide.md).