#!/usr/bin/env python3
"""Runs the host build of q2-connectfour as Q2 Pod would (fake /dev/fb0 from fbshim.so, input as text
datagrams on /tmp/q2-connectfour.input.sock, TERM to end it) through one scenario, and prints what it
logged and its last state. Frames are saved as OUT_DIR/<scenario>-<name>.raw (view.py shows them).

Usage: drive.py BUILD_DIR OUT_DIR SCENARIO
Scenarios: menus, friend, ai, pause, quit"""
import os, shutil, signal, socket, subprocess, sys, tempfile, time

build, outdir, scenario = sys.argv[1:4]
build, outdir = os.path.abspath(build), os.path.abspath(outdir)
SOCK = '/tmp/q2-connectfour.input.sock'
card = tempfile.mkdtemp()
app = os.path.join(card, '.connectfour')
os.mkdir(app)
shutil.copy(os.path.join(build, 'q2-connectfour'), app)
dump = os.path.join(card, 'fb.raw')
env = dict(os.environ, LD_PRELOAD=os.path.join(build, 'fbshim.so'), FB_DUMP=dump,
           FB_FINAL=os.path.join(card, 'fb.final'), C4_STATE='1', C4_SEED='7')
if scenario == 'ai':
    open(os.path.join(app, 'connectfour.sav'), 'w').write('level 0\n')  # Easy: a game the test can win
if os.path.exists(SOCK):
    os.unlink(SOCK)
proc = subprocess.Popen([os.path.join(app, 'q2-connectfour')], env=env, cwd=app)
log = os.path.join(app, 'connectfour.log')

def wait_for(cond, secs, what):
    end = time.time() + secs
    while time.time() < end:
        if cond():
            return
        time.sleep(0.05)
    proc.kill()
    sys.exit(f'FAIL: no {what} after {secs} s\n' + (open(log).read() if os.path.exists(log) else ''))

def state():
    out = {}
    try:
        for line in open(os.path.join(app, 'state.txt')):
            k, v = line.rsplit(' ', 1)
            out[k] = int(v)
    except (OSError, ValueError):
        pass
    return out

wait_for(lambda: os.path.exists(SOCK), 5, 'input socket')
wait_for(lambda: os.path.exists(os.path.join(app, 'state.txt')), 10, 'first frame')
s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

def send(m):
    s.sendto(m.encode(), SOCK)

def key(code, hold=0.06):
    send(f'k d {code}'); time.sleep(hold); send(f'k u {code}'); time.sleep(0.12)

def wheel(total, steps=4, gap=0.03):  # +: anticlockwise
    for _ in range(steps):
        send(f'w {total // steps}'); time.sleep(gap)
    time.sleep(0.08)

def tap(x, y):
    send(f't d {x} {y}'); time.sleep(0.05); send(f't u {x} {y}'); time.sleep(0.12)

def snap(name):
    time.sleep(0.15)
    for _ in range(50):
        data = open(dump, 'rb').read()
        if len(data) == 320 * 375 * 4:
            break
        time.sleep(0.01)
    open(os.path.join(outdir, f'{scenario}-{name}.raw'), 'wb').write(data)

def settle(secs=3.0):  # until nothing falls and it's a person's move (or the game is over)
    end = time.time() + secs
    while time.time() < end:
        st = state()
        if st.get('state') != 1 or (not st.get('falling') and (st.get('mode') == 1 or st.get('turn') == 1)):
            return st
        time.sleep(0.05)
    return state()

COL_X = [53 + 5 + 18 + 37 * c for c in range(7)]  # the columns' centres in the view
CENTRE, RETURN = 218, 170
results = {}
if scenario == 'menus':
    time.sleep(0.3); snap('title')
    lv0 = state()['save_level']
    wheel(-50); snap('title-level')   # clockwise: down two
    results['sel_after_wheel'] = state()['sel']
    key(CENTRE); time.sleep(0.2)
    results['level_changed'] = int(state()['save_level'] == (lv0 + 1) % 3)
    snap('title-level2')
    wheel(100)  # back up to the top
    key(CENTRE); time.sleep(0.3)
    results['started'] = state()['state']
    results['mode'] = state()['mode']
    settle(); snap('game')
