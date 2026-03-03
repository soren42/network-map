# network-map

A fast, zero-dependency local network discovery and visualization tool written in ANSI C. Discovers all hosts on your LAN, enriches them with OS/service/manufacturer data via nmap, detects your NAT boundary, and produces multiple output formats including an interactive 3D WebVR visualization.

## Features

- **Automatic LAN discovery** via ARP cache, ICMP ping sweep, and IPv6 neighbor discovery
- **Service & OS detection** through nmap integration (XML output parsing)
- **NAT boundary detection** by tracing outward from the gateway to find the RFC 1918 boundary
- **mDNS/Bonjour browsing** for device names and service types
- **Smart host classification** into 7 types: local, gateway, server, workstation, printer, IoT, boundary
- **Kruskal MST + BFS** graph layout for clean topology visualization
- **6 output formats**: text table, JSON, PNG, MP4, interactive HTML/WebVR, ncurses tree
- **Cross-platform**: macOS (Darwin) and Linux with platform-specific networking backends

## Quick Start

```sh
git clone https://github.com/soren42/network-map.git
cd network-map
./configure
make
sudo ./bin/network-map
```

Root/sudo is required for raw ICMP sockets and nmap OS detection.

## Output Formats

| Format | Flag | Description | Requires |
|--------|------|-------------|----------|
| **Text** | `-o text` | Table + MST tree to stdout (default) | — |
| **JSON** | `-o json` | Machine-readable graph data | — |
| **PNG** | `-o png` | 2D radial topology map | cairo |
| **MP4** | `-o mp4` | Animated flythrough video | cairo, ffmpeg |
| **HTML** | `-o html` | Interactive 3D WebVR visualization | — |
| **Curses** | `-o curses` | Terminal tree view with colors | ncurses |

Combine multiple formats with commas:

```sh
sudo ./bin/network-map -o text,json,png,html
```

## Usage

```
Usage: network-map [OPTIONS] [BOUNDARY_HOST]

Options:
  -v                   Increase verbosity (-v to -vvvvv)
  -o, --output FMT     Output formats: text,json,curses,png,mp4,html
  -f, --file PATH      Output filename base (default: intranet)
  -4                   IPv4 only
  -6                   IPv6 only
  --no-mdns            Disable mDNS discovery
  --no-arp             Disable ARP cache reading
  --no-nmap            Disable nmap service scanning
  --fast               Fast mode (reduce timeouts, skip slow probes)
  -n, --nameserver IP  Use custom DNS server for reverse lookups
  --from-json FILE     Re-render outputs from a previously exported JSON file
  -h, --help           Show this help
  --version            Show version

Positional:
  BOUNDARY_HOST        Manually specify upstream NAT boundary IP
```

### Examples

```sh
# Default scan with text output
sudo ./bin/network-map

# Generate all visual outputs
sudo ./bin/network-map -o png,html,mp4 -f my-network

# Fast scan, skip nmap, IPv4 only
sudo ./bin/network-map --fast --no-nmap -4

# Use a specific DNS server and set boundary manually
sudo ./bin/network-map -n 10.0.0.253 10.0.0.1

# Verbose scan with JSON export
sudo ./bin/network-map -vvv -o text,json

# Re-render from a previously exported JSON (no sudo needed)
./bin/network-map --from-json intranet.json -o png,html -f re-rendered
```

## Discovery Pipeline

The scan runs in 7 phases:

1. **Local interfaces** — enumerate all network interfaces via `getifaddrs()`
2. **Routing table** — read system routes to identify gateways
3. **LAN discovery** — ARP cache + ICMP ping sweep across all local /24 subnets
4. **Name resolution** — reverse DNS + mDNS/Bonjour browsing
5. **Boundary detection** — traceroute outward to find the RFC 1918 → public IP transition
6. **nmap enrichment** — service version and OS detection via `nmap -sV -O`
7. **IPv6 augmentation** — ICMPv6 neighbor discovery with EUI-64 MAC matching

## Building

### Prerequisites

**Required:**
- C11-compatible compiler (gcc, clang)
- POSIX system (macOS or Linux)
- `make`

**Optional (for full output support):**

| Library | Output | Detection |
|---------|--------|-----------|
| [ncurses](https://invisible-island.net/ncurses/) | Curses tree view | `pkg-config` or header probe |
| [cairo](https://cairographics.org/) | PNG rendering | `pkg-config` |
| [libpng](http://www.libpng.org/) | PNG file writing | `pkg-config` |
| [FFmpeg](https://ffmpeg.org/) (libavformat, libavcodec, libswscale) | MP4 video | `pkg-config` |

**Runtime (optional):**
- [nmap](https://nmap.org/) — for service/OS detection (phase 6)

### macOS

```sh
# Install Xcode command line tools (provides clang, make)
xcode-select --install

# Install optional dependencies via Homebrew
brew install cairo libpng ffmpeg nmap

# Build
./configure
make
```

macOS provides mDNS (dns_sd) and ncurses natively — no extra packages needed.

### Ubuntu / Debian

```sh
# Install build tools and required headers
sudo apt update
sudo apt install build-essential pkg-config

# Install optional dependencies
sudo apt install libncurses-dev libcairo2-dev libpng-dev \
                 libavformat-dev libavcodec-dev libswscale-dev libavutil-dev \
                 libavahi-client-dev nmap

# Build
./configure
make
```

### Fedora / RHEL / Rocky

```sh
# Install build tools
sudo dnf install gcc make pkg-config

# Install optional dependencies
sudo dnf install ncurses-devel cairo-devel libpng-devel \
                 ffmpeg-free-devel avahi-devel nmap

# Build
./configure
make
```

### Arch Linux

```sh
sudo pacman -S base-devel cairo libpng ffmpeg avahi nmap

./configure
make
```

### Configure Options

```sh
./configure --help

Options:
  --prefix=DIR          Install prefix [/usr/local]
  --cc=COMPILER         C compiler [cc]
  --without-ncurses     Disable ncurses output
  --without-cairo       Disable PNG/MP4 output
  --without-ffmpeg      Disable MP4 output
  --without-dns-sd      Disable macOS mDNS
  --without-avahi       Disable Linux mDNS
  --without-libpng      Disable libpng
```

### Running Tests

```sh
make test
```

Runs the built-in test suite (147 tests across 7 suites: graph, host, layout, JSON, CLI, nmap, boundary).

## Host Classification

Hosts are automatically classified based on discovered services, mDNS names, and manufacturer data:

| Type | Color (VR) | Detection Criteria |
|------|-----------|-------------------|
| **Local** | Blue | The machine running the scan |
| **Gateway** | Amber | Found in routing table as default route |
| **Server** | Green | Runs HTTP, SSH, DNS, SMTP, or other server ports |
| **Workstation** | Gray | Default — no distinguishing services |
| **Printer** | Purple | IPP (631), LPD (515), RAW (9100), or mDNS name contains printer brands |
| **IoT** | Cyan | MQTT, mDNS-only, Espressif/Shelly/Tuya manufacturer |
| **Boundary** | Red | Last private-IP hop before the public internet |

## Project Structure

```
network-map/
├── configure           # Build system configuration script
├── config.h.in         # Config header template
├── Makefile            # GNU Make build rules
├── src/
│   ├── main.c          # Entry point
│   ├── cli.c/h         # Command-line argument parsing
│   ├── log.c/h         # Logging (6 levels: ERROR → PACKET)
│   ├── core/
│   │   ├── host.c/h    # Host data model (7 types, services, classification)
│   │   ├── edge.c/h    # Edge types (LAN, ROUTE, GATEWAY)
│   │   ├── graph.c/h   # Graph with adjacency lists, Kruskal MST, BFS
│   │   ├── scan.c/h    # 7-phase discovery orchestrator
│   │   └── json_out.c/h # cJSON serialization
│   ├── net/
│   │   ├── iface.c/h       # Interface enumeration (getifaddrs)
│   │   ├── arp_darwin.c     # ARP cache (macOS sysctl)
│   │   ├── arp_linux.c      # ARP cache (Linux /proc/net/arp)
│   │   ├── icmp_darwin.c    # ICMP ping/probe (macOS raw sockets)
│   │   ├── icmp_linux.c     # ICMP ping/probe (Linux raw sockets)
│   │   ├── route_darwin.c   # Routing table (macOS sysctl)
│   │   ├── route_linux.c    # Routing table (Linux /proc/net/route)
│   │   ├── mdns_darwin.c    # mDNS (macOS dns_sd.h)
│   │   ├── mdns_linux.c     # mDNS (Linux Avahi)
│   │   ├── dns.c/h          # Reverse DNS lookups
│   │   ├── ping.c/h         # Subnet ping sweep
│   │   ├── icmp6.c/h        # IPv6 neighbor discovery + EUI-64 MAC matching
│   │   ├── boundary.c/h     # NAT boundary detection
│   │   └── nmap.c/h         # nmap XML output parsing
│   ├── output/
│   │   ├── layout.c/h       # Radial 2D, 3D, force-directed layout
│   │   ├── out_text.c/h     # Text table + MST tree
│   │   ├── out_json.c/h     # JSON file output
│   │   ├── out_html.c/h     # Self-contained HTML + Three.js WebVR
│   │   ├── out_png.c/h      # Cairo 2D rendering
│   │   ├── out_mp4.c/h      # FFmpeg video encoding
│   │   ├── out_curses.c/h   # ncurses terminal tree
│   │   └── threejs_template.h # Embedded Three.js VR visualization
│   └── util/
│       ├── platform.h       # Platform abstraction macros
│       ├── alloc.c/h        # Checked allocation wrappers
│       └── strutil.c/h      # Safe string operations
├── tests/
│   ├── test_main.c          # Test runner
│   ├── test_host.c          # Host classification tests
│   ├── test_graph.c         # Graph/MST/BFS tests
│   ├── test_layout.c        # Layout algorithm tests
│   ├── test_json.c          # JSON serialization tests
│   ├── test_cli.c           # CLI parsing tests
│   ├── test_nmap.c          # nmap integration tests
│   ├── test_boundary.c      # Boundary detection tests
│   └── mock_net.c/h         # Mock graph builder for tests
└── vendor/
    └── cJSON/               # Vendored cJSON library
```

## License

[MIT](LICENSE)
