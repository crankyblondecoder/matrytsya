# Notes on external libraries

Dynamic hives use the embedded key/value store LMDB: https://symas.com/lmdb.php

General-purpose JSON parsing and serialization uses the header-only RapidJSON library: https://rapidjson.org/


HTTPS connections to model providers (e.g. OpenRouter) use OpenSSL, which Mongoose is built against (`-DMG_TLS=MG_TLS_OPENSSL`). Executables link `-lssl -lcrypto`. Servers are verified against the system CA bundle at `/etc/ssl/certs/ca-certificates.crt`: https://www.openssl.org/
