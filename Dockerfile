FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Configure, build and run the unit tests; any failure fails the image build.
RUN cmake -S /app -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DTP_WARNINGS_AS_ERRORS=ON \
    && cmake --build /build --parallel \
    && ctest --test-dir /build --output-on-failure

# Build tree lives outside /app so a host build/ dir copied in can't clash.
WORKDIR /build

# Run the pusher demo for a few seconds: [config_file] [num_pushers] [seconds]
CMD ["./tp_pusher_demo", "threadpool.ini", "2", "5"]
