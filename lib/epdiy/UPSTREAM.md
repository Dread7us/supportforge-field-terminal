# Vendored epdiy provenance

This directory is a trimmed snapshot of `lib/epdiy` from LILYGO's official
T5 E-Paper S3 Pro repository:

- Repository: <https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO>
- Branch used as the electrical/display candidate: `H752-01`
- Revision: `5067e1fd6a66cf8b06e0b484070dc1b405eac1aa`
- Source path: `lib/epdiy`

The copied files were verified byte-for-byte against that revision. The
non-build bulk directories `examples`, `hardware`, `doc`, `scripts`, and `test`
were omitted. The original `LICENSE`, library metadata, headers, sources, board
drivers, output backends, and waveform data are retained.

The LILYGO snapshot declares epdiy version `2.0.0`, but includes material local
changes to the V7 board/power and LCD-output implementation. Do not replace it
with stock upstream epdiy solely on the basis of the matching version string.
