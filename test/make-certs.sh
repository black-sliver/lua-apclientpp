#!/usr/bin/env sh

# Helper script to generate self-signed certificates for testing.

if [ "$(basename "$(pwd)")" = "test" ]; then
    dest=".."  # if running from test/, put into parent
else
    dest="."
fi

openssl req -x509 -newkey ed25519 -days 1 \
  -noenc -keyout "${dest}/trusted-key.pem" -out "${dest}/trusted.pem" -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

openssl req -x509 -newkey ed25519 -days 1 \
  -noenc -keyout "${dest}/untrusted-key.pem" -out "${dest}/untrusted.pem" -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
