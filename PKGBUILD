# -*- shell-script -*-
# Maintainer: Eric Schulte <schulte.eric@gmail.com>
pkgname=clang-mutate-git
_gitname=clang-mutate
pkgver=ase.2016.r124.g823e76a
pkgrel=1
pkgdesc="Manipulate C-family ASTs with Clang"
url="https://github.com/eschulte/clang-mutate"
arch=('i686' 'x86_64')
license=('GPL3')
depends=('clang>=4.0.0')
makedepends=('git' 'pandoc' 'sed' 'gzip')
provides=('clang-mutate')
source=('clang-mutate::git+https://git.grammatech.com/synthesis/clang-mutate')
sha256sums=('SKIP')

pkgver() {
  cd $_gitname
  git describe --long | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}

# prepare() { }

build() {
  cd $_gitname
  make
  make man
}

package() {
  install -d "$pkgdir"/usr/bin
  install -Dm755 $_gitname/clang-mutate "$pkgdir"/usr/bin/clang-mutate

  # man pages
  install -dm755 $pkgdir/usr/share/man/man1
  install -m644 $_gitname/man/*.1.gz $pkgdir/usr/share/man/man1

  # documentation
  install -dm755 $pkgdir/usr/share/doc/$_gitname
  install -m644 $_gitname/README $pkgdir/usr/share/doc/$_gitname
}

# vim: ts=2 sw=2 et:
