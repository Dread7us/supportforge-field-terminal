# FastEPD evaluation snapshot provenance

This directory is a minimal, unmodified Arduino library snapshot used only by
the `fast_epd_h752_02_compile_only` PlatformIO environment.

- Authoritative repository: <https://github.com/Xinyuan-LilyGO/FastEPD>
- Branch at investigation time: `main`
- Immutable commit: `95f8696466e386fce84dbe10edb8713a8a9be387`
- Upstream library metadata version: `1.3.0`
- Included surface: `src/`, `library.properties`, and `LICENSE`
- Excluded surface: examples, Git metadata, and recursively referenced example
  submodules; none are required to compile the Arduino library.

The minimal snapshot avoids a Windows path-length failure caused when
PlatformIO recursively copied unrelated ESP-IDF example submodules from a Git
dependency checkout. Production environments do not discover this directory.

## SHA-256 manifest

Hashes are over the exact copied upstream files, before this provenance file
was added.

```text
12fb641cbf35194f97f73ff564eb77bf27495ebbba6173402b19e4e1c5b8eb21  library.properties
d211cc51eb077a26e8025537f578600ad098e78c70f4727bb6416294c29e9233  LICENSE
ad5588b9272d7a3712e6dfbe6b8f5fce98b4d52ee5359818a3b6060696842335  src/arduino_io.inl
6c5d51fc545ea8efcc2c9e98d52b1732bc9061ced2aa98f8a355ae4655c31181  src/bb_ep_gfx.inl
e5f140d96e09378f4c4c4f3de5a97e52804eb1cc85b1a71d626f90da811b9654  src/FastEPD.cpp
f01a9332619857088a1cb90796be5968b610f7bd80ce47e3e3ef7605f3287723  src/FastEPD.h
c8960ad5e962c4081eeaeb81adc6942f5039e4ab9f9bef743f4ff8e0ce178de5  src/FastEPD.inl
cd926bb89600873c2ce1c61c141b2fabdfadfefcec321a5c552adb1974ddec2a  src/g5dec.inl
c8d61d2a9f642c76802e667d1e9dda871a6c0b781aaf820fb051c247a096cc1c  src/g5enc.inl
7d45c4742221b74fecee86b1c0feea6579407b684b10590286ada4d898eb549a  src/Group5.cpp
3adb31afbb70f10bf35b8cb585e6870dcfe3af303d17a12a355be201854892b9  src/Group5.h
```