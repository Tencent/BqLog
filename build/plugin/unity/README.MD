# Unity Build Process

You may notice the absence of a local, all-in-one build script in this repository. This is intentional.

This project contains native binary plugins for multiple operating systems, each requiring its own specific SDK and build environment (e.g., Xcode for macOS/iOS, Visual Studio for Windows). As it is not feasible to build all of these on a single machine, the official build process is managed entirely by **GitHub Actions**.

The workflow is configured to compile each plugin on its respective platform, ensuring a consistent and reliable build for all targets.