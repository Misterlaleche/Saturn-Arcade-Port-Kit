#!/usr/bin/env bash
set -euo pipefail
IMAGE="${YAUL_DOCKER_IMAGE:-ijacquez/yaul:latest}"
mkdir -p out
# Resolve and record the image actually used; no assumed successful build.
docker pull "$IMAGE"
docker image inspect "$IMAGE" --format '{{json .RepoDigests}}' > out/docker-digests.json
docker run --rm --entrypoint /bin/bash -v "$PWD/out:/out" "$IMAGE" -lc '
  set -eu
  ROOT="${YAUL_INSTALL_ROOT:-/opt/tool-chains/sh2eb-elf}"
  "$ROOT/bin/sh2eb-elf-gcc" --version > /out/compiler-version.txt
  printf "int smoke(int x) { return x + 1; }\n" > /out/smoke.c
  "$ROOT/bin/sh2eb-elf-gcc" -m2 -mb -ffreestanding -O2 -c /out/smoke.c -o /out/smoke.o
  "$ROOT/bin/sh2eb-elf-objdump" -f -d /out/smoke.o > /out/smoke-disassembly.txt
  test -f "$ROOT/share/build.pre.mk"
  tar -C "$(dirname "$ROOT")" -chJf /out/yaul-c-sdk.tar.xz \
    --exclude="*/share/doc" --exclude="*/share/man" --exclude="*/share/info" \
    --exclude="*/include/c++" --exclude="*/cc1plus" --exclude="*gdb*" \
    --exclude="*libstdc++*" --exclude="*/sh2eb-elf-g++" --exclude="*/sh2eb-elf-c++" \
    "$(basename "$ROOT")"
  find "$ROOT" -name cc1 -exec ldd {} \; > /out/compiler-dependencies.txt || true
  sha256sum /out/yaul-c-sdk.tar.xz /out/smoke.o > /out/SHA256SUMS.txt
'
# Split artifact payloads into transport-sized pieces. The manifest authenticates
# the reconstructed archive. This does not alter compiler bytes.
cd out
split --bytes=12000000 --numeric-suffixes=0 --suffix-length=2 yaul-c-sdk.tar.xz yaul-sdk.part-
sha256sum yaul-sdk.part-* > PARTS-SHA256SUMS.txt
ls -lh yaul-c-sdk.tar.xz yaul-sdk.part-*
