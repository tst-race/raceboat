FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

#RUN apt-get update && apt-get install -y software-properties-common

#RUN add-apt-repository ppa:deadsnakes/ppa
    

# Install base dependencies and debug tools (won't package python3.12-dev so we'll install here to test)
RUN apt-get update && apt-get install -y \
    unzip \
    ca-certificates \
    bash \
    file \
    strace \
    libc-bin \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /opt

# Copy the zipped package into the container
COPY raceboat_package_*.zip ./package.zip

# Unzip the package and detect the extracted directory
RUN unzip package.zip && \
    PKGDIR=$(find . -maxdepth 1 -type d -name 'raceboat_package_*' | head -n 1) && \
    chmod +x "$PKGDIR/start.sh" && \
    [ -f "$PKGDIR/diagnose.sh" ] && chmod +x "$PKGDIR/diagnose.sh" && \
    echo "$PKGDIR" > /opt/pkgdir.txt

# Dynamically set the working directory in a new layer
RUN PKGDIR=$(cat /opt/pkgdir.txt) && \
    echo "cd $PKGDIR" >> /opt/temp.sh && \
    echo "./start.sh" >> /opt/temp.sh && \
    chmod +x /opt/temp.sh
