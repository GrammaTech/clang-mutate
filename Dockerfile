FROM base/archlinux

ADD .ssh/id_rsa.testbot /root/.ssh/id_rsa
ADD .ssh/id_dsa.testbot /root/.ssh/id_dsa
ADD .ssh/known_hosts    /root/.ssh/known_hosts

RUN chmod 600 /root/.ssh/id_rsa && \
    chmod 600 /root/.ssh/id_dsa

RUN pacman --noconfirm -Syu

RUN pacman --noconfirm -S base-devel openssh git sed wget rsync gzip pandoc

# Specific version of clang/llvm
RUN mkdir -p /gt/pkgs && \\
    cd /gt/pkgs && \\
    export PKGS=clang-4.0.1-5-x86_64.pkg.tar.xz clang-tools-extra-4.0.1-5-x86_64.pkg.tar.xz llvm-4.0.1-5-x86_64.pkg.tar.xz llvm-libs-4.0.1-5-x86_64.pkg.tar.xz && \\
    for PKG in $PKGS;do wget http://otsego.grammatech.com/u1/eschulte/share-ro/pkgs/$PKG; done && \\
    pacman --noconfirm -U $PKGS

# Enable makepkg as root
RUN sed -i "s/^\(OPT_LONG=(\)/\1'asroot' /;s/EUID == 0/1 == 0/" /usr/bin/makepkg

# Install required packages
RUN mkdir -p /gt/libtinfo && \
    git clone https://aur.archlinux.org/libtinfo.git /gt/libtinfo && \
    cd /gt/libtinfo && \
    makepkg --asroot --noconfirm -si

RUN mkdir -p /gt/clang-mutate/ && \
    git clone git@git.grammatech.com:synthesis/clang-mutate.git /gt/clang-mutate/ && \
    cd /gt/clang-mutate/ && \
    git checkout CI_COMMIT_SHA && \
    mkdir -p src/clang-mutate_pkg && \
    rsync --exclude .git --exclude src/clang-mutate_pkg -aruv ./ src/clang-mutate_pkg && \
    make -C src/clang-mutate_pkg clean && \
    makepkg --asroot --noconfirm -ef -si

WORKDIR /gt/clang-mutate

CMD /bin/bash
