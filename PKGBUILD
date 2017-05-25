# -*- shell-script -*-
# Maintainer: Eric Schulte <schulte.eric@gmail.com>
pkgname=clang-mutate-git
_srcdir=clang-mutate_pkg
pkgver=ase.2016.r137.g0a513f3
pkgrel=1
pkgdesc="Manipulate C-family ASTs with Clang"
url="https://github.com/eschulte/clang-mutate"
arch=('i686' 'x86_64')
license=('GPL3')
depends=('clang>=4.0.0')
makedepends=('git' 'pandoc' 'sed' 'gzip')
provides=('clang-mutate')
source=("${_srcdir}::git+https://git.grammatech.com/synthesis/clang-mutate")
sha256sums=('SKIP')

pkgver() {
  cd $_srcdir
  git describe --long | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}

# prepare() { }

build() {
  cd $_srcdir
  make
  make man
}

package() {
  install -d "$pkgdir"/usr/bin
  install -Dm755 $_srcdir/clang-mutate "$pkgdir"/usr/bin/clang-mutate

  # man pages
  install -dm755 $pkgdir/usr/share/man/man1
  install -m644 $_srcdir/man/*.1.gz $pkgdir/usr/share/man/man1

  # documentation
  install -dm755 $pkgdir/usr/share/doc/$_srcdir
  install -m644 $_srcdir/README $pkgdir/usr/share/doc/$_srcdir
}

# vim: ts=2 sw=2 et:
