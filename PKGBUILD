pkgname=wikipath
pkgver=0.2
pkgrel=1
pkgdesc="Wikipedia shortest path search tool"
arch=('x86_64')
url="https://github.com/maksverver/WikipediaGraphSearch"
depends=('sqlite' 'libxml2')
optdepends=(
    'python: Python bindings and Python HTTP server'
    'wt: Wt HTTP server'
)
makedepends=('cmake' 'python' 'pybind11' 'wt')

_build_dir=${startdir}/build-package

build() {
    # https://wiki.archlinux.org/title/CMake_package_guidelines
    cmake -S "${startdir}" -B "${_build_dir}" -D CMAKE_BUILD_TYPE=None -D CMAKE_INSTALL_PREFIX=/usr
    make -C "${_build_dir}" CPPFLAGS="${CPPFLAGS}" CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}"
}

check() {
    make -C "${_build_dir}" test
}

package() {
    make -C "${_build_dir}" DESTDIR="${pkgdir}" install

    # Directory for user-installed Wikipedia graphs:
    install -d "${pkgdir}/var/lib/wikipath"
}
