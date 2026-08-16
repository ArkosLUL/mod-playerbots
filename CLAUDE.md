# CLAUDE.md

- **Never trust a `*.conf` value:** gitignored `configurationOverrides/*.env` at the server root override it via `AC_*` env vars. Read the effective value with `docker exec ac-worldserver env | grep ^AC_`.

- **Never invent a raid coordinate:** verify it with `navprobe` before shipping. Client data lives in the `ac-client-data` Docker volume — `env/dist/data` is empty on the host and that is not a reason to skip the check. See [docs/engine/pitfalls.md](docs/engine/pitfalls.md).
