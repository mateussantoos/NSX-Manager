# NSX Manager build and verification environment.
#
# One image that can do everything CI does, so "works on my machine" and "passes
# CI" stop being different questions:
#
#   * devkitA64  - nsx-manager.nro and nsx-forwarder.nro
#   * devkitARM  - the RCM payload (GPL-2.0-only, separate program - ADR-0011)
#   * clang      - the host build for nsx_core plus its unit tests
#   * the lint, release and documentation toolchain
#
# Built on devkitpro/devkita64, which is Ubuntu with devkitPro's pacman already
# configured. devkitARM is added through that same package manager rather than a
# second image, so a single `docker compose run` can build all three binaries.
#
# SPDX-License-Identifier: GPL-3.0-only

FROM devkitpro/devkita64:latest

LABEL org.opencontainers.image.title="NSX Manager build environment"
LABEL org.opencontainers.image.source="https://github.com/mateussantoos/nsx-manager"
LABEL org.opencontainers.image.licenses="GPL-3.0-only"

ENV DEBIAN_FRONTEND=noninteractive

# --------------------------------------------------------------------------
# devkitARM - needed ONLY for apps/rcm-payload. Installed here so the whole
# project builds in one container; the licence isolation is enforced by the
# build files and tools/lint/check_license_isolation.sh, not by keeping the
# compilers in separate images.
# --------------------------------------------------------------------------
# devkitARM builds apps/rcm-payload ONLY. The Switch portlibs below are what
# the application links: zlib for zipper, curl+mbedtls for networking, and
# glfw/glad/mesa/drm_nouveau for Borealis. Installing them explicitly rather
# than pulling the whole switch-portlibs group keeps the image honest about
# what the project actually depends on.
RUN dkp-pacman -Sy --noconfirm --needed \
        devkitARM \
        switch-zlib \
        switch-curl \
        switch-mbedtls \
        switch-glfw \
        switch-glad \
        switch-mesa \
        switch-libdrm_nouveau \
    && rm -rf /opt/devkitpro/pacman/var/cache/pacman/pkg/*

# --------------------------------------------------------------------------
# Host toolchain and the verification tooling.
#
# clang is the host compiler for nsx_core and, unlike on Windows, its
# sanitizers work here - which is the whole reason the sanitizers job runs on
# Linux (see docs/contributing/testing.md).
# --------------------------------------------------------------------------
ARG LLVM_VERSION=18

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        python3 \
        python3-pip \
        python3-venv \
        git \
        curl \
        gnupg \
        ca-certificates \
        lsb-release \
        zip \
        unzip \
        doxygen \
        graphviz \
        jq \
    && rm -rf /var/lib/apt/lists/*

# clang is PINNED to one version, from LLVM's own apt repository rather than
# whatever the base distribution happens to ship.
#
# This is not fussiness. clang-format output differs between major versions, so
# an unpinned toolchain means check_format.sh gives a different answer on a
# developer machine, in this container, and in CI - and the lint becomes noise
# that people learn to ignore. One version everywhere is the only way the check
# means anything. CI installs this same version.
RUN curl -fsSL --proto '=https' --tlsv1.2 https://apt.llvm.org/llvm-snapshot.gpg.key \
        | gpg --dearmor -o /usr/share/keyrings/llvm-archive-keyring.gpg \
    && CODENAME="$(lsb_release -cs)" \
    && echo "deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] http://apt.llvm.org/${CODENAME}/ llvm-toolchain-${CODENAME}-${LLVM_VERSION} main" \
        > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        clang-${LLVM_VERSION} \
        clang-format-${LLVM_VERSION} \
        clang-tidy-${LLVM_VERSION} \
        libclang-rt-${LLVM_VERSION}-dev \
    && update-alternatives --install /usr/bin/clang        clang        /usr/bin/clang-${LLVM_VERSION} 100 \
    && update-alternatives --install /usr/bin/clang++      clang++      /usr/bin/clang++-${LLVM_VERSION} 100 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-${LLVM_VERSION} 100 \
    && update-alternatives --install /usr/bin/clang-tidy   clang-tidy   /usr/bin/clang-tidy-${LLVM_VERSION} 100 \
    && rm -rf /var/lib/apt/lists/*

# jsonschema backs tools/release/validate_manifest.py. --break-system-packages
# is correct in a container: there is no other Python consumer to protect.
RUN pip3 install --no-cache-dir --break-system-packages jsonschema

ENV DEVKITPRO=/opt/devkitpro \
    DEVKITARM=/opt/devkitpro/devkitARM \
    DEVKITPPC=/opt/devkitpro/devkitPPC \
    PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/opt/devkitpro/devkitARM/bin:$PATH \
    CC=clang \
    CXX=clang++

# Build directories are per-preset, so the host-debug tree built in the
# container never collides with a switch-release tree, and neither collides
# with anything a developer built outside it.
WORKDIR /workspace

# git refuses to operate on a bind-mounted repository owned by a different uid.
# This is a build container, not a shared host - the repository is trusted.
RUN git config --global --add safe.directory /workspace \
    && git config --global --add safe.directory '*'

COPY docker/entrypoint.sh /usr/local/bin/nsx-entrypoint
RUN chmod +x /usr/local/bin/nsx-entrypoint

ENTRYPOINT ["/usr/local/bin/nsx-entrypoint"]
CMD ["help"]
