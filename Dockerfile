FROM ubuntu:24.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Add required repositories
RUN apt-get update && apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test

# Install all apt packages in one layer
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++-11 \
    gettext \
    git \
    libkf5syntaxhighlighting-dev \
    libkf5texteditor-dev \
    pkg-config \
    qtbase5-dev \
    qtdeclarative5-dev \
    libqt5svg5-dev \
    texlive-fonts-extra \
    texlive-latex-extra \
    texlive-science \
    unzip \
    uuid-dev \
    wget \
    extra-cmake-modules

# Verify KF5 package installation and header locations
# Find KF5 header locations
RUN echo "=== Contents of KF5 syntax highlighting package ===" && \
    dpkg -L libkf5syntaxhighlighting-dev && \
    echo "=== Looking for abstract*.h files ===" && \
    find /usr -name "abstract*.h" && \
    echo "=== Looking in specific KF5 locations ===" && \
    ls -R /usr/include/KF5 && \
    echo "=== Looking in lib locations ===" && \
    ls -R /usr/lib/x86_64-linux-gnu/cmake/KF5SyntaxHighlighting
RUN find /usr -name "*.pc" | grep -i KF5
RUN ls -la /usr/lib/x86_64-linux-gnu/pkgconfig/
RUN ls -la /usr/share/pkgconfig/
RUN find /usr -name "KF5SyntaxHighlighting.pc"
RUN find /usr/include/KF5 -type f | grep -i abstract
RUN echo "PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:/usr/lib/pkgconfig" >> /etc/environment
RUN . /etc/environment && pkg-config --list-all | grep -i KF5

# Build and install ANTLR4
WORKDIR /tmp/antlr4
RUN wget https://www.antlr.org/download/antlr4-cpp-runtime-4.9.3-source.zip && \
    unzip antlr4-cpp-runtime-4.9.3-source.zip && \
    env CXX=g++-11 cmake . -DANTLR4_INSTALL=ON -DWITH_DEMO=False && \
    make install

# Clone and build Potato Presenter
WORKDIR /app
COPY . .
RUN mkdir -p build && \
    cd build && \
    echo "=== Looking for abstracthighlighter.h ===" && \
    find /usr -name "abstracthighlighter.h" && \
    echo "=== KF5 include directory contents ===" && \
    ls -R /usr/include/KF5 && \
    env CXX=g++-11 cmake -DCMAKE_PREFIX_PATH="/usr/lib/x86_64-linux-gnu/cmake;/usr/local/lib/cmake/antlr4/" \
    -DCMAKE_INCLUDE_PATH="/usr/include/KF5" \
    -DCMAKE_CXX_FLAGS="-I/usr/include/KF5 -I/usr/include/KF5/KSyntaxHighlighting/KSyntaxHighlighting" \
    -DCMAKE_VERBOSE_MAKEFILE=ON .. && \
    VERBOSE=1 make

# Set working directory to the binary location
WORKDIR /app/build

# Command to run the application
CMD ["./PotatoPresenter"]
