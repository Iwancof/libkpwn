savedcmd_vuln.mod := printf '%s\n'   vuln.o | awk '!x[$$0]++ { print("./"$$0) }' > vuln.mod
