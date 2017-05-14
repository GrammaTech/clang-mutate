
FROM docker.grammatech.com:14850/synthesis/clang:latest

RUN apt-get -y update && \
    apt-get -y install wdiff libtinfo-dev libz-dev libxcb1-dev

ENV GT_DOCKER_CHOWN_PATHS="$GT_DOCKER_CHOWN_PATHS /gt/clang-mutate"

RUN mkdir -p /gt/clang-mutate/ && \
    git clone git@git.grammatech.com:synthesis/clang-mutate.git /gt/clang-mutate/ && \
    cd /gt/clang-mutate/ && \
    git checkout CI_COMMIT_SHA && \
    make -j4 clang-mutate && \
    make install

WORKDIR __GTD__PATH_GT_ROOT

CMD /bin/bash
