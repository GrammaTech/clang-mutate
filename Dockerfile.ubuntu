FROM docker.grammatech.com/synthesis/clang as clang
FROM ubuntu:16.04
RUN apt-get -y update && \
    apt-get -y install autogen autoconf build-essential wdiff libtinfo-dev libtool libz-dev libxcb1-dev pandoc

COPY --from=clang /usr/synth /usr/synth
ENV PATH=$PATH:/gt/bin:/usr/synth/bin/ \
    LD_LIBRARY_PATH=/usr/synth/lib/

# Copy in clang-mutate directory.
ARG GT
COPY . /gt/clang-mutate

# Build clang-mutate package and install.
WORKDIR /gt/clang-mutate
RUN make -j4 clang-mutate
RUN make install
