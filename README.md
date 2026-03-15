# network-map

A fast, zero-dependency local network discovery and visualization tool written in ANSI C. Discovers all hosts on your LAN via IP probing, LLDP, and the Ubiquiti UniFi controller API. Enriches with OS/service/manufacturer data via nmap, maps physical switch-port topology, and produces multiple output formats including an interactive 3D WebVR visualization.

## Features

- **L2 physical topology** via LLDP (`lldpcli`) and Ubiquiti UniFi controller API
- **Automatic LAN discovery** via ARP cache, ICMP ping sweep, and IPv6 neighbor discovery
- **Service & OS detection** through nmap integration (XML output parsing)
- **NAT boundary detection** by tracing outward from the gateway to find the RFC 1918 boundary
- **mDNS/Bonjour browsing** for device names and service types
- **Smart host classification** into 9 types: local, gateway, server, workstation, printer, IoT, boundary, switch, AP
- **Connection medium tracking**: wired, WiFi, MoCA — with switch port, VLAN, SSID, and signal data
- **Kruskal MST + BFS** graph layout with L2-preference weighting for physical topology
- **6 output formats**: text table, JSON, PNG, MP4, interactive HTML/WebVR, ncurses tree
- **Configuration file** support: `/etc/netmap.conf`, `~/.config/netmap/netmap.conf`, environment variables
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
  -i, --interface IF   Only use specified interface(s) (repeatable)
  -4                   IPv4 only
  -6                   IPv6 only
  --no-mdns            Disable mDNS discovery
  --no-arp             Disable ARP cache reading
  --no-nmap            Disable nmap service scanning
  --no-lldp            Disable LLDP discovery
  --no-unifi           Disable UniFi API discovery
  --unifi-host HOST    UniFi controller hostname/IP
  --unifi-user USER    UniFi controller username
  --unifi-pass PASS    UniFi controller password
  --unifi-site SITE    UniFi site name (default: "default")
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

# Scan with UniFi controller integration
sudo ./bin/network-map --unifi-host 192.168.1.1 \
  --unifi-user admin --unifi-pass secret

# Scan only specific interface(s) on a multi-homed machine
sudo ./bin/network-map -i en0
sudo ./bin/network-map -i eth0 -i eth1

# Skip L2 discovery, IP-layer only
sudo ./bin/network-map --no-lldp --no-unifi
```

## Discovery Pipeline

The scan runs in 9 phases:

1. **Local interfaces** — enumerate all network interfaces via `getifaddrs()`
2. **Routing table** — read system routes to identify gateways
3. **LAN discovery** — ARP cache + ICMP ping sweep across all local /24 subnets
4. **Name resolution** — reverse DNS + mDNS/Bonjour browsing
5. **Boundary detection** — traceroute outward to find the RFC 1918 → public IP transition
6. **nmap enrichment** — service version and OS detection via `nmap -sV -O`
7. **IPv6 augmentation** — ICMPv6 neighbor discovery with EUI-64 MAC matching
8. **LLDP discovery** — query `lldpcli` for switch/AP neighbors with port-level data
9. **UniFi API** — query the Ubiquiti controller for devices, clients, and WiFi associations

L2 phases (8–9) run last so all IP-discovered hosts are already in the graph for MAC-based matching and merging.

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
- [lldpd](https://lldpd.github.io/) — provides `lldpcli` for LLDP neighbor discovery (phase 8)
- [curl](https://curl.se/) — for UniFi controller API integration (phase 9)

### macOS

```sh
# Install Xcode command line tools (provides clang, make)
xcode-select --install

# Install optional dependencies via Homebrew
brew install cairo libpng ffmpeg nmap lldpd curl

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
                 libavahi-client-dev nmap lldpd curl

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
                 ffmpeg-free-devel avahi-devel nmap lldpd curl

# Build
./configure
make
```

### Arch Linux

```sh
sudo pacman -S base-devel cairo libpng ffmpeg avahi nmap lldpd curl

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
  --without-lldp        Disable LLDP discovery
  --without-curl        Disable curl (UniFi API)
  --with-lldp           Enable LLDP discovery
  --with-curl           Enable curl (UniFi API)
```

### Running Tests

```sh
make test
```

Runs the built-in test suite (299 tests across 11 suites: graph, host, layout, JSON, CLI, nmap, boundary, conffile, LLDP, UniFi).

## Host Classification

Hosts are automatically classified based on discovered services, mDNS names, manufacturer data, and L2 discovery:

| Type | Color (VR) | Detection Criteria |
|------|-----------|-------------------|
| **Local** | Green | The machine running the scan |
| **Gateway** | Amber | Found in routing table as default route, or UniFi UDM/UDR/USG |
| **Switch** | Teal | LLDP "Bridge" capability, or UniFi USW device |
| **AP** | Light Green | LLDP "WLAN" capability, or UniFi UAP/U6/U7 device |
| **Server** | Blue | Runs HTTP, SSH, DNS, SMTP, or other server ports |
| **Workstation** | Light Blue | Default — no distinguishing services |
| **Printer** | Orange | IPP (631), LPD (515), RAW (9100), or mDNS name contains printer brands |
| **IoT** | Purple | MQTT, mDNS-only, Espressif/Shelly/Tuya manufacturer |
| **Boundary** | Red | Last private-IP hop before the public internet |

## Configuration

UniFi credentials and other settings can be provided via CLI flags, environment variables, or a configuration file. Sources are applied in order — later sources override earlier ones:

1. `/etc/netmap.conf` (system-wide)
2. `~/.config/netmap/netmap.conf` (per-user)
3. Environment variables: `NETMAP_UNIFI_HOST`, `NETMAP_UNIFI_USER`, `NETMAP_UNIFI_PASS`, `NETMAP_UNIFI_SITE`
4. CLI flags: `--unifi-host`, `--unifi-user`, `--unifi-pass`, `--unifi-site`

See `netmap.conf.example` for the config file format.

### LLDP Setup

LLDP discovery requires `lldpd` running on the scanning machine:

```sh
# macOS
brew install lldpd
sudo brew services start lldpd

