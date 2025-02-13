# syntax=docker/dockerfile:1.7-labs

# Example build command:
#
# % (sudo) docker buildx build -t wikipath . \
#   --build-arg graph_basename=fywiki-20240201-pages-articles \
#   --build-arg wiki_base_url=https://fy.wikipedia.org/wiki/
#
# graph_basename is the basename of the graph data files in the data/
# subdirectory. For example, if you want to use:
#
#   data/enwiki-20240220-pages-articles.graph
#   data/enwiki-20240220-pages-articles.metadata
#
# then pass: --build-arg graph_basename=enwiki-20240220-pages-articles
#
# wiki_base_url is optional, and defaults to the base URL of English wikipedia.
#
# The Python HTTP server is exposed on port 8080 by default. Example run command:
#
# % (sudo) docker run -p 8080:8080 wikipath
#
# To run on a different external port:
#
# % (sudo) docker run -p 8123:8080 wikipath
#
# To run on a different internal port:
#
# % (sudo) docker run -p 8080:80 -e PORT=80 wikipath
#
# To lock the graph into memory at startup (which increases startup time and
# persistant memory use, but should decrease path search times):
#
# % (sudo) docker run -p 8080:8080 --cap-add IPC_LOCK -e MLOCK=FOREGROUND wikipath
#
# Alternatively, MLOCK=BACKGROUND doesn't wait until all memory is locked to
# start serving requests, and MLOCK=POPULATE doesn't require the IPC_LOCK
# capability, though neither alternative guarantees that all pages are locked
# into memory.
#
# To upload to Google Cloud Registry so the new image can be deployed on
# Google Cloud Run:
#
#  sudo docker buildx build -t wikipath . --build-arg graph_basename=enwiki-20240220-pages-articles
#  sudo docker tag wikipath gcr.io/wikipedia-graph-search/wikipath
#  sudo docker push gcr.io/wikipedia-graph-search/wikipath
#

# Create an up-to-date Arch Linux package.
#
# Note that this will be cached. To clear the cache and generate a new and
# up-to-date image, first run: docker buildx prune --all
FROM archlinux:latest as archlinux-up-to-date
RUN pacman -Sy
RUN pacman -S archlinux-keyring --noconfirm && pacman-key --init && pacman-key --populate archlinux
RUN pacman -Su --noconfirm

# Create an image with all build dependencies installed. This is a separate
# image from `build` so that changes to the source code don't cause refetching
# packages from the internet, which is relatively slow/expensive, and may fail
# when the base image (archlinux-up-to-date) is outdated. When this fails, just
# clear the cache as described above.
FROM archlinux-up-to-date AS archlinux-build
RUN useradd build
RUN pacman -S base-devel --noconfirm
RUN --mount=type=bind,source=makepkg/PKGBUILD,target=PKGBUILD \
        bash -c 'source PKGBUILD; pacman -S --noconfirm "${makedepends[@]}"'

# Create an image with runtime dependencies installed. Again, this prevents
# refetching packages whenever the wikipath package itself changes.
FROM archlinux-up-to-date AS archlinux-run
RUN pacman -S python3 --noconfirm

# Build the app.
FROM archlinux-build AS build
COPY --exclude=data/ --chown=build . /build
WORKDIR /build/makepkg/
RUN su build -c 'makepkg -fc'

# Create the main app image.
FROM archlinux-run
WORKDIR /pkg
COPY --from=build /build/makepkg/wikipath-*.pkg* .
RUN pacman -U wikipath-*.pkg* --noconfirm
ARG graph_basename=NONEXISTENT
COPY --link "data/${graph_basename}.graph" "data/${graph_basename}.metadata" /var/lib/wikipath/
ARG wiki_base_url=https://en.wikipedia.org/wiki/
ENV WIKI_BASE_URL=${wiki_base_url}
ARG host=
ENV HOST=${host}
ARG port=8080
ENV PORT=${port}
ARG mlock=NONE
ENV MLOCK=${mlock}
EXPOSE ${PORT}
ENTRYPOINT /usr/lib/wikipath/http_server.py \
    -h "${HOST}" -p "${PORT}" -d /usr/share/wikipath/htdocs/ \
    --wiki_base_url "${WIKI_BASE_URL}" \
    --mlock "${MLOCK}" \
    /var/lib/wikipath/*.graph
