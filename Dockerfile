# Multi-stage build for Skrumi
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
    xvfb-run libx11 openbox \
    bash

WORKDIR /usr/src/skrumi
COPY . .

RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DSKRUMI_STATIC_LINUX=ON \
    && cmake --build build --config Release -j$(nproc) \
    && FOXLANG_REQUIRE_GRAPHICS_TESTS=1 python3 tests/run_with_xvfb.py ctest --test-dir build --output-on-failure

# Export static Linux binaries, without the build tree or compiler.
FROM scratch AS portable
COPY --from=builder /usr/src/skrumi/build/skrumi /skrumi
COPY --from=builder /usr/src/skrumi/build/skrumi-lsp /skrumi-lsp
COPY --from=builder /usr/src/skrumi/build/foxlang /foxlang
COPY --from=builder /usr/src/skrumi/build/foxlang-lsp /foxlang-lsp

# Final runtime image
FROM alpine:3.24.2

# Copy binaries and standard library
COPY --from=builder /usr/src/skrumi/build/skrumi /usr/local/bin/skrumi
COPY --from=builder /usr/src/skrumi/build/skrumi-lsp /usr/local/bin/skrumi-lsp
COPY --from=builder /usr/src/skrumi/std /usr/local/share/skrumi/std
COPY --from=builder /usr/src/skrumi/VERSION /usr/local/share/skrumi/VERSION

ENV SKRUMI_HOME=/usr/local/share/skrumi

WORKDIR /app

RUN ln -s skrumi /usr/local/bin/foxlang && ln -s skrumi-lsp /usr/local/bin/foxlang-lsp

ENTRYPOINT ["skrumi"]
CMD ["--help"]
