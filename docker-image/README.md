# Race SDK Docker Image

## Building on M1 Mac

Currently there is no support for building x86 artifacts on M1 Macs. It is still possible to build raceboat Docker images, however they will only include ARM artifacts. To build the Docker image follows these steps (all paths are relative to the root of the project).

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
    ./build_raceboat_image.sh --platform-arm64
    ```
3. Start the new container, and confirm the desired architecture is as expected
   ```bash
   docker run -it --rm  -v $(pwd):/code/ raceboat:latest bash
   uname -a
   ```
