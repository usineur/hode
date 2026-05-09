#!/usr/bin/env python3
"""
Heart of Darkness - Combat bot that fights monsters.
Reads monster positions, dodges attacks, and shoots back.
Tests all levels with actual combat gameplay.
"""

import socket, json, time, os, subprocess, random

SOCKET_BASE = "/tmp/hode-bot"
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

DIR_RIGHT = 2; DIR_LEFT = 8; DIR_UP = 1; DIR_DOWN = 4
ACT_RUN = 1; ACT_JUMP = 2; ACT_SHOOT = 4

class HodeClient:
    def __init__(self, path, timeout=30):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.buf = b""
        for i in range(timeout * 2):
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
                chunk = self.sock.recv(8192)
                if not chunk: return None
                self.buf += chunk
            except socket.timeout: return None

    def get_state(self):
        try:
            self.sock.settimeout(3.0)
            self.send({"cmd": "get_state"})
            for _ in range(5):
                line = self.recv_line()
                if line:
                    try:
                        obj = json.loads(line)
                        if "andy" in obj: return obj
                    except: pass
        except: pass
        return {}

    def inject(self, direction=0, action=0, frames=1):
        try:
            self.send({"cmd": "input", "dir": direction, "act": action, "frames": frames})
        except: pass

    def close(self):
        try: self.sock.close()
        except: pass

def start_game(level_num, sock_path, hd=True):
    try: os.unlink(sock_path)
    except: pass
    env = os.environ.copy()
    env["SDL_AUDIODRIVER"] = "dummy"
    cmd = ["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24",
           GAME_BIN, f"--automation={sock_path}", f"--level={level_num}"]
    if hd:
        cmd.append("--hd")
    return subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def stop_game(proc):
    try:
        proc.terminate()
        proc.wait(timeout=3)
    except:
        try: proc.kill()
        except: pass

class CombatBot:
    """AI bot that fights monsters and navigates levels."""

    def __init__(self, client):
        self.client = client
        self.deaths = 0
        self.kills_est = 0
        self.screens_visited = set()
        self.max_checkpoint = 0
        self.frames_played = 0
        self.last_andy_pos = (0, 0)
        self.stuck_counter = 0

    def play(self, duration_sec=60):
        """Play for duration_sec seconds with combat AI."""
        start_time = time.time()

        while time.time() - start_time < duration_sec:
            state = self.client.get_state()
            if not state:
                time.sleep(0.2)
                continue

            andy = state.get("andy", {})
            monsters = state.get("monsters", [])
            screen = state.get("screen", 0)
            checkpoint = state.get("checkpoint", 0)
            dying = andy.get("dying", False)
            has_cannon = andy.get("hasCannon", False)
            ax, ay = andy.get("x", 0), andy.get("y", 0)

            self.screens_visited.add(screen)
            if checkpoint > self.max_checkpoint:
                self.max_checkpoint = checkpoint

            if dying:
                self.deaths += 1
                # Wait for respawn
                time.sleep(1.0)
                self.stuck_counter = 0
                continue

            # Detect if stuck (same position for too long)
            if (ax, ay) == self.last_andy_pos:
                self.stuck_counter += 1
            else:
                self.stuck_counter = 0
            self.last_andy_pos = (ax, ay)

            # Decide action based on monsters and position
            direction, action = self.decide(ax, ay, monsters, has_cannon, screen)

            # If stuck, try random movement
            if self.stuck_counter > 10:
                direction = random.choice([DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_DOWN])
                action = random.choice([0, ACT_JUMP, ACT_RUN])
                self.stuck_counter = 0

            dur = 0.3
            frames = max(1, int(dur / 0.08))
            self.client.inject(direction=direction, action=action, frames=frames)
            self.frames_played += frames
            time.sleep(dur)

    def decide(self, ax, ay, monsters, has_cannon, screen):
        """Combat AI decision making."""
        if not monsters:
            # No monsters: explore right
            return DIR_RIGHT, ACT_RUN

        # Find closest monster
        closest = None
        closest_dist = 999999
        for m in monsters:
            mx, my = m.get("x", 0), m.get("y", 0)
            dist = abs(mx - ax) + abs(my - ay)
            if dist < closest_dist:
                closest_dist = dist
                closest = m

        if not closest:
            return DIR_RIGHT, ACT_RUN

        mx, my = closest.get("x", 0), closest.get("y", 0)
        dx = mx - ax
        dy = my - ay

        # Combat logic
        if closest_dist < 30:
            # Very close! Jump away and shoot if possible
            escape_dir = DIR_LEFT if dx > 0 else DIR_RIGHT
            if has_cannon:
                return escape_dir, ACT_JUMP | ACT_SHOOT
            else:
                return escape_dir, ACT_JUMP

        elif closest_dist < 80:
            # Medium range: shoot if we have cannon, otherwise dodge
            if has_cannon:
                face_dir = DIR_RIGHT if dx > 0 else DIR_LEFT
                return face_dir, ACT_SHOOT
            else:
                # No cannon: run past or jump over
                if abs(dy) < 20:
                    return DIR_RIGHT if dx > 0 else DIR_LEFT, ACT_JUMP
                else:
                    return DIR_RIGHT, ACT_RUN

        else:
            # Far away: advance toward right side of level
            if dx > 0:
                # Monster is to the right, approach cautiously
                return DIR_RIGHT, ACT_RUN if closest_dist > 120 else 0
            else:
                # Monster is behind, keep moving right
                return DIR_RIGHT, ACT_RUN

def main():
    print("=" * 70)
    print("  Heart of Darkness - Combat Bot (HD Mode)")
    print("=" * 70)

    results = {}
    play_duration = 45  # seconds per level

    for level_num, level_id, level_name in LEVELS:
        print(f"\n{'='*60}")
        print(f"  Level {level_num}: {level_name}")
        print(f"{'='*60}")

        sock_path = f"{SOCKET_BASE}{level_num}.sock"
        proc = None

        try:
            proc = start_game(level_num, sock_path, hd=True)
            client = HodeClient(sock_path, timeout=20)
            time.sleep(2)

            start = client.get_state()
            andy = start.get("andy", {})
            monsters = start.get("monsters", [])
            print(f"  Start: pos=({andy.get('x')},{andy.get('y')}) "
                  f"cannon={andy.get('hasCannon')} monsters={len(monsters)}")

            bot = CombatBot(client)
            print(f"  Fighting for {play_duration}s...")
            bot.play(duration_sec=play_duration)

            final = client.get_state()
            fa = final.get("andy", {})
            fm = final.get("monsters", [])

            print(f"  Final: pos=({fa.get('x')},{fa.get('y')}) screen={final.get('screen')}")
            print(f"  Deaths: {bot.deaths} | Screens: {sorted(bot.screens_visited)} "
                  f"| Checkpoint: {bot.max_checkpoint} | Frames: {bot.frames_played}")

            results[level_id] = {
                "level": level_num,
                "name": level_name,
                "deaths": bot.deaths,
                "screens_visited": sorted(list(bot.screens_visited)),
                "max_checkpoint": bot.max_checkpoint,
                "frames_played": bot.frames_played,
                "start_pos": {"x": andy.get("x"), "y": andy.get("y")},
                "final_pos": {"x": fa.get("x"), "y": fa.get("y")},
                "final_screen": final.get("screen"),
                "status": "OK"
            }

            client.close()

        except Exception as e:
            print(f"  ERROR: {e}")
            results[level_id] = {"level": level_num, "name": level_name, "status": "ERROR", "error": str(e)}

        finally:
            if proc: stop_game(proc)
            try: os.unlink(sock_path)
            except: pass

    # Summary
    print(f"\n{'='*70}")
    print(f"  COMBAT BOT SUMMARY")
    print(f"{'='*70}")
    print(f"  {'#':<3} {'Level':<22} {'Deaths':<8} {'Screens':<20} {'Chkpt':<6} {'Final Pos':<15}")
    print(f"  {'-'*3} {'-'*22} {'-'*8} {'-'*20} {'-'*6} {'-'*15}")

    total_deaths = 0
    total_screens = 0
    for level_num, level_id, level_name in LEVELS:
        r = results.get(level_id, {})
        if r.get("status") != "OK":
            print(f"  {level_num:<3} {level_name:<22} {'ERROR':<8}")
            continue
        fp = r.get("final_pos", {})
        screens = r.get("screens_visited", [])
        total_deaths += r.get("deaths", 0)
        total_screens += len(screens)
        print(f"  {level_num:<3} {level_name:<22} {r.get('deaths',0):<8} "
              f"{str(screens):<20} {r.get('max_checkpoint',0):<6} "
              f"({fp.get('x','?')},{fp.get('y','?')})")

    print(f"\n  Total deaths: {total_deaths}")
    print(f"  Total screens explored: {total_screens}")

    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "combat_bot_results.json")
    with open(path, "w") as f:
        json.dump({"results": results, "total_deaths": total_deaths}, f, indent=2)
    print(f"  Results saved: {path}")

if __name__ == "__main__":
    main()