elif scenario == 'friend':
    wheel(-25); key(CENTRE); time.sleep(0.3)
    results['mode'] = state()['mode']
    snap('start')
    # red down column 1 by the wheel, yellow in column 2 by touch: red's four go up column 1
    for i in range(7):
        if i % 2 == 0:
            want, cur = 0, state()['target']
            wheel(25 * (cur - want), max(1, cur - want)) if cur != want else None
            key(CENTRE)
        else:
            tap(COL_X[1], 200)
        settle()
        if i == 4:
            snap('mid')
    time.sleep(0.3); snap('over')
    st = state()
    results['winner'], results['p1'], results['over'] = st['winner'], st['p1'], st['state']
    key(CENTRE); time.sleep(0.3)
    results['next_starter'] = state()['starter']
    results['next_turn'] = state()['turn']
elif scenario == 'ai':
    key(CENTRE); time.sleep(0.3)  # Play vs AI (the first item)
    results['level'] = state()['level']
    moves = 0
    for i in range(30):
        st = settle(6.0)
        if st.get('state') != 1:
            break
        # a simple player: the first column from the middle out that isn't full
        before = st['moves']
        for c in (3, 2, 4, 1, 5, 0, 6):
            tap(COL_X[c], 150)
            time.sleep(0.05)
            if state()['moves'] > before:
                break
        moves += 1
        if i == 2:
            time.sleep(0.2); snap('thinking')
    time.sleep(0.4); snap('over')
    st = state()
    results['winner'], results['over'], results['moves'] = st['winner'], st['state'], st['moves']
    key(RETURN); time.sleep(0.2)
    results['back_to_title'] = state()['state']
    time.sleep(0.1); snap('title-record')
elif scenario == 'pause':
    key(CENTRE); time.sleep(0.3)
    tap(COL_X[3], 150); settle(6.0)
    results['moves_before'] = state()['moves']
    # Return held: Q2 Pod passes its repeats on as more downs, which must open the pause once
    send(f'k d {RETURN}')
    for _ in range(7):
        time.sleep(0.07); send(f'k d {RETURN}')
    time.sleep(0.2)
    results['held_return'] = state()['state']
    send(f'k u {RETURN}'); time.sleep(0.2)
    snap('paused')
    wheel(-25); key(CENTRE); time.sleep(0.3)  # Take Back
    st = state()
    results['after_undo_state'], results['after_undo_moves'], results['after_undo_turn'] = st['state'], st['moves'], st['turn']
    key(RETURN); time.sleep(0.2)
    wheel(-75); key(CENTRE); time.sleep(0.3)  # Main Menu
    results['menu'] = state()['state']
elif scenario == 'quit':
    wheel(-100)  # down to Quit, the last
    snap('title-quit')
    send(f'k d {CENTRE}')  # it's gone before the release
    try:
        results['exited_alone'] = int(proc.wait(3) == 0)
    except subprocess.TimeoutExpired:
        results['exited_alone'] = 0

end = state()
proc.send_signal(signal.SIGTERM)
try:
    status = proc.wait(5)
except subprocess.TimeoutExpired:
    proc.kill()
    status = 'killed'
print(open(log).read())
print('exit status:', status)
print('panel:', open(os.path.join(card, 'fb.final')).read().strip() if os.path.exists(os.path.join(card, 'fb.final')) else '-')
print('socket left:', os.path.exists(SOCK))
for k, v in end.items():
    print(f'end: {k}: {v}')
for k, v in results.items():
    print(f'result: {k}: {v}')
shutil.rmtree(card)
