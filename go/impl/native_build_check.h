// Generated distribution supports the desktop targets tested by release CI.
#if !defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__) && !defined(__FreeBSD__)
#error "This BqLog Go source distribution does not support this target"
#endif
#if defined(__ANDROID__)
#error "Use the platform BqLog build for Android"
#endif
