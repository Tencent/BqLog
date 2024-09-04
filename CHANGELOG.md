# Changelog

## [v1.4.0] - 2023-08-03
- **First Release**

## [v1.4.1] - 2023-08-30
- **Console Buffer**: In addition to passively intercepting the output of console appenders through console callbacks, it is also possible to cache the output of console appenders through a console buffer and actively retrieve it via API.
- **Bug fix**: Fix the compiling issue with C++ 20.
- **Bug fix**: Fix the issue where JNI_Onload sometimes is not called.

## [v1.4.2] - 2023-09-04
- **Uninit API**: An `uninit()` API has been added for calling before the program exits, to avoid issues where the program cannot exit normally in some cases. For details, see the API documentation.