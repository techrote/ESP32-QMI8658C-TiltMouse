"""Synthetic one-shot delta / retained-state model. No USB or RF implementation.
All IDs, completion tickets and release revisions are PRIVATE instrumentation or
transport state, never logical mouse-report fields. One serialized owner task.
"""
from __future__ import annotations
from dataclasses import dataclass, asdict

@dataclass(frozen=True)
class Report:
    dx: int = 0
    dy: int = 0
    buttons: int = 0
    kind: str = 'current'

@dataclass
class Flight:
    ticket: tuple[int, int]
    dx: int
    dy: int
    buttons: int
    origin: str | None
    debt: int
    revision: int
    accepted_at: int

class Transport:
    def __init__(self, name: str, active: bool = True):
        self.name, self.active = name, active
        self.ready = True
        self.accept_next = True
        self.desired = self.debt = self.revision = 0
        self.epoch = self.serial = 0
        self.flight: Flight | None = None
        self.history: list[dict] = []
        self.host_buttons = self.host_x = self.host_y = 0
        self.last_ticket = None

    def _record(self, event: str, **data):
        self.history.append({'transport': self.name, 'event': event, **data})

    def _state(self, buttons: int):
        falling = self.desired & ~buttons
        if falling:
            self.debt |= falling
            self.revision += 1
        self.desired = buttons

    def neutralize(self):
        self.desired = 0
        self.debt = 3
        self.revision += 1
        self._record('neutralize', revision=self.revision)

    def _send(self, dx: int, dy: int, buttons: int, origin: str | None, now: int):
        if not self.ready or self.flight is not None:
            return 'not_ready'
        if not self.accept_next:
            self.accept_next = True
            self._record('submit_failed', dx=dx, dy=dy, buttons=buttons, origin=origin)
            return 'submit_failed'
        assert not (buttons & self.debt), 'a pending release must precede re-press'
        self.serial += 1
        self.flight = Flight((self.epoch, self.serial), dx, dy, buttons, origin,
                             self.debt, self.revision, now)
        self.last_ticket = self.flight.ticket
        self._record('accepted', **asdict(self.flight))
        return 'ok'

    def publish(self, report: Report | None, origin: str, now: int, deadline: int):
        """Consumes the candidate on ALL outcomes. No rejected-motion storage."""
        self._record('offered', origin=origin, now=now, deadline=deadline,
                     report=asdict(report) if report else None)
        if not self.active:
            result = {'motion':'dropped', 'state':'unchanged', 'reason':'inactive'}
            self._record('result', origin=origin, **result)
            return result
        valid = (report is not None and report.kind == 'current' and
                 type(report.dx) is int and type(report.dy) is int and
                 type(report.buttons) is int and -127 <= report.dx <= 127 and
                 -127 <= report.dy <= 127 and 0 <= report.buttons <= 3)
        forced = report is not None and report.kind == 'release'
        if not valid or forced:
            self.neutralize()
            self.service(now)
            result = {'motion':'none' if forced else 'dropped',
                      'state':'safe_release_retained',
                      'reason':'ok' if forced else 'invalid'}
            self._record('result', origin=origin, **result)
            return result
        self._state(report.buttons)
        dx, dy = report.dx, report.dy
        reason = 'ok'
        if deadline < now:
            dx = dy = 0
            reason = 'expired'
        if self.desired & self.debt:
            dx = dy = 0
            reason = 'release_first'
        source = origin if dx or dy else None
        send = self._send(dx, dy, self.desired & ~self.debt, source, now)
        if send != 'ok' and reason == 'ok':
            reason = send
        movement = 'none' if not (report.dx or report.dy) else (
            'accepted' if send == 'ok' and (dx or dy) else 'dropped')
        result = {'motion':movement, 'state':'buttons_retained', 'reason':reason}
        self._record('result', origin=origin, **result)
        return result

    def service(self, now: int):
        if not self.active:
            return 'inactive'
        return self._send(0, 0, self.desired & ~self.debt, None, now)

    def complete(self, ok: bool, delivered: bool, ticket=None):
        """ok is chosen backend completion evidence, NOT inherently host delivery.
        delivered is a hidden test oracle, unavailable to application/adapter.
        Failure can therefore be injected before or after hidden delivery.
        """
        if ticket is None:
            ticket = self.flight.ticket if self.flight else self.last_ticket
        if self.flight is None or self.flight.ticket != tuple(ticket):
            self._record('ignored_completion', ticket=ticket)
            return
        f = self.flight
        self.flight = None
        if delivered:
            self.host_x += f.dx
            self.host_y += f.dy
            self.host_buttons = f.buttons
            self._record('delivered', **asdict(f))
        if ok and f.revision == self.revision:
            self.debt &= ~f.debt
        self._record('completion', ok=ok, delivered=delivered, ticket=ticket)
        # No motion is restored, even for failure/unknown delivery.

    def activate(self):
        assert self.flight is None, 'backend must be fenced before reuse'
        self.epoch += 1
        self.active = True
        self.neutralize()

    def assert_invariants(self):
        seen = set()
        for item in self.history:
            if item['event'] != 'accepted':
                continue
            assert -127 <= item['dx'] <= 127 and -127 <= item['dy'] <= 127
            assert 0 <= item['buttons'] <= 3
            origin = item['origin']
            if item['dx'] or item['dy']:
                assert origin is not None, 'heartbeat replayed old delta'
                assert origin not in seen, 'same consumed delta accepted twice'
                seen.add(origin)
                candidates = [e for e in self.history if e['event']=='offered'
                              and e['origin']==origin]
                assert len(candidates) == 1, 'application offered a consumed delta twice'
                assert item['accepted_at'] <= candidates[0]['deadline'], 'expired delta emitted'
        return seen

class Router:
    """Switching is OPTIONAL; this model chooses fail-closed fencing + rearm."""
    def __init__(self):
        self.usb = Transport('USB')
        self.radio = Transport('RADIO', False)
        self.current, self.target = self.usb, None
        self.phase = 'run'
        self.residual_x = self.residual_y = 0.0

    def publish(self, report, origin, now, deadline):
        if self.phase == 'quiescing':
            return {'motion':'dropped', 'state':'unchanged', 'reason':'inactive'}
        if self.phase == 'rearm':
            # Only fresh, valid all-up input rearms; the rearm sample carries no motion.
            if (report and report.kind == 'current' and report.buttons == 0 and
                    type(report.dx) is int and type(report.dy) is int and
                    -127 <= report.dx <= 127 and -127 <= report.dy <= 127 and
                    deadline >= now and not self.current.debt and self.current.flight is None):
                self.phase = 'run'
            return {'motion':'dropped', 'state':'unchanged', 'reason':'inactive'}
        return self.current.publish(report, origin, now, deadline)

    def begin_switch(self):
        assert self.phase == 'run'
        self.phase = 'quiescing'
        self.target = self.radio if self.current is self.usb else self.usb
        self.residual_x = self.residual_y = 0.0
        self.current.neutralize()

    def finish_switch(self):
        assert self.phase == 'quiescing'
        assert self.current.flight is None and self.current.debt == 0
        assert self.current.desired == 0
        self.current.active = False
        self.current = self.target
        self.current.activate()
        self.target = None
        self.phase = 'rearm'

    def assert_invariants(self):
        a, b = self.usb.assert_invariants(), self.radio.assert_invariants()
        assert a.isdisjoint(b), 'one delta accepted by two transports'
        assert int(self.usb.active) + int(self.radio.active) == 1
