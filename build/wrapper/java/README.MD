# README

## 1. Script: JarGenerator

This script has two purposes:

1. To generate a JAR file, which will be located in `(ProjectRoot)/artifacts/wrapper/java` directory. The reason we do not provide a pre-generated JAR file in the `dist` directory is that we do not know which version of Java the user will be using. The binaries for Java versions before and after Java 11 are incompatible. Therefore, users need to generate the JAR themselves or directly copy the Java wrapper source code into their own project.
2. To generate the corresponding JNI header file, which will automatically replace the previous header file. The file will be located in `(ProjectRoot)/src/bq_log/api/bq_impl_log_invoker.h`.

## 2. EclipseProject Directory

This is the Eclipse project for the corresponding JAR, used for editing and debugging.
