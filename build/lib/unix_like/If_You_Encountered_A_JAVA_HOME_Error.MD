If you encounter an error during the build process: ***"can not find environment variable `JAVA_HOME`"***, this is because BqLog comes with Java support by default. You have three options: 
- Install JDK, set the environment variable `JAVA_HOME` to point to the JDK directory, and rebuild.  
- Modify the relevant build script, remove the -DJAVA_SUPPORT=ON, but this way your BqLog cannot be used in a Java environment.  
- If you are building directly from CMake or CMake-GUI, please turn off the JAVA_SUPPORT option, but this way your BqProp cannot be used in a Java environment.  