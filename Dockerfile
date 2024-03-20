# syntax=docker/dockerfile:1

# Example build command:
#
# % (sudo) docker buildx build -t wikipath . \
#   --build-arg graph_basename=fywiki-20240201-pages-articles \
#   --build-arg wiki_base_url=https://fy.wikipedia.org/wiki/
#
# graph_basename must be set to the basename of the graph data files to be
# copied, which must exist in the current subdirectory tree. For example, if
# graph_basename=foo/bar, then foo/bar.graph and foo/bar.metadata are copied
# into the image.
#
# wiki_base_url is optional, and defaults to the base URL of English wikipedia.
#
# The Python HTTP server is exposed on port 80. Example run command:
#
# % (sudo) docker run -p 80:80 wikipath
#

# Build the Arch Linux package.
FROM archlinux:latest AS build
RUN pacman -Sy base-devel --noconfirm
COPY . /build
RUN useradd build
WORKDIR /build/makepkg/
RUN bash -c 'source PKGBUILD; pacman -S --noconfirm "${makedepends[@]}"'
RUN chown build .
RUN su build -c 'makepkg -fc'

# Create the main app image.
FROM archlinux:latest
RUN pacman -Sy python3 --noconfirm
WORKDIR /pkg
COPY --from=build /build/makepkg/wikipath-*.pkg* .
RUN pacman -U wikipath-*.pkg* --noconfirm
ARG graph_basename=NONEXISTENT
COPY --link "${graph_basename}".graph "${graph_basename}".metadata /var/lib/wikipath/
ARG wiki_base_url=https://en.wikipedia.org/wiki/
ENTRYPOINT /usr/lib/wikipath/http_server.py \
    -h '' -p 80 -d /usr/share/wikipath/htdocs/ \
    --wiki_base_url "${wiki_base_url}" \
    /var/lib/wikipath/*.graph
