"""Rendered per-shell shotgun checks using the existing QA player's real inputs."""
import json

def run_checks(c):
    action, seconds, wait = (c[n] for n in ('action', 'seconds', 'wait'))
    call, prop, check, capture = (c[n] for n in ('call', 'prop', 'check', 'capture'))
    weapon, montage, now, inject = (c[n] for n in ('weapon', 'montage', 'now', 'inject'))
    probe, report = c['probe'], c['R']
    samples = []
    ammo = lambda: int(call(weapon(), 'GetCurrentAmmo'))
    capacity = int(call(weapon(), 'GetAmmoPerMag'))
    report['capacity'] = capacity

    def phase(name):
        c['phase'] = 'shotgun_' + name

    def commits(start, label):
        rows = samples[start:]
        rises = [(a, b) for a, b in zip(rows, rows[1:]) if b['ammo'] > a['ammo']]
        check(label + '_adds_one_shell_per_commit', bool(rises) and all(b['ammo'] - a['ammo'] == 1 for a, b in rises),
              fatal=False, changes=[{'before': a['ammo'], 'after': b['ammo'], 'time': b['time'], 'montage': b['montage']} for a, b in rises])
        return rises

    def shot(aim=False, label='Fire'):
        before = ammo()
        # The native template fires and pumps inside a single shot montage.
        # Its separate Pump montage is only a fallback for bNeedsPump.
        token = 'Fire'
        probe.begin('DJMShotgun_' + label, token)
        end = now() + .07
        while now() < end:
            inject('IA_FireWeapon', 1)
            if aim: inject('IA_Aim', 1)
            yield
        inject('IA_FireWeapon', 0)
        for tick in seconds(1.8):
            if aim: inject('IA_Aim', 1)
            yield tick
        trace = probe.end()
        report.setdefault('motion_traces', {})[label] = trace
        check(label + '_consumes_one_shell', ammo() == before - 1, fatal=False, before=before, after=ammo())
        check(label + '_pump_travels_and_returns', trace['mechanism_ranges'].get('pump', {}).get('position_cm', 0) > 5 and probe.ready(),
              fatal=False, ranges=trace['mechanism_ranges'], ready_errors=probe.ready_errors())
        check(label + '_pump_paired_clocks', trace['paired_samples'] >= 4 and trace['max_paired_clock_error_seconds'] is not None and trace['max_paired_clock_error_seconds'] < .055,
              fatal=False, paired_samples=trace['paired_samples'], max_error_seconds=trace['max_paired_clock_error_seconds'])
        if aim:
            check('aimed_fire_keeps_weapon_near_aim_pose', trace['max_weapon_component_travel_cm'] < 8,
                  fatal=False, measured_cm=trace['max_weapon_component_travel_cm'], limit_cm=8.)
        check(label + '_pump_flag_cleared', not bool(prop(weapon(), 'bNeedsPump', False)), fatal=False)

    def flow():
        check('shotgun_seven_shell_capacity', capacity == 7, actual=capacity)
        phase('warm_fire')
        yield from action('IA_FireWeapon', .07)
        yield from seconds(2)
        phase('hip_fire')
        yield from shot(label='HipFire')
        yield from capture('Fire')
        phase('ads')
        before_fov = c['pc'].player_camera_manager.get_fov_angle()
        yield from action('IA_Aim', 1.2)
        inject('IA_Aim', 1)
        yield
        check('ads_zoom', c['pc'].player_camera_manager.get_fov_angle() < before_fov - 2)
        report['glove_geometry_ads'] = probe.glove_snapshot('ADS')
        yield from capture('ADS', True)
        for tick in seconds(.35): inject('IA_Aim', 1); yield tick
        phase('aimed_fire')
        yield from shot(aim=True, label='AimedFire')
        yield from capture('ADSAfireRecovery', True)
        inject('IA_Aim', 0)
        yield from seconds(.6)

        phase('partial_reload')
        start, before = len(samples), ammo()
        yield from action('IA_Reload')
        yield from seconds(.35)
        check('partial_reload_uses_owned_montage', montage() and '/DJMShotgun/' in c['path'](montage()), fatal=False)
        yield from capture('TacticalReload')
        yield from wait(lambda: ammo() == capacity and montage() is None, 20)
        yield from seconds(.15)
        rises = commits(start, 'partial_reload')
        check('partial_reload_exact_shell_count', len(rises) == capacity - before, fatal=False, commits=len(rises), missing=capacity-before)
        c['ready_pose']('partial_reload_restores_pump_gate_and_hidden_shell')

        phase('empty_tube')
        start = len(samples)
        deadline = now() + capacity * 4 + 10
        while ammo() > 0:
            if now() > deadline: raise TimeoutError('cannot empty shotgun with real inputs')
            yield from action('IA_FireWeapon', .07)
            if ammo() == 0: break
            yield from seconds(1.8)
        check('shotgun_empty', ammo() == 0)
        empty_start = len(samples) - 1
        phase('empty_reload')
        yield from seconds(1.8)
        if not montage(): yield from action('IA_Reload')
        yield from seconds(.3)
        yield from capture('EmptyReload')
        yield from wait(lambda: ammo() == capacity and montage() is None, 25)
        yield from seconds(.2)
        rises = commits(empty_start, 'empty_reload')
        check('empty_reload_adds_seven_shells', len(rises) == capacity, fatal=False, commits=len(rises))
        check('empty_reload_finishes_ready_to_fire', not bool(prop(weapon(), 'bNeedsPump', False)) and probe.ready(), fatal=False, ready_errors=probe.ready_errors())

        phase('interrupt_prepare')
        for _ in range(2):
            yield from action('IA_FireWeapon', .07)
            yield from seconds(1.8)
        before = ammo()
        check('interruption_starts_partially_loaded', 0 < before < capacity - 1)
        phase('reload_interrupt')
        yield from action('IA_Reload')
        yield from wait(lambda: ammo() > before, 8)
        committed = ammo()
        yield from action('IA_SecondaryWeapon')
        yield from wait(lambda: not c['is_selected'](), 8)
        yield from seconds(1.)
        yield from action('IA_PrimaryWeapon')
        yield from wait(c['is_selected'], 8)
        yield from seconds(3)
        check('interrupted_reload_preserves_committed_shells', committed <= ammo() <= capacity, fatal=False, committed=committed, actual=ammo())
        check('interruption_does_not_instantly_fill_tube', ammo() < capacity, fatal=False, actual=ammo(), capacity=capacity)
        c['ready_pose']('interrupted_reload_resets_pump_gate_and_hidden_shell')
        yield from capture('ReloadInterruptedReequipped')
        phase('inspect')
        c['inspect_press']()
        yield from seconds(.5)
        yield from capture('Inspect')
        yield from wait(lambda: montage() is None, 15)
        c['ready_pose']('inspect_returns_shotgun_ready')

    try:
        for step in flow():
            if c['is_selected']():
                m = montage()
                samples.append({'time': now(), 'phase': c['phase'], 'ammo': ammo(),
                                'montage': c['path'](m), 'hand_time': c['hands'].get_anim_instance().montage_get_position(m) if m else None,
                                'pump_needed': bool(prop(weapon(), 'bNeedsPump', False))})
            yield step
    finally:
        file = c['OUT'] / 'PerShell_Ammo_Trace.json'
        file.write_text(json.dumps({'samples': samples}, separators=(',', ':')))
        report['per_shell_trace'] = str(file)
