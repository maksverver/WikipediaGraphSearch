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
    cmake -S "${startdir}" -B "${_build_dir}" -D CMAKE_BUILD_TYPE=None
    make -C "${_build_dir}" CPPFLAGS="${CPPFLAGS}" CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}"
}

check() {
    make -C "${_build_dir}" test
}

package() {
    local site_packages=$(python -c "import site; print(site.getsitepackages()[0])")

    cd "${_build_dir}/src"

    install -D -t "${pkgdir}/${site_packages}/" wikipath.*.so

    cd "${startdir}/python"

    install -D -t "${pkgdir}/usr/lib/wikipath" http_server.py inspect.py search.py

    cd "${_build_dir}/apps"

    install -D -t "${pkgdir}/usr/lib/wikipath" index inspect search websearch xml-stats

    cd "${startdir}"

    install -d "${pkgdir}/usr/share/wikipath"
    cp -r htdocs "${pkgdir}/usr/share/wikipath/"

    cd "${startdir}/systemd"

    install -D -t "${pkgdir}/usr/lib/wikipath/" websearch-wt.sh websearch-python.sh
    install -D -m644 websearch-wt.service "${pkgdir}/usr/lib/systemd/system/wikipath-websearch-wt.service"
    install -D -m644 websearch-python.service "${pkgdir}/usr/lib/systemd/system/wikipath-websearch-python.service"

    install -d "${pkgdir}/var/lib/wikipath"
}
