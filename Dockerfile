FROM docker.grammatech.com:14850/synthesis/clang

RUN apt-get -y update && \
    apt-get -y install wdiff libtinfo-dev libz-dev libxcb1-dev pandoc

RUN mkdir -p /gt/clang-mutate/ && \
    git clone git@git.grammatech.com:synthesis/clang-mutate.git /gt/clang-mutate/ && \
    cd /gt/clang-mutate/ && \
    git checkout CI_COMMIT_SHA && \
    make -j4 clang-mutate && \
    make install

WORKDIR /gt/clang-mutate

CMD /bin/bash
