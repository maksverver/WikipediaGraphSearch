#!/bin/sh

set -e -E -o pipefail

host=127.0.0.1
port=8001
latest_graph=$(ls /var/lib/wikipath/enwiki-*.graph | tail -1)
wiki_base_url=https://en.wikipedia.org/wiki/
docroot=/usr/share/wikipath/htdocs/

exec /usr/lib/wikipath/http_server.py "${latest_graph}" \
    --host="${host}" \
    --port="${port}" \
    --wiki_base_url="${wiki_base_url}" \
    --docroot="${docroot}"
