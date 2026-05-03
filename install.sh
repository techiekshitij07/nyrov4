#!/bin/bash
# install.sh — Nyro v4 Final — Raspberry Pi 3B+ Installer
# Optimized specifically for Raspberry Pi 3B+ (32-bit OS / armv7l)
# 100% Offline Piper TTS (Priyamvada & Rohan) - NO gTTS

set -e
G='\033[0;32m'; Y='\033[1;33m'; B='\033[1;34m'; R='\033[0;31m'; N='\033[0m'
ok()   { echo -e "${G}[OK]${N} $1"; }
info() { echo -e "${B}[>>]${N} $1"; }
warn() { echo -e "${Y}[!!]${N} $1"; }

NYRO_DIR="$HOME/nyro_v4"
VENV="$NYRO_DIR/venv"
PIPER_DIR="$HOME/piper"
SRC="$(cd "$(dirname "$0")" && pwd)"

echo -e "\n${B}╔═══════════════════════════════╗"
echo -e "║  NYRO v4 — Pi 3B+ Installer   ║"
echo -e "╚═══════════════════════════════╝${N}\n"

# ── System packages ───────────────────────────────────────────
info "System packages install..."
sudo apt-get update -qq
sudo apt-get install -y -qq \
    python3 python3-pip python3-venv \
    portaudio19-dev mpg123 alsa-utils flac git wget \
    libatlas-base-dev libasound2-dev pigpio python3-pigpio \
    2>/dev/null
ok "System packages done"

# ── Enable SPI (MCP3008 gas sensor ke liye) ──────────────────
info "SPI enable check..."
if ! grep -q "^dtparam=spi=on" /boot/config.txt; then
    echo "dtparam=spi=on" | sudo tee -a /boot/config.txt
    warn "SPI enabled — reboot required after install"
fi
ok "SPI configured"

# ── Files copy ────────────────────────────────────────────────
mkdir -p "$NYRO_DIR"
[ "$SRC" != "$NYRO_DIR" ] && cp "$SRC"/*.py "$NYRO_DIR/" 2>/dev/null || true
ok "Files in $NYRO_DIR"

# ── Python venv ───────────────────────────────────────────────
info "Python venv..."
[ ! -d "$VENV" ] && python3 -m venv "$VENV" --system-site-packages
source "$VENV/bin/activate"
pip install --upgrade pip -q
ok "venv: $VENV"

# ── Python packages ───────────────────────────────────────────
info "Python packages (Optimized - No gTTS)..."
pip install -q \
    SpeechRecognition \
    pyaudio \
    google-generativeai \
    pyserial \
    RPi.GPIO \
    adafruit-circuitpython-dht \
    spidev
ok "Python packages done"

# ── Piper TTS (Optimized for 32-bit armv7l) ───────────────────
info "Piper TTS install (32-bit OS optimization)..."
PIPER_BIN="$PIPER_DIR/piper"

if [ ! -f "$PIPER_BIN" ]; then
    mkdir -p "$PIPER_DIR"
    
    # Direct 32-bit (armv7l) URL for Pi 3B+
    URL="https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_linux_armv7l.tar.gz"
    
    info "Piper binary download (~15MB) for armv7l..."
    if wget -q -O /tmp/piper.tar.gz "$URL"; then
        tar -xzf /tmp/piper.tar.gz -C "$PIPER_DIR" --strip-components=1
        chmod +x "$PIPER_BIN"
        rm -f /tmp/piper.tar.gz
        ok "Piper binary installed: $PIPER_BIN"
    else
        warn "Piper download failed! Check internet connection."
        exit 1
    fi
fi

# Download Priyamvada (Nyro Female Voice)
if [ ! -f "$PIPER_DIR/hi_IN-priyamvada-medium.onnx" ]; then
    info "Downloading Nyro Voice (Priyamvada - Female)..."
    BASE="https://huggingface.co/rhasspy/piper-voices/resolve/main/hi/hi_IN/priyamvada/medium"
    wget -q -O "$PIPER_DIR/hi_IN-priyamvada-medium.onnx"          "$BASE/hi_IN-priyamvada-medium.onnx"
    wget -q -O "$PIPER_DIR/hi_IN-priyamvada-medium.onnx.json"     "$BASE/hi_IN-priyamvada-medium.onnx.json"
    ok "Nyro voice ready"
fi

# Download Rohan (Bhai Male Voice)
if [ ! -f "$PIPER_DIR/hi_IN-rohan-medium.onnx" ]; then
    info "Downloading Bhai Voice (Rohan - Male)..."
    BASE="https://huggingface.co/rhasspy/piper-voices/resolve/main/hi/hi_IN/rohan/medium"
    wget -q -O "$PIPER_DIR/hi_IN-rohan-medium.onnx"          "$BASE/hi_IN-rohan-medium.onnx"
    wget -q -O "$PIPER_DIR/hi_IN-rohan-medium.onnx.json"     "$BASE/hi_IN-rohan-medium.onnx.json"
    ok "Bhai voice ready"
fi

# ── start_nyro.sh ─────────────────────────────────────────────
cat > "$NYRO_DIR/start_nyro.sh" << 'LAUNCH'
#!/bin/bash
# Nyro v4 Quick Start
cd "$HOME/nyro_v4"
source venv/bin/activate
echo ""
echo "  ╔═════════════════════════╗"
echo "  ║   NYRO v4 Starting...   ║"
echo "  ╚═════════════════════════╝"
echo ""
python3 main.py
LAUNCH
chmod +x "$NYRO_DIR/start_nyro.sh"
ok "start_nyro.sh ready"

# ── Systemd auto-start ────────────────────────────────────────
info "Systemd service (boot par auto-start)..."
sudo tee /etc/systemd/system/nyro.service > /dev/null << SVC
[Unit]
Description=Nyro v4 AI Robot
After=network.target sound.target pigpio.service

[Service]
Type=simple
User=$USER
WorkingDirectory=$NYRO_DIR
ExecStart=$VENV/bin/python3 $NYRO_DIR/main.py
Restart=on-failure
RestartSec=8
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
SVC

sudo systemctl daemon-reload
sudo systemctl enable nyro
ok "Systemd service enabled"

# ── Audio test ────────────────────────────────────────────────
if aplay -l 2>/dev/null | grep -q "card"; then
    ok "Audio device detected"
else
    warn "Audio device nahi mila — speaker check karo"
fi

echo ""
echo -e "${G}╔════════════════════════════════════╗"
echo -e "║   NYRO v4 INSTALL COMPLETE!        ║"
echo -e "╚════════════════════════════════════╝${N}"
echo ""
echo -e "  ${Y}bash ~/nyro_v4/start_nyro.sh${N}   ← chalao"
echo -e "  ${Y}sudo systemctl start nyro${N}       ← ya systemd se auto-run"
echo -e "  ${Y}journalctl -u nyro -f${N}           ← logs dekhne ke liye"
echo ""
```eof

Is update ke baad Raspberry Pi par installation ekdum makhan (smooth) chalegi, aur OS mismatch ka koi khatra nahi hoga! Terminal me jaakar `bash install.sh` run kijiye aur batayiye kaisa chal raha hai.