# Ubuntu/Debian
sudo apt install lldpd
sudo systemctl enable --now lldpd

# Verify it works
sudo lldpcli show neighbors
```

Your managed switches must also have LLDP enabled (most Ubiquiti, Cisco, and enterprise switches do by default).

### UniFi Setup

To enable UniFi controller integration, create a config file:

```sh
mkdir -p ~/.config/netmap
cat > ~/.config/netmap/netmap.conf << 'EOF'
[unifi]
host = 192.168.1.1
user = admin
pass = your-password
site = default
EOF
chmod 600 ~/.config/netmap/netmap.conf
```

Or pass credentials via CLI:

```sh
sudo ./bin/network-map --unifi-host 192.168.1.1 --unifi-user admin --unifi-pass secret
```

The tool connects to `https://<host>/api/auth/login` and queries the device and client stats APIs. Self-signed certificates are accepted (`curl -k`).

## Project Structure

```
network-map/
├── configure              # Build system configuration script
├── Makefile               # GNU Make build rules
├── netmap.conf.example    # Example configuration file
├── src/
│   ├── main.c             # Entry point
│   ├── cli.c/h            # Command-line argument parsing
│   ├── log.c/h            # Logging (6 levels: ERROR → PACKET)
│   ├── core/
│   │   ├── types.c/h      # Shared enums (nm_medium_t)
│   │   ├── host.c/h       # Host data model (9 types, services, L2 fields)
│   │   ├── edge.c/h       # Edge types (LAN, ROUTE, GATEWAY, L2, WIFI)
│   │   ├── graph.c/h      # Graph with adjacency lists, Kruskal MST, BFS
│   │   ├── scan.c/h       # 9-phase discovery orchestrator
│   │   └── json_out.c/h   # cJSON serialization (format v1.1)
│   ├── net/
│   │   ├── iface.c/h      # Interface enumeration (getifaddrs)
│   │   ├── arp_darwin.c    # ARP cache (macOS sysctl)
│   │   ├── arp_linux.c     # ARP cache (Linux /proc/net/arp)
│   │   ├── icmp_darwin.c   # ICMP ping/probe (macOS raw sockets)
│   │   ├── icmp_linux.c    # ICMP ping/probe (Linux raw sockets)
│   │   ├── route_darwin.c  # Routing table (macOS sysctl)
│   │   ├── route_linux.c   # Routing table (Linux /proc/net/route)
│   │   ├── mdns_darwin.c   # mDNS (macOS dns_sd.h)
│   │   ├── mdns_linux.c    # mDNS (Linux Avahi)
│   │   ├── dns.c/h         # Reverse DNS lookups
│   │   ├── ping.c/h        # Subnet ping sweep
│   │   ├── icmp6.c/h       # IPv6 neighbor discovery + EUI-64 MAC matching
│   │   ├── boundary.c/h    # NAT boundary detection
│   │   ├── nmap.c/h        # nmap XML output parsing
│   │   ├── lldp.c/h        # LLDP neighbor discovery via lldpcli
│   │   └── unifi.c/h       # Ubiquiti UniFi controller REST API
│   ├── output/
│   │   ├── layout.c/h      # Radial 2D, 3D, force-directed layout
│   │   ├── out_text.c/h    # Text table + physical topology tree
│   │   ├── out_json.c/h    # JSON file output
│   │   ├── out_html.c/h    # Self-contained HTML + Three.js WebVR
│   │   ├── out_png.c/h     # Cairo 2D rendering (L2/WiFi edge colors)
│   │   ├── out_mp4.c/h     # FFmpeg video encoding
│   │   ├── out_curses.c/h  # ncurses terminal tree
│   │   └── threejs_template.h # Embedded Three.js VR visualization
│   └── util/
│       ├── platform.h      # Platform abstraction macros
│       ├── alloc.c/h       # Checked allocation wrappers
│       ├── strutil.c/h     # Safe string operations + nm_read_all_fp
│       └── conffile.c/h    # INI config file parser
├── tests/
│   ├── test_main.c         # Test runner (11 suites)
│   ├── test_host.c         # Host classification + medium tests
│   ├── test_graph.c        # Graph/MST/BFS tests
│   ├── test_layout.c       # Layout algorithm tests
│   ├── test_json.c         # JSON serialization + L2 round-trip tests
│   ├── test_cli.c          # CLI parsing tests
│   ├── test_nmap.c         # nmap integration tests
│   ├── test_boundary.c     # Boundary detection tests
│   ├── test_conffile.c     # Config file parser tests
│   ├── test_lldp.c         # LLDP JSON parsing + graph integration tests
│   ├── test_unifi.c        # UniFi JSON parsing + graph integration tests
│   └── mock_net.c/h        # Mock graph builder for tests
└── vendor/
    └── cJSON/              # Vendored cJSON library
```

## License

[MIT](LICENSE)
