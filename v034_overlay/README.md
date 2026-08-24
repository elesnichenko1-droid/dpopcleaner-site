# v034_overlay

This directory contains only intentional, reviewable source overrides/new files for DPopCleaner 0.3.4.

Build flow:
1. recover the verified 0.3.3 donor;
2. copy donor `v033` to temporary `v034`;
3. copy this directory over the temporary `v034` tree;
4. apply guarded identity bump to `0.3.4 BETA R1` / version code `3041`;
5. build/test the resulting normalized source on Windows.

Rules:
- no binaries;
- no symlinks;
- paths are relative to the generated `v034` root;
- do not copy historical 0.2.14 executable code into this directory;
- every behavior change must have a regression/contract test.
