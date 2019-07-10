# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

add_definitions(
        -DLINUX
        -DANDROID)

include_directories(../../../Component/mmBASE/BaseAPI)
include_directories(../../../Component/mmBASE/SubSystem)
include_directories(../../../Component/mmDiscovery)
include_directories(../../../Component/mmINC)
include_directories(../../../Component/mmOSAL)
include_directories(../../../Component/mmSH)
include_directories(../../../Component/mmSOCK)
include_directories(src/main/cpp)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        sd-server-lib

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ../../../Component/mmBASE/BaseAPI/bFile.cpp
        ../../../Component/mmBASE/BaseAPI/bINIParser.cpp
        ../../../Component/mmBASE/BaseAPI/bMessage.cpp
        ../../../Component/mmBASE/BaseAPI/bSocket.cpp
        ../../../Component/mmBASE/BaseAPI/bTask.cpp
        ../../../Component/mmBASE/BaseAPI/bThread.cpp
        ../../../Component/mmBASE/BaseAPI/ifaddrs.cpp

        ../../../Component/mmBASE/SubSystem/Debugger.cpp
        ../../../Component/mmBASE/SubSystem/Dispatcher.cpp

        ../../../Component/mmDiscovery/discovery_server.cpp
        ../../../Component/mmDiscovery/monitor_server.cpp
        ../../../Component/mmDiscovery/service_launcher.cpp
        ../../../Component/mmDiscovery/service_server.cpp

        ../../../Component/mmOSAL/daemonAPI.cpp
        ../../../Component/mmOSAL/ioAPI.cpp
        ../../../Component/mmOSAL/osal.cpp
        ../../../Component/mmOSAL/posixAPI.cpp
        ../../../Component/mmOSAL/processAPI.cpp
        ../../../Component/mmOSAL/socketAPI.cpp
        ../../../Component/mmOSAL/timeAPI.cpp
        ../../../Component/mmOSAL/uartAPI.cpp

        ../../../Component/mmSH/server_runner.cpp

        ../../../Component/mmSOCK/pTcpClient.cpp
        ../../../Component/mmSOCK/pTcpServer.cpp
        ../../../Component/mmSOCK/pUdpClient.cpp
        ../../../Component/mmSOCK/pUdpServer.cpp

        src/main/cpp/server_runner_jni.cpp)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        sd-server-lib

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})