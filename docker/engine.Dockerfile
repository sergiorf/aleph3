FROM ubuntu:24.04 AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates cmake g++ git make \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --target aleph3_engine_service --config Release

FROM ubuntu:24.04
WORKDIR /app
COPY --from=build /src/build/bin/aleph3_engine_service /app/aleph3_engine_service
ENV ALEPH3_ENGINE_PORT=8080
EXPOSE 8080
ENTRYPOINT ["/app/aleph3_engine_service"]
