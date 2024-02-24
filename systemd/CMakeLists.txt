install(PROGRAMS websearch-python.sh websearch-wt.sh DESTINATION lib/wikipath/)
install(FILES websearch-wt.service DESTINATION lib/systemd/system/ RENAME wikipath-websearch-wt.service)
install(FILES websearch-python.service DESTINATION lib/systemd/system/ RENAME wikipath-websearch-python.service)
