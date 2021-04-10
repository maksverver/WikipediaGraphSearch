pkgname=wikipath
pkgver=0.1
pkgrel=2
pkgdesc="Wikipedia shortest path search tool"
arch=('x86_64')
url="https://github.com/maksverver/WikipediaGraphSearch"
depends=('sqlite' 'libxml2' 'wt=4.5.0')

build() {
    cd "${startdir}"

    make ${MAKEFLAGS} CPPFLAGS="${CPPFLAGS}" CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS}"
}

check() {
    cd "${startdir}"

    make  test
}

package() {
    cd "${startdir}"

    # Binaries are installed in /usr/lib/wikipath
    install -d "${pkgdir}/usr/lib/wikipath"
    install index inspect search websearch xml-stats "${pkgdir}/usr/lib/wikipath"

    # Graph data should be created (by the user) in /var/lib/wikipath
    install -d "${pkgdir}/var/lib/wikipath"

    # Install systemd service
    # Requires a symlink at /var/lib/wikipath/SERVING pointing to a .graph file.
    install -d "${pkgdir}/usr/lib/systemd/system"
    install websearch.sh "${pkgdir}/usr/lib/wikipath"
    install -m644 websearch.service "${pkgdir}/usr/lib/systemd/system/wikipath-websearch.service"
}
