FROM ubuntu:20.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get install -y \
      gcc \
      g++ \
      cmake \
      libasound2-dev \
      libpulse-dev \
      oss4-dev \
      mingw-w64 \
      make \
      curl && \
    mkdir -p /src/

# setup toolchain file - i686
RUN echo "set(CMAKE_SYSTEM_NAME Windows)" > /opt/i686-w64-mingw32.cmake && \
    echo "set(TOOLCHAIN_PREFIX i686-w64-mingw32)" >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH "/usr/${TOOLCHAIN_PREFIX};/opt/${TOOLCHAIN_PREFIX}")' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> /opt/i686-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> /opt/i686-w64-mingw32.cmake

# setup toolchain file - x86_64
RUN echo "set(CMAKE_SYSTEM_NAME Windows)" > /opt/x86_64-w64-mingw32.cmake && \
    echo "set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)" >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH "/usr/${TOOLCHAIN_PREFIX};/opt/${TOOLCHAIN_PREFIX}")' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> /opt/x86_64-w64-mingw32.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> /opt/x86_64-w64-mingw32.cmake

COPY . /src/liblwa

# regular ol linux compile
RUN mkdir -p /src/liblwa-build && \
    cd /src/liblwa-build && \
    cmake /src/liblwa && \
    make

RUN mkdir -p /src/liblwa-build-win32 && \
    cd /src/liblwa-build-win32 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=/opt/i686-w64-mingw32.cmake \
      /src/liblwa && \
    make

RUN mkdir -p /src/liblwa-build-win64 && \
    cd /src/liblwa-build-win64 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=/opt/x86_64-w64-mingw32.cmake \
      /src/liblwa && \
    make
