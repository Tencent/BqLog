# Changelog

## [v1.4.0] - 2023-08-03
- **First Release**

## [v1.4.1] - 2023-08-30
- **Console Buffer**: In addition to passively intercepting the output of console appenders through console callbacks, it is also possible to cache the output of console appenders through a console buffer and actively retrieve it via API.
- **Bug fix**: Fix the compiling issue with C++ 20.
- **Bug fix**: Fix the issue where JNI_Onload sometimes is not called.