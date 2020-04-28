#!/bin/sh

set -e -E -o pipefail

http_address=127.0.0.1
http_port=8001
latest_graph=$(ls /var/lib/wikipath/enwiki-*.graph | tail -1)

exec /usr/lib/wikipath/websearch "${latest_graph}" --docroot /usr/share/Wt --http-address ${http_address}  --http-port ${http_port}
