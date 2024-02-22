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
    cmake -S "${startdir}" -B "${_build_dir}" -D CMAKE_BUILD_TYPE=Release
    make -C "${_build_dir}" VERBOSE=1 CPPFLAGS="${CPPFLAGS}" CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}"
}

check() {
    make -C "${_build_dir}" test
}

package() {
    # Graph data should be created (by the user) in /var/lib/wikipath
    install -d "${pkgdir}/var/lib/wikipath"

    # Binaries and Python scripts are installed un /usr/lib/wikipath
    install -d "${pkgdir}/usr/lib/wikipath"

    # Python bindings are installed under /usr/lib/python3.XX/site-packages
    local site_packages=$(python -c "import site; print(site.getsitepackages()[0])")
    install -d "${pkgdir}/${site_packages}"

    cd "${_build_dir}/src"

    install -s wikipath.cpython-*.so "${pkgdir}/${site_packages}"

    cd "${startdir}/python"

    install http_server.py inspect.py search.py "${pkgdir}/usr/lib/wikipath"

    cd "${_build_dir}/apps"

    install -s index inspect search websearch xml-stats "${pkgdir}/usr/lib/wikipath"

    cd "${startdir}"

    # Install systemd service
    install -d "${pkgdir}/usr/lib/systemd/system"
    install "websearch.sh" "${pkgdir}/usr/lib/wikipath/"
    install -m644 "websearch.service" "${pkgdir}/usr/lib/systemd/system/wikipath-websearch.service"
}
