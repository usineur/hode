#!/usr/bin/env python3
"""
Heart of Darkness - All levels test in 4K mode.
Starts a separate game instance per level for clean state.
"""

import socket, json, time, os, subprocess, signal

SOCKET_BASE = "/tmp/hode-4k-lvl"
GAME_BIN = "./hode"

LEVELS = [
    (0, "rock", "Canyon"),
    (1, "fort", "Fort"),
    (2, "pwr1", "Power 1 (Swamp)"),
    (3, "isld", "Island"),
    (4, "lava", "Lava"),
    (5, "pwr2", "Power 2 (Underwater)"),
    (6, "lar1", "Lair 1"),
    (7, "lar2", "Lair 2"),
    (8, "dark", "Dark (Final)"),
]

DIR_RIGHT = 2
DIR_LEFT = 8
ACT_RUN = 1
ACT_JUMP = 2
ACT_SHOOT = 4

class HodeClient:
    def __init__(self, path, timeout=30):
        self.path = path
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.buf = b""
        for i in range(timeout * 2):
            try:
                self.sock.connect(path)
                self.sock.settimeout(5.0)
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
                chunk = self.sock.recv(8192)
                if not chunk: return None
                self.buf += chunk
            except socket.timeout:
                return None

    def get_state(self):
        try:
            self.sock.settimeout(5.0)
            self.send({"cmd": "get_state"})
            for _ in range(5):
                line = self.recv_line()
                if line:
                    try:
                        obj = json.loads(line)
                        if "andy" in obj: return obj
                    except: pass
        except:
            pass
        return {}

    def inject(self, direction=0, action=0, frames=1):
        try:
            self.send({"cmd": "input", "dir": direction, "act": action, "frames": frames})
        except: pass

    def close(self):
        try: self.sock.close()
        except: pass

def start_game(level_num, sock_path):
    """Start a game instance for a specific level."""
    try: os.unlink(sock_path)
    except: pass

    env = os.environ.copy()
    env["SDL_AUDIODRIVER"] = "dummy"

    cmd = [
        "xvfb-run", "-a", "-s", "-screen 0 3840x2880x24",
        GAME_BIN,
        f"--4k",
        f"--hd-cache=./cache",
        f"--automation={sock_path}",
        f"--level={level_num}",
    ]

    proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return proc

def stop_game(proc):
    try:
        proc.terminate()
        proc.wait(timeout=3)
    except:
        try: proc.kill()
        except: pass

def test_level(client, level_num):
    """Run gameplay on a level, collect states."""
    states = []
    screens = set()

    moves = [
        (DIR_RIGHT, 0,        1.5),
        (DIR_RIGHT, ACT_JUMP, 0.4),
        (DIR_RIGHT, ACT_RUN,  2.0),
        (DIR_RIGHT, ACT_JUMP, 0.4),
        (DIR_RIGHT, 0,        1.5),
        (DIR_RIGHT, ACT_RUN,  2.5),
        (DIR_RIGHT, ACT_JUMP, 0.4),
        (DIR_RIGHT, 0,        2.0),
        (DIR_RIGHT, ACT_RUN,  3.0),
        (DIR_RIGHT, ACT_JUMP, 0.4),
        (DIR_RIGHT, 0,        1.5),
        (DIR_LEFT,  0,        1.0),
        (DIR_LEFT,  ACT_JUMP, 0.4),
        (DIR_RIGHT, ACT_RUN,  2.0),
        (DIR_RIGHT, ACT_JUMP, 0.4),
        (DIR_RIGHT, 0,        2.0),
        (DIR_RIGHT, ACT_RUN,  3.0),
        (DIR_RIGHT, ACT_SHOOT,1.0),
        (DIR_RIGHT, ACT_JUMP, 0.4),
        (DIR_RIGHT, 0,        2.0),
    ]

    for d, a, dur in moves:
        frames = max(1, int(dur / 0.08))
        client.inject(direction=d, action=a, frames=frames)
        time.sleep(dur)
        s = client.get_state()
        if s:
            states.append(s)
            scr = s.get("screen")
            if scr is not None:
                screens.add(scr)

    return states, screens

def main():
    print("=" * 70)
    print("  Heart of Darkness - All Levels Test (4K / 3840x2880)")
    print("=" * 70)

    os.makedirs("./cache", exist_ok=True)
    results = {}

    for level_num, level_id, level_name in LEVELS:
        print(f"\n{'='*60}")
        print(f"  Level {level_num}: {level_name} ({level_id})")
        print(f"{'='*60}")

        sock_path = f"{SOCKET_BASE}{level_num}.sock"
        proc = None
        status = "FAIL"
        states = []
        screens = set()
        start_state = {}
        final_state = {}

        try:
            # Start game for this level
            print(f"  Starting game instance (level {level_num})...")
            proc = start_game(level_num, sock_path)

            # Connect
            client = HodeClient(sock_path, timeout=30)
            time.sleep(2)

            start_state = client.get_state()
            andy = start_state.get("andy", {})
            print(f"  Start: level={start_state.get('level')} screen={start_state.get('screen')} "
                  f"pos=({andy.get('x','?')},{andy.get('y','?')})")

            # Play
            print(f"  Playing...")
            states, screens = test_level(client, level_num)

            final_state = client.get_state()
            final_andy = final_state.get("andy", {})

            has_states = len(states) > 0
            has_positions = any(
                s.get("andy", {}).get("x", 0) != 0 or s.get("andy", {}).get("y", 0) != 0
                for s in states
            )
            status = "OK" if (has_states and has_positions) else "ISSUE"

            print(f"  Final: level={final_state.get('level')} screen={final_state.get('screen')} "
                  f"pos=({final_andy.get('x','?')},{final_andy.get('y','?')}) "
                  f"checkpoint={final_state.get('checkpoint')}")

            client.close()

        except Exception as e:
            print(f"  ERROR: {e}")
            status = "ERROR"

        finally:
            if proc:
                stop_game(proc)
            try: os.unlink(sock_path)
            except: pass

        results[level_id] = {
            "level": level_num,
            "name": level_name,
            "status": status,
            "states_collected": len(states),
            "screens_seen": sorted(list(screens)),
            "start_pos": start_state.get("andy", {}),
            "final_pos": final_state.get("andy", {}),
            "final_checkpoint": final_state.get("checkpoint"),
        }

        print(f"  Status: {status} | {len(states)} states | screens: {sorted(list(screens))}")

    # ===== SUMMARY =====
    print(f"\n{'='*70}")
    print(f"  SUMMARY - 4K Mode (15x scale, 3840x2880)")
    print(f"{'='*70}")
    print(f"  {'#':<3} {'Name':<22} {'Status':<8} {'States':<8} {'Screens':<25} {'Start Pos':<15} {'Final Pos':<15}")
    print(f"  {'-'*3} {'-'*22} {'-'*8} {'-'*8} {'-'*25} {'-'*15} {'-'*15}")

    all_ok = True
    for level_num, level_id, level_name in LEVELS:
        r = results.get(level_id, {})
        sp = r.get("start_pos", {})
        fp = r.get("final_pos", {})
        screens_str = str(r.get("screens_seen", []))
        s_pos = f"({sp.get('x','?')},{sp.get('y','?')})"
        f_pos = f"({fp.get('x','?')},{fp.get('y','?')})"
        st = r.get("status", "?")
        if st != "OK":
            all_ok = False
        print(f"  {level_num:<3} {level_name:<22} {st:<8} "
              f"{r.get('states_collected',0):<8} {screens_str:<25} {s_pos:<15} {f_pos:<15}")

    print(f"\n  Overall: {'ALL PASS' if all_ok else 'SOME ISSUES'}")

    # Check disk cache
    cache_files = 0
    cache_size = 0
    for root, dirs, files in os.walk("./cache"):
        for f in files:
            cache_files += 1
            cache_size += os.path.getsize(os.path.join(root, f))

    print(f"  Disk cache: {cache_files} files, {cache_size / 1024 / 1024:.1f} MB")

    # Save results
    output = {
        "mode": "4k",
        "scale": 15,
        "resolution": "3840x2880",
        "all_ok": all_ok,
        "cache_files": cache_files,
        "cache_size_mb": round(cache_size / 1024 / 1024, 1),
        "levels": results
    }
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "all_levels_4k_results.json")
    with open(path, "w") as f:
        json.dump(output, f, indent=2)
    print(f"  Results saved: {path}")

if __name__ == "__main__":
    main()
