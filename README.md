# GENISYS

A generic AI voice-command interchange system for Raspberry Pi 5 with Hailo-10H NPU.
GENISYS listens to voice input, matches commands against a configured studio layout, and routes actions to connected devices over OSC.

Three components, three roles:

| Binary | Where it runs | What it does |
|---|---|---|
| `GenisysBackend` | RPi5 (headless) | Voice capture → VAD → Whisper/Hailo → command routing |
| `GenisysFrontend` | Any desktop / Pi display | Studio Builder wizard, sends config to backend |
| `GenisysDebugConsole` | Any display | Live transcript log, WER measurement, signal watchdog |

OSC ports: backend `54280`, frontend `54281`, diagnostics `54282`.

---

## Building for RPi5 (cross-compile from x86_64 Ubuntu)

### 1. Configure apt sources

arm64 packages live on a different Ubuntu mirror (`ports.ubuntu.com`) from the standard x86_64 mirror (`archive.ubuntu.com`). The two architectures need to be pinned to their respective mirrors, otherwise `apt` tries to fetch arm64 packages from the wrong server.

#### Ubuntu 24.04 Noble and newer — DEB-822 format (`.sources` files)

Ubuntu 24.04 manages sources in `/etc/apt/sources.list.d/ubuntu.sources` using the DEB-822 format, where each stanza has an explicit `Architectures:` field.

**Register arm64 as a foreign architecture:**
```bash
sudo dpkg --add-architecture arm64
```

**Pin the existing Ubuntu sources to amd64.**
Edit `/etc/apt/sources.list.d/ubuntu.sources` and ensure every stanza has `Architectures: amd64`:

```
Types: deb
URIs: http://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu/
Suites: noble-security
Components: main universe restricted multiverse
Architectures: amd64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

**Create the arm64 ports source file.**
Create `/etc/apt/sources.list.d/ubuntu-arm64-ports.sources`:

```
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports
Suites: noble-security
Components: main universe restricted multiverse
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

#### Ubuntu 22.04 or machines upgraded from 22.04 — legacy one-line format

<!-- TODO: document inline [arch=amd64]/[arch=arm64] syntax from native upgraded machine -->

---

### 2. Install cross toolchain and target libraries

```bash
sudo apt update
sudo apt install cmake ninja-build gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

sudo apt install \
    libasound2-dev:arm64 \
    libfreetype-dev:arm64 libfontconfig1-dev:arm64 \
    libx11-dev:arm64 libxrandr-dev:arm64 libxinerama-dev:arm64 \
    libxcursor-dev:arm64 libxext-dev:arm64
```

### 3. Build

```bash
cd ADCJapan26/GENISYS
./build.sh            # default: GENISYS_HAS_HAILO=ON
./build.sh --no-hailo # OSC + UI only, no HailoRT SDK required
```

Binaries are collected into `build/dist/`.

### 4. Deploy to RPi5

```bash
python deploy.py --target-ip 192.168.1.100
```

---

## Running

On the Pi, in separate terminals:

```bash
# Backend (always headless; --diag forwards transcripts to your workstation)
~/genisys/GenisysBackend [--diag <workstation-ip>]

# Frontend — Studio Builder wizard (Pi touchscreen or any connected display)
~/genisys/GenisysFrontend [--backend 127.0.0.1]

# Debug Console — live WER / transcript monitor (Pi or workstation)
~/genisys/GenisysDebugConsole
```

The Debug Console signal LED turns green the moment the first diagnostic OSC packet arrives from the backend.
