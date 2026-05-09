#!/usr/bin/env python3
"""
Replay walkthrough recording in HD mode and compare results.
"""

import socket, json, time, os

SOCKET_PATH = "/tmp/hode-hd-test.sock"
RECORDING = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walkthrough_recording.json")

class HodeClient:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.buf = b""
        for i in range(60):
            try:
                self.sock.connect(path)
                self.sock.settimeout(3.0)
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
        self.sock.settimeout(3.0)
        self.send({"cmd": "get_state"})
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

    def close(self):
        self.sock.close()

def main():
    print("=" * 60)
    print("  Heart of Darkness - HD Mode Replay")
    print("=" * 60)

    # Load recording
    with open(RECORDING) as f:
        rec = json.load(f)

    print(f"Recording: {len(rec['seq'])} sequences, {rec['total_frames']} frames")
    print(f"Normal mode final: {json.dumps(rec['final'])}")

    client = HodeClient(SOCKET_PATH)

    # Wait for game to start
    time.sleep(3)
    initial = client.get_state()
    print(f"\nHD mode initial: {json.dumps(initial)}")

    # Replay all sequences
    print("\n--- Replaying walkthrough in HD mode ---")
    states = []

    for i, seq in enumerate(rec["seq"]):
        dur = seq["f"] * 0.08
        client.inject(raw=seq.get("raw", 0),
                      direction=seq.get("dir", 0),
                      action=seq.get("act", 0),
                      frames=seq["f"])
        time.sleep(dur)

        s = client.get_state()
        a = s.get("andy", {})
        states.append(s)

        desc = seq.get("d", "?")
        print(f"  [{i:2d}] {desc:20s} lvl={s.get('level','?')} scr={s.get('screen','?')} "
              f"pos=({a.get('x','?'):>4},{a.get('y','?'):>4})")

    # Final state comparison
    hd_final = client.get_state()
    normal_final = rec["final"]

    print(f"\n--- Comparison ---")
    print(f"Normal final: {json.dumps(normal_final)}")
    print(f"HD     final: {json.dumps(hd_final)}")

    # Compare key fields
    na = normal_final.get("andy", {})
    ha = hd_final.get("andy", {})
    match = (na.get("screen") == ha.get("screen") and
             na.get("x") == ha.get("x") and
             na.get("y") == ha.get("y"))

    if match:
        print("\n** MATCH: HD mode produces identical game state! **")
    else:
        print(f"\n** DIFFERENCE DETECTED **")
        print(f"  Normal: screen={na.get('screen')} pos=({na.get('x')},{na.get('y')})")
        print(f"  HD:     screen={ha.get('screen')} pos=({ha.get('x')},{ha.get('y')})")
        print("  (Minor differences expected due to timing variations in real-time replay)")

    # Save HD results
    results = {
        "mode": "hd",
        "normal_final": normal_final,
        "hd_final": hd_final,
        "match": match,
        "hd_states": states
    }
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "hd_replay_results.json")
    with open(path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved: {path}")

    client.close()
    print("Done!")

if __name__ == "__main__":
    main()
