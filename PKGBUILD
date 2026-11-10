# Maintainer: SimonShiki
pkgname=subtly
pkgver=${SUBTLY_VERSION:-$(sed -n 's/^set_version("\([^"]*\)").*/\1/p' "$startdir/xmake.lua")}
pkgrel=1
pkgdesc='On-device live captions for Linux Wayland'
arch=('x86_64')
url='https://github.com/SimonShiki/subtly'
license=('MIT')
depends=('gcc-libs' 'glibc' 'layer-shell-qt' 'pipewire' 'qt6-base' 'qt6-declarative' 'qt6-svg' 'qt6-wayland')
makedepends=('git' 'unzip' 'xmake' 'patchelf')
optdepends=('noto-fonts-cjk: CJK caption font')
conflicts=('subtly-git' 'subtly-nemotron')
if [[ ${SUBTLY_NEMOTRON:-n} == y ]]; then
  pkgname=subtly-nemotron
  provides=('subtly')
  conflicts=('subtly-git' 'subtly')
fi
# The prebuilt ASR libraries must keep the RPATH set by stage.sh.
options=('!strip' '!debug')
source=()

build() {
  cd "$startdir"
  xmake f -y -m release --nemotron="${SUBTLY_NEMOTRON:-n}"
  xmake build subtly
}

package() {
  cd "$startdir"
  bash packaging/stage.sh build/linux/x86_64/release/subtly "$pkgdir"
}
