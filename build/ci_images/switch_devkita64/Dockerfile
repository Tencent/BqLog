# CI image for the Nintendo Switch builds of BqLog.
#
# Covers both development environments:
#   - devkitPro/libnx (homebrew): static library + unit test ELF via
#     devkitPro's Switch.cmake toolchain.
#   - official Nintendo SDK (nn::): syntax-check harness
#     (build/test/switch_nn/run_syntax_check.sh) against public
#     open-ead/nnheaders, cloned at runtime by the harness.
#
# The upstream image already ships the devkitA64 GCC toolchain, libnx,
# cmake, ninja, git and ca-certificates, so nothing extra is installed.
FROM devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282

# Compiler and packaging tools live outside the default PATH in the upstream
# image; add them so CI steps can invoke cmake/aarch64-none-elf-g++ directly.
ENV PATH="/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:${PATH}"
