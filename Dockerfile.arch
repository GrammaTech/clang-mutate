FROM base/archlinux

# Echo the commit hash to ensure we re-run to grab the latest packages.
RUN pacman --noconfirm -Syu archlinux-keyring

# Requirements to build clang-mutate.
RUN pacman --noconfirm -Syu base-devel findutils openssh git sed wget rsync gzip pandoc

# Requirements to test clang-mutate.
RUN pacman --noconfirm -Syu jshon libxcb

ENV PATH=$PATH:/usr/synth/bin/ \
    LD_LIBRARY_PATH=/usr/synth/lib/

# Enable makepkg as root.
RUN sed -i "s/^\(OPT_LONG=(\)/\1'asroot' /;s/EUID == 0/1 == 0/" /usr/bin/makepkg

# Install wdiff which is required by the clang tests.
RUN mkdir -p /gt/wdiff && \
    git clone https://aur.archlinux.org/wdiff.git /gt/wdiff && \
    cd /gt/wdiff && \
    makepkg --asroot --noconfirm -si

# Install bear which is used to generate clang compilation databases.
RUN mkdir -p /gt/bear-git && \
    git clone https://aur.archlinux.org/bear-git.git /gt/bear-git && \
    cd /gt/bear-git && \
    makepkg --asroot --noconfirm -si

# Install specific version (6.0.1) of clang/llvm.
RUN mkdir -p /gt/pkgs && \
    cd /gt/pkgs && \
    export PKGS="c/clang/clang-6.0.1-2-x86_64.pkg.tar.xz l/llvm/llvm-6.0.1-4-x86_64.pkg.tar.xz l/llvm-libs/llvm-libs-6.0.1-4-x86_64.pkg.tar.xz" && \
    echo $PKGS|tr ' ' '\n'|xargs -I{} wget https://archive.archlinux.org/packages/{} && \
    pacman --noconfirm -U $(echo $PKGS|sed 's| [^ ]*/| |g;s|^.*/||')

# Don't upgrade clang/llvm packages.
RUN sed -i '/^#IgnorePkg/ a IgnorePkg = llvm llvm-libs clang clang-tools-extra' /etc/pacman.conf

# Copy in clang-mutate directory.
ARG GT
COPY . /gt/clang-mutate

# Build clang-mutate package and install.
WORKDIR /gt/clang-mutate
RUN mkdir -p src/clang-mutate_pkg
RUN rsync --exclude .git --exclude src/clang-mutate_pkg -aruv ./ src/clang-mutate_pkg
RUN make -C src/clang-mutate_pkg clean
RUN makepkg --asroot --noconfirm -ef -si
