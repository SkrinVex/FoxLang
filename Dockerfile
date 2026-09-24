# Multi-stage build for FoxLang
FROM alpine:3.24.2 AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    make \
    python3 \
    curl \
    openssl \
    ca-certificates \
    pkgconf \
    libxcb-dev libxcb-static libxau-dev libxdmcp-dev libbsd-static libmd-dev \
    xvfb-run libx11 \
    bash

WORKDIR /usr/src/foxlang
COPY . .

RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DFOXLANG_STATIC_LINUX=ON \
    && cmake --build build --config Release -j$(nproc) \
    && FOXLANG_REQUIRE_GRAPHICS_TESTS=1 xvfb-run -a ctest --test-dir build --output-on-failure

# Export static Linux binaries, without the build tree or compiler.
FROM scratch AS portable
COPY --from=builder /usr/src/foxlang/build/foxlang /foxlang
COPY --from=builder /usr/src/foxlang/build/foxlang-lsp /foxlang-lsp

# Final runtime image
FROM alpine:3.24.2

# Copy binaries and standard library
COPY --from=builder /usr/src/foxlang/build/foxlang /usr/local/bin/foxlang
COPY --from=builder /usr/src/foxlang/build/foxlang-lsp /usr/local/bin/foxlang-lsp
COPY --from=builder /usr/src/foxlang/std /usr/local/share/foxlang/std
COPY --from=builder /usr/src/foxlang/VERSION /usr/local/share/foxlang/VERSION

ENV FOXLANG_HOME=/usr/local/share/foxlang

WORKDIR /app

ENTRYPOINT ["foxlang"]
CMD ["--help"]
