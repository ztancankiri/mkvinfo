FROM alpine AS base

WORKDIR /repos

ENV CFLAGS=-fPIC
ENV CXXFLAGS=-fPIC

RUN apk update && apk add gcc g++ make cmake libtool lld git python3 python3-dev py3-pip \
    && pip install pybind11[global] \
    && cd /repos && git clone --depth 1 https://github.com/Matroska-Org/libebml.git && cd libebml \
    && mkdir build && cd build \
    && cmake .. -DBUILD_SHARED_LIBS=OFF -DENABLE_WIN32_IO=OFF \
    && make -j$(nproc) && make install \
    && cd /repos && git clone --depth 1 https://github.com/Matroska-Org/libmatroska.git && cd libmatroska \
    && mkdir build && cd build \
    && cmake .. -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF \
    && make -j$(nproc) && make install \
    && cd /repos && git clone --depth 1 https://github.com/zeux/pugixml.git && cd pugixml \
    && mkdir build && cd build \
    && cmake .. -DBUILD_SHARED_LIBS=OFF \
    && make -j$(nproc) && make install \
    && cd /repos && git clone --depth 1 https://github.com/fmtlib/fmt.git && cd fmt \
    && mkdir build && cd build \
    && cmake .. -DBUILD_SHARED_LIBS=OFF -DFMT_DOC=OFF -DFMT_TEST=OFF -DFMT_FUZZ=OFF \
    && make -j$(nproc) && make install

WORKDIR /app
RUN rm -rf /repos

COPY CMakeLists.txt .
COPY src src
COPY include include
COPY lib lib

RUN mkdir build && cd build && cmake .. && make -j$(nproc)

FROM alpine
WORKDIR /
COPY --from=base /app/build/*.so /