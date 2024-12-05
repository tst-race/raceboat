# Race SDK Docker Image

## Building on M1 Mac

Currently there is no support for building x86 artifacts on M1 Macs. It is still possible to build raceboat Docker images, however they will only include ARM artifacts. To build the Docker image follow these steps (all paths are relative to the root of the project).  Note that it may be necessary to `rm -fr build` to clear out stale incompatible image components.  

1. From inside a `race-compile` container build the linux artifacts:
    ```bash
    cd raceboat
    docker run -it --rm  -v $(pwd):/code/ race-compile:latest bash
    cd code
    export MAKEFLAGS="-j"
    ./build_it_all.sh
    ./create-package.sh --linux-arm64
    ```
2. From you host machine build the image:
    ```bash
    ./docker-image/build_image.sh --platform-arm64
    ```
3. Start the new container, and confirm the desired architecture is as expected
   ```bash
   docker run -it --rm  -v $(pwd):/code/ raceboat:latest bash
   uname -a
   ```
