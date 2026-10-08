#!/usr/bin/env python3
import json, random, sys
from pathlib import Path
from state_model import Report, Transport, Router
ROOT=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'results'
out.mkdir(parents=True,exist_ok=True)
fixtures=json.loads((ROOT/'fixtures/scenarios.json').read_text())
traces=[]
for scenario in fixtures['scenarios']:
    t=Transport('USB')
    for ev in scenario['events']:
        match ev:
            case ['ready', value]: t.ready=value
            case ['accept_next', value]: t.accept_next=value
            case ['publish', ident, now, deadline, x, y, buttons]:
                t.publish(Report(x,y,buttons),ident,now,deadline)
            case ['complete', ok, delivered]: t.complete(ok,delivered)
            case ['service', now]: t.service(now)
            case _: raise ValueError(ev)
        t.assert_invariants()
    assert [t.host_x,t.host_y]==scenario['expected_xy'],scenario['name']
    assert t.host_buttons==scenario['expected_buttons'],scenario['name']
    assert t.debt==0,scenario['name']
    traces.append({'name':scenario['name'],'events':t.history,
                   'xy':[t.host_x,t.host_y],'buttons':t.host_buttons})
# Release barrier must survive release/re-press and failures.
t=Transport('USB');t.publish(Report(buttons=1),'p',0,8);t.complete(True,True)
t.ready=False;t.publish(Report(buttons=0),'r',8,16)
t.publish(Report(8,0,1),'p2',16,24);t.ready=True;t.service(24)
assert t.flight.buttons==0 and t.flight.dx==0
t.complete(False,False);assert t.debt==1
t.service(32);t.complete(True,True);t.service(40);t.complete(True,True)
assert [e['buttons'] for e in t.history if e['event']=='delivered']==[1,0,1]
t.assert_invariants()
traces.append({'name':'release_barrier_retry','events':t.history})
# No activity on inactive route. Invalid/NULL and forced release cannot replay fields.
u=Transport('inactive',False);u.publish(Report(4,0,1),'i',0,8)
assert u.flight is None and u.desired==0
for candidate in [None, Report(99,88,3,'invalid'), Report(99,88,3,'release'), Report(0,0,4)]:
    t=Transport('USB');t.publish(candidate,'bad',0,8)
    assert t.flight is not None and (t.flight.dx,t.flight.dy,t.flight.buttons)==(0,0,0)
# Switch: fence old owner, clear residual, new neutral, fresh all-up rearm.
r=Router();r.residual_x=.75
r.publish(Report(5,0,1),'old',0,8);old_ticket=r.usb.flight.ticket
r.begin_switch();r.publish(Report(99,0,1),'blocked',1,9)
try:
    r.finish_switch()
    raise AssertionError('switch allowed before fence')
except AssertionError as e:
    if str(e)=='switch allowed before fence': raise
r.usb.complete(True,True);r.usb.service(8);r.usb.complete(True,True)
r.finish_switch();r.radio.service(16);r.radio.complete(True,True)
r.publish(Report(buttons=1),'stillheld',24,32);assert r.phase=='rearm'
r.publish(Report(),'allup',32,40);assert r.phase=='run'
r.publish(Report(3,0,0),'new',40,48);r.radio.complete(True,True)
r.usb.complete(True,True,old_ticket)
r.assert_invariants();assert r.residual_x==0 and r.usb.host_buttons==0
assert r.usb.host_x==5 and r.radio.host_x==3
traces.append({'name':'fenced_switch','usb':r.usb.history,'radio':r.radio.history})
# Seeded, finite stateful stress; eventual release under restored service/completion.
rng=random.Random(5005);steps=0
for run in range(64):
    t=Transport('USB')
    for step in range(100):
        steps+=1;now=step*8
        t.ready=rng.choice([True,True,False]);t.accept_next=rng.choice([True,True,False])
        if t.flight and rng.random()<.65:
            ok=rng.choice([True,False]);t.complete(ok, True if ok else rng.choice([True,False]))
        t.publish(Report(rng.randint(-127,127),rng.randint(-127,127),rng.randrange(4)),
                  f'{run}-{step}',now,now+8)
        t.assert_invariants()
    if t.flight: t.complete(False,False)
    t.ready=t.accept_next=True;t.publish(Report(kind='release'),f'{run}-release',808,816)
    t.complete(True,True);assert t.host_buttons==0 and t.debt==0
# Deliberate defects, checked by the same history invariants or release trace.
controls=[]
def expect_defect(name, operation):
    try: operation()
    except AssertionError as e: controls.append({'name':name,'detected':True,'message':str(e)})
    else: raise AssertionError('missed deliberate defect: '+name)
def duplicate():
    t=Transport('USB');t.publish(Report(7),'d',0,8);t.complete(False,True)
    t.publish(Report(7),'d',8,16);t.assert_invariants()
expect_defect('retry_after_ambiguous_acceptance',duplicate)
def stale():
    t=Transport('USB');t.publish(Report(7),'s',0,8);t.complete(True,True)
    t._send(7,0,0,'s',32);t.assert_invariants()
expect_defect('heartbeat_replays_cached_delta',stale)
def expired():
    t=Transport('USB');t.ready=False;t.publish(Report(7),'e',0,8);t.ready=True
    t._send(7,0,0,'e',32);t.assert_invariants()
expect_defect('old_rejected_delta_requeued',expired)
def two_owners():
    r=Router();r.radio.active=True
    r.usb.publish(Report(7),'d',0,8);r.radio.publish(Report(7),'d',0,8)
    r.assert_invariants()
expect_defect('two_transport_owners',two_owners)
def lose_release():
    t=Transport('USB');t.publish(Report(buttons=1),'p',0,8);t.complete(True,True)
    t.ready=False;t.publish(Report(),'r',8,16);t.publish(Report(buttons=1),'p2',16,24)
    t.debt=0 # deliberate bug: latest state overwrites release debt
    t.ready=True;t.service(24)
    assert t.flight.buttons==0,'release lost when overwritten by re-press'
expect_defect('latest_state_erases_release_obligation',lose_release)
# Fundamental wireless counterexample: MAC-level success is not receiver delivery.
t=Transport('RADIO');t.publish(Report(buttons=1),'p',0,8);t.complete(True,True)
t.publish(Report(),'r',8,16);t.complete(True,False) # ACK evidence weaker than USB fake
assert t.host_buttons==1
t.publish(Report(buttons=1),'p2',16,24);t.complete(True,True)
assert [e['buttons'] for e in t.history if e['event']=='delivered']==[1,1]
traces.append({'name':'weak_ack_counterexample_NOT_a_guarantee','events':t.history})
summary={'fixture_scenarios_passed':len(fixtures['scenarios']),
         'extra_checks':['release barrier retry','inactive route','four invalid/release candidates',
                         'fenced switch and fresh-all-up rearm'],
         'seed':5005,'stress_runs':64,'stress_steps':steps,
         'deliberate_defects':controls,
         'weak_ack_counterexample':'A lost release followed by re-press is NOT repaired by repeating current pressed state. Receiver ACK or fail-closed timeout/rearm is required for a stronger guarantee.',
         'scope':'Synthetic model only. No end-to-end USB/RF claim.'}
(out/'model_results.json').write_text(json.dumps(summary,indent=2)+'\n')
(out/'worked_traces.json').write_text(json.dumps(traces,indent=2)+'\n')
print(json.dumps(summary,indent=2))
