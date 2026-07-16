FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config \
    libpq-dev git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y libpq5 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /app/build/bin/TCPServer /app/TCPServer
COPY --from=builder /app/config.json /app/config.json
RUN mkdir -p logs

EXPOSE 4124
CMD ["./TCPServer"]