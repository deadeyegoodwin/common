# C++ utilities

Library of utility function and classes. The `common` namespace is
used for all functions and classes.

## Build

Build `libcommon.a` using cmake.

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

Use -DASAN=ON to build with ASAN memory checking enabled.

```
$ cmake -DASAN=ON ..
```

Use make target `common-test` to run tests.

```
$ make common-test
```
