#!/usr/bin/env bash
exec /usr/bin/ssh -N \
  -o ExitOnForwardFailure=yes \
  -o ServerAliveInterval=30 \
  -o ServerAliveCountMax=3 \
  -o UserKnownHostsFile=/opt/sekailink/nexus/tunnel/.ssh/known_hosts \
  -i /opt/sekailink/nexus/tunnel/id_ed25519 \
  -L 127.0.0.1:19197:127.0.0.1:19097 \
  debian@167.114.3.84
