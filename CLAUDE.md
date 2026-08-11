# CLAUDE.md

- **Never trust a `*.conf` value:** gitignored `configurationOverrides/*.env` at the server root override it via `AC_*` env vars. Read the effective value with `docker exec ac-worldserver env | grep ^AC_`.
