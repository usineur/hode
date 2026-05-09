#!/usr/bin/env python3
"""
Heart of Darkness - Automated walkthrough.
Records inputs for HD replay comparison.
"""

import socket, json, time, os

SOCKET_PATH = "/tmp/hode-test.sock"

SYS_INP_JUMP  = 0x20
SYS_INP_SHOOT = 0x40

DIR_RIGHT = 2
ACT_RUN = 1
ACT_JUMP = 2

class HodeClient:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.buf = b""
        for i in range(60):
            try:
                self.sock.connect(path)
                self.sock.settimeout(2.0)
                return
            except:
                time.sleep(0.5)
        raise Exception(f"Could not connect to {path}")

    def send(self, cmd):
        self.sock.sendall((json.dumps(cmd) + "\n").encode())

    def recv_line(self):
        while True:
            if b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                return line.decode()
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    return None
                self.buf += chunk
            except socket.timeout:
                return None

    def get_state(self):
        self.sock.settimeout(2.0)
        self.send({"cmd": "get_state"})
        # Read until we get a valid JSON line with "andy" in it
        for _ in range(10):
            line = self.recv_line()
            if line:
                try:
                    obj = json.loads(line)
                    if "andy" in obj:
                        return obj
                except:
                    pass
        return {}

    def inject(self, raw=0, direction=0, action=0, frames=1):
        cmd = {"cmd": "input", "dir": direction, "act": action, "frames": frames}
        if raw:
            cmd["raw"] = raw
        self.send(cmd)
        # No response expected

    def close(self):
        self.sock.close()

def main():
    print("=" * 60)
    print("  Heart of Darkness - Automated Walkthrough")
    print("=" * 60)

    client = HodeClient(SOCKET_PATH)
    recording = []

    def play(desc, raw=0, direction=0, action=0, dur=0.5):
        frames = max(1, int(dur / 0.08))
        recording.append({"d": desc, "dir": direction, "act": action, "raw": raw, "f": frames})
        client.inject(raw=raw, direction=direction, action=action, frames=frames)
        time.sleep(dur)

    def state(label=""):
        s = client.get_state()
        a = s.get("andy", {})
        x, y = a.get("x", "?"), a.get("y", "?")
        print(f"  {label:30s} lvl={s.get('level','?')} scr={s.get('screen','?')} "
              f"pos=({str(x):>4},{str(y):>4}) anim={a.get('anim','?')}")
        return s

    # ===== MENU =====
    print("\n--- Menu ---")
    print("  Loading game...")
    time.sleep(3)
    state("Initial")

    # Skip cutscene / press jump to start
    play("Skip/Jump", raw=SYS_INP_JUMP, dur=0.25)
    time.sleep(1.0)
    state("After skip")

    # Select Play on title screen
    play("Select Play", raw=SYS_INP_JUMP, dur=0.25)
    time.sleep(1.5)
    state("After Play")

    # Confirm through menu screens
    for i in range(10):
        play(f"Confirm {i}", raw=SYS_INP_JUMP, dur=0.25)
        time.sleep(0.8)
        s = state(f"Menu {i}")
        a = s.get("andy", {})
        if isinstance(a.get("x"), int) and (a["x"] != 0 or a["y"] != 0):
            print("  >> IN GAME!")
            break

    # ===== LEVEL 1 =====
    print("\n--- Level 1: Rock Canyon ---")
    time.sleep(2)
    state("Level start")

    moves = [
        ("Walk right",  DIR_RIGHT, 0,        2.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Walk right",  DIR_RIGHT, 0,        1.5),
        ("Run right",   DIR_RIGHT, ACT_RUN,  2.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Walk right",  DIR_RIGHT, 0,        1.0),
        ("Run right",   DIR_RIGHT, ACT_RUN,  3.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Pause",       0,         0,        0.3),
        ("Walk right",  DIR_RIGHT, 0,        2.5),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Run right",   DIR_RIGHT, ACT_RUN,  4.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Walk right",  DIR_RIGHT, 0,        2.0),
        ("Run right",   DIR_RIGHT, ACT_RUN,  3.0),
        ("Walk right",  DIR_RIGHT, 0,        3.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Run right",   DIR_RIGHT, ACT_RUN,  4.0),
        ("Walk right",  DIR_RIGHT, 0,        2.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Walk right",  DIR_RIGHT, 0,        3.0),
        ("Run right",   DIR_RIGHT, ACT_RUN,  5.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Walk right",  DIR_RIGHT, 0,        2.0),
        ("Run right",   DIR_RIGHT, ACT_RUN,  4.0),
        ("Walk right",  DIR_RIGHT, 0,        3.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
        ("Run right",   DIR_RIGHT, ACT_RUN,  5.0),
        ("Walk right",  DIR_RIGHT, 0,        4.0),
        ("Jump",        DIR_RIGHT, ACT_JUMP, 0.5),
    ]

    prev_scr = None
    for desc, d, a, dur in moves:
        play(desc, direction=d, action=a, dur=dur)
        s = client.get_state()
        andy = s.get("andy", {})
        scr = s.get("screen")
        chg = " ** NEW SCREEN **" if scr != prev_scr else ""
        prev_scr = scr
        x = andy.get("x", "?")
        y = andy.get("y", "?")
        print(f"  {desc:20s} scr={scr} pos=({str(x):>4},{str(y):>4}){chg}")

        if s.get("level", 0) != 0 or s.get("endLevel"):
            print("  >> LEVEL END")
            break

    # ===== SAVE =====
    final = client.get_state()
    print(f"\n--- Final ---")
    print(json.dumps(final, indent=2))

    total = sum(r["f"] for r in recording)
    data = {"mode": "normal", "total_frames": total, "final": final, "seq": recording}
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walkthrough_recording.json")
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
    print(f"\nSaved {path} ({len(recording)} seq, {total} frames)")

    client.close()

if __name__ == "__main__":
    main()
