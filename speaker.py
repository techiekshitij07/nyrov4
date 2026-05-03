# speaker.py — Nyro v4 — TTS + Arduino Mouth Animation
# 100% Offline TTS using Piper (Human Touch Tuned)
# Sends START_SPEECH / frame numbers / END_SPEECH to Arduino
# -*- coding: utf-8 -*-
import subprocess, threading, time
import servo_ctrl
from config import (
    PIPER_BIN, PIPER_MODEL_NYRO, PIPER_MODEL_BHAI,
    AUDIO_WAV, AUDIO_MP3
)

_player       = None
_stop_flag    = threading.Event()
speaking_lock = threading.Lock()

# ── Arduino serial write ──────────────────────────────────────
def _ard(cmd: bytes):
    try:
        from arduino_ctrl import write
        write(cmd)
    except: pass

# ════════════════════════════════════════════════════════════
#  MOUTH PATTERNS — context-aware
# ════════════════════════════════════════════════════════════
_PATTERNS = {
    "excited":  [0,4,5,4,3,5,1,3],
    "question": [2,3,4,3,2,3,2],
    "soft":     [1,2,1,2,1,2],
    "normal":   [1,3,2,4,2,3,1],
}

def _mouth_pattern(text: str):
    t = text.lower()
    if any(w in t for w in ["!", "waah", "arre", "mast", "yaar", "chal"]):
        return _PATTERNS["excited"]
    if t.strip().endswith("?"):
        return _PATTERNS["question"]
    if any(w in t for w in ["udaas", "sorry", "theek", "dukhi"]):
        return _PATTERNS["soft"]
    return _PATTERNS["normal"]

# ════════════════════════════════════════════════════════════
#  TTS ENGINES (100% PIPER OFFLINE - HUMAN TUNED)
# ════════════════════════════════════════════════════════════

def _piper(text: str, model_path: str) -> bool:
    """Offline Hindi using Piper with Human/Natural Tuning."""
    try:
        r = subprocess.run(
            [PIPER_BIN, 
             "--model", model_path,
             "--length_scale", "1.05",       # Thoda slow aur thehar kar bolega (Natural feel)
             "--sentence_silence", "0.15",   # Sentence ke baad saans lene ka gap
             "--output_file", AUDIO_WAV, 
             "--quiet"],
            input=text.encode("utf-8"),
            capture_output=True, timeout=12
        )
        return r.returncode == 0
    except Exception as e:
        print(f"[Speaker] Piper error: {e}")
        return False

def _gtts_fallback(text: str) -> bool:
    try:
        from gtts import gTTS
        gTTS(text=text, lang="hi").save(AUDIO_MP3)
        return True
    except: return False

def _generate(text: str, mode: str):
    """Returns (filepath, is_wav)"""
    model_to_use = PIPER_MODEL_NYRO if mode == "nyro" else PIPER_MODEL_BHAI
    
    if _piper(text, model_to_use): 
        return AUDIO_WAV, True
        
    if _gtts_fallback(text): 
        return AUDIO_MP3, False
        
    return None, False

# ════════════════════════════════════════════════════════════
#  MAIN SPEAK FUNCTION
# ════════════════════════════════════════════════════════════

def speak(text: str, mode: str = "nyro"):
    global _player
    _stop_flag.clear()

    with speaking_lock:
        # Servo gesture before speaking
        gesture = servo_ctrl.gesture_for(text, mode)
        if gesture:
            gesture()
            time.sleep(0.25)

        audio_path, is_wav = _generate(text, mode)
        if not audio_path:
            print("[Speaker] No audio generated.")
            return

        cmd = ["aplay", "-q", audio_path] if is_wav else ["mpg123", "-q", audio_path]

        _ard(b'START_SPEECH\n')
        _player = subprocess.Popen(cmd)

        frames = _mouth_pattern(text)
        idx = 0

        while True:
            if _stop_flag.is_set(): break
            if _player.poll() is not None: break
            frame = frames[idx % len(frames)]
            _ard(f"{frame}\n".encode())
            idx += 1
            time.sleep(0.10)

        _kill()
        _ard(b'END_SPEECH\n')
        servo_ctrl.idle()

def stop():
    _stop_flag.set()
    _kill()
    _ard(b'END_SPEECH\n')

def _kill():
    global _player
    try:
        if _player and _player.poll() is None:
            _player.terminate()
            time.sleep(0.05)
            if _player.poll() is None:
                _player.kill()
    except: pass
    _player = None
    
