# sikradio

An internet radio client written in C++, communicating with a streaming server over TCP (IPv4/IPv6). The client receives an audio stream — optionally multiplexed with text metadata — and writes it to standard output for playback via an external tool such as `play` (SoX) or `mpv`.

```bash
./sikradio -u radio.example.com:8080 | mpv --really-quiet -
```

## Features

- **TCP-based streaming client** with custom application-level protocol
- **Dual-stack networking** — automatic IPv4/IPv6 selection based on `getaddrinfo` results, or explicit `-4` / `-6` override
- **Text metadata multiplexing** — optional interleaved text stream alongside audio (`-m`)
- **Configurable reconnection** — automatic reconnect on stream timeout, with a tunable threshold
- **Leveled diagnostic logging** — five verbosity tiers, from silent to full debug output
- **Clean shutdown semantics** — graceful termination on server disconnect or user command, with well-defined exit codes

## Usage

```
sikradio -u <url> [-m] [-t timeout] [-4 | -6] [-v verbosity | -q]
```

| Flag | Description | Default |
|------|-------------|---------|
| `-u url` | Server address / stream identifier (**required**) | — |
| `-m` | Request text metadata multiplexed with the audio stream | off |
| `-t timeout` | Reconnect threshold in ms, if no data arrives (100–100000) | 5000 |
| `-4` / `-6` | Force IPv4 / IPv6 | auto-detected |
| `-v verbosity` | Diagnostic output level, 0–4 | 2 |
| `-q` | Shorthand for `-v0` | — |

Flags can be given in any order, combined without a separating space (`-t3000`), and blocked together (`-m46`).

Typing `quit` at any point closes the connection cleanly and flushes any buffered data before exiting.

### Verbosity levels

| Level | Output |
|-------|--------|
| 0 | Silent |
| 1 | Server communication progress |
| 2 | Critical errors (default) |
| 3 | Non-critical errors |
| 4 | Full debug diagnostics |

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Clean shutdown (`quit` or server closed the connection) |
| `1` | Invalid arguments, or an unrecoverable runtime error |

## Building

```bash
make
```

Produces the `sikradio` executable in the project root.

```bash
make clean
```

Removes all build artifacts.

No external networking libraries are used beyond the standard BSD sockets interface — only OpenSSL (`libssl` / `libcrypto`) is linked where relevant.