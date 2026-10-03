"""Rendered Carnival checks for new guns before exposing them in user loadouts."""
import unreal as u
import sys,time,json,re,traceback,hashlib,struct
from pathlib import Path
P=Path(__file__).resolve().parent
sys.path.insert(0,str(P))
from ue_expansion_common import OUT,DEST
from gameplay_probe import GameplayProbe,MP7AttachmentProbe
cmd=u.SystemLibrary.get_command_line()
match=re.search(r'-ExpansionGun=(MP7|Ballista|SwitchKnife|PGM|M1911|Pistol9mm|TalonPistol|UZI|AR15|MP5)',cmd)
assert match,'Missing explicit -ExpansionGun'
GUN=match.group(1)
BOLT_GUN=GUN in ['Ballista','PGM']
PISTOL=GUN in ['M1911','Pistol9mm','TalonPistol']
MAG_ACTOR=GUN=='MP7' or PISTOL
INTEGRATED_MAG=GUN in ['UZI','AR15','MP5']
MAG_BONE='magazine_joint' if GUN in ('Ballista','AR15') else 'magazine'
BOLT_SLIDE='bolt_joint' if GUN=='Ballista' else 'bolt_slide'
BOLT_ROTATE='bolt_joint' if GUN=='Ballista' else 'bolt_rotate'
SIDEARM=GUN=='SwitchKnife' or PISTOL
CAPTURE_ONLY='-ExpansionPoseCaptureOnly' in cmd
QA_CONFIG=globals().get('QA_CONFIG',{})
OUT=Path(QA_CONFIG.get('output',OUT/GUN));OUT.mkdir(parents=True,exist_ok=True)
MAP='/Game/Carnival/Maps/Carnival_Extraction'
GM=QA_CONFIG.get('game_mode',DEST+'/'+GUN+'/QA/BP_'+GUN+'_QAGameMode.BP_'+GUN+'_QAGameMode_C')
R={'status':'running','gun':GUN,'map':MAP,'checks':[],'captures':[],
   'limits':['Single-player rendered PIE; multiplayer not certified.','Visual grip approval requires reviewing the captured frames.',
             'Third-person still verifies appearance/attachment only; newly authored third-person choreography is not certified.']}
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
world=pawn=pc=inventory=hands=inputs=None
probe=None
mp7_probe=None
phase='starting';started=time.monotonic();busy=False;ending=None

def path(o):return o.get_path_name() if o else None
def prop(o,k,d=None):
    try:return o.get_editor_property(k)
    except Exception:return d
def call(o,n,*args):
    result=o.call_method(n,args=args)
    return result[0] if isinstance(result,(tuple,list)) and len(result)==1 else result
def now():return u.GameplayStatics.get_time_seconds(world)
def checkpoint():
    R.update(phase=phase,elapsed_seconds=round(time.monotonic()-started,2))
    (OUT/'Gameplay_Validation.json').write_text(json.dumps(R,indent=2,default=str))
def check(name,passed,fatal=True,**data):
    R['checks'].append(dict(name=name,passed=bool(passed),**data));checkpoint()
    if fatal:assert passed,name
def wait(fn,timeout=60):
    deadline=time.monotonic()+timeout
    while not fn():
        if time.monotonic()>deadline:raise TimeoutError(phase)
        yield
def seconds(duration):
    target=now()+duration;deadline=time.monotonic()+max(60,duration*15)
    while now()<target:
        if time.monotonic()>deadline:raise TimeoutError(phase)
        yield
def command(text):u.SystemLibrary.execute_console_command(world,text)
def inject(name,value):
    a=u.load_asset('/Game/FPS_Controller/Inputs/InputActions/'+name);assert a
    inputs.inject_input_vector_for_action(a,u.Vector(float(value),0,0),[],[])
def action(name,duration=.10):
    end=now()+duration
    while now()<end:inject(name,1);yield
    inject(name,0);yield
def weapon():return call(inventory,'GetCurrentWeapon')
def mesh():return next(c for c in weapon().get_components_by_class(u.SkeletalMeshComponent) if c.get_name()=='MeshFPP')
def montage():return hands.get_anim_instance().get_current_active_montage()
def is_selected():return bool(weapon() and ('BP_'+GUN)in weapon().get_class().get_name())
def paired_result(label):
    summary=probe.end();R.setdefault('motion_traces',{})[label]=summary
    error=summary['max_paired_clock_error_seconds']
    check(label+'_paired_clocks',summary['paired_samples']>=4 and error is not None and error<=.055,
          fatal=False,**summary)
    return summary
def ready_pose(label):
    errors=probe.ready_errors()
    check(label,probe.ready(),fatal=False,reference=probe.expected_source,bone_errors=errors)

def ballista_idle_delivery(label):
    ad=prop(weapon(),'AnimationData')
    assigned=path(prop(ad,'FPP_WeaponAnims'))
    expected=QA_CONFIG.get('expected_layer',DEST+'/'+GUN+'/Animations/ABP_FPP_'+GUN+'_Layer.ABP_FPP_'+GUN+'_Layer_C')
    check(label+'_owned_full_idle_layer',assigned==expected,fatal=False,actual=assigned,expected=expected)
    result=probe.idle_right_fingers(DEST+'/'+GUN+'/Animations/A_FPP_'+GUN+'_Idle')
    passed=result.pop('passed')
    check(label+'_authored_right_fingers',passed,fatal=False,**result)

def switchknife_idle_delivery(label):
    ad=prop(weapon(),'AnimationData')
    assigned=path(prop(ad,'FPP_WeaponAnims'))
    expected=DEST+'/SwitchKnife/Animations/ABP_FPP_SwitchKnife_Layer.ABP_FPP_SwitchKnife_Layer_C'
    check(label+'_knife_owned_full_idle_layer',assigned==expected,fatal=False,actual=assigned,expected=expected)
    # Importer names this sequence A_SwitchKnife_Idle_FPP.
    result=probe.idle_right_fingers(DEST+'/SwitchKnife/Animations/A_SwitchKnife_Idle_FPP')
    passed=result.pop('passed')
    check(label+'_knife_authored_right_fingers',passed,fatal=False,**result)
def mp7_ready(label):
    mp7_probe.refresh()
    check(label,mp7_probe.ready(),fatal=False,bone_errors=mp7_probe.ready_errors(),reference=mp7_probe.D+'/Animations/A_'+GUN+'_Mag_Idle')
def mp7_attachments(label):
    snapshot=mp7_probe.attachment_snapshot();R.setdefault('mp7_attachments',{})[label]=snapshot
    for key,expected in ([('magazine','BP_MP7_Magazine_C'),('foregrip','BP_MP7_Foregrip_C')] if GUN=='MP7' else [('magazine','BP_'+GUN+'_Magazine_C')]):
        row=snapshot[key];fpp=next((c for c in row['components']if c['name']=='FPP'),None)
        own_class=bool(row['class']and row['class'].startswith(mp7_probe.D+'/')and row['class'].endswith('.'+expected))
        expected_mesh='SK_'+GUN+'_Magazine'if key=='magazine'else'SKM_VerticalGrip_02'
        check(label+'_'+key+'_spawned_attached',own_class and fpp and fpp['mesh']and fpp['mesh'].split('.')[-1]==expected_mesh and fpp['attached_to_weapon_fpp'],fatal=False,actual=row)
def mp7_trace_result(label,require_both_move=True):
    summary=mp7_probe.end();R.setdefault('mp7_magazine_traces',{})[label]=summary
    error=summary['max_clock_error_seconds']
    check(label+'_magazine_paired_clocks',summary['paired_samples']>=4 and error is not None and error<=.055,fatal=False,**summary)
    if require_both_move:
        ranges=summary['mechanism_ranges']
        if MAG_ACTOR:
            # Verified against the authored stock clip: mag_02 is a parked spare,
            # while mag_01 alone performs the exchange. Both must reset after.
            check(label+'_magazine_exchange_and_parked_spare',ranges.get('mag_01',{}).get('position_cm',0)>3 and ranges.get('mag_02',{}).get('position_cm',999)<.15,fatal=False,measured=ranges)
        else:
            check(label+'_both_magazine_copies_move',all(b in ranges and ranges[b]['position_cm']>3 for b in ['mag_01','mag_02']),fatal=False,measured=ranges)
    return summary
def inspect_press():
    key=u.Key();key.import_text('(KeyName="I")')
    call(pawn,'InpActEvt_Inspect Weapon_K2Node_InputActionEvent_0',key)
def capture(label,aim=False):
    file=OUT/(label+'.png');old=file.stat().st_mtime_ns if file.exists() else 0
    R['captures'].append({'label':label,'file':str(file),'game_time':now(),
        'montage':path(montage()),'weapon_clip_time':mesh().get_position(),
        'hand_clip_time':hands.get_anim_instance().montage_get_position(montage()) if montage() else None})
    command('HighResShot 1600x900 filename="'+file.as_posix()+'"')
    for tick in wait(lambda:file.exists() and file.stat().st_mtime_ns>old and file.stat().st_size>24,40):
        if aim:inject('IA_Aim',1)
        yield tick
    check('capture_'+label,struct.unpack('>II',file.read_bytes()[16:24])==(1600,900))
def run():
    global world,pawn,pc,inventory,hands,inputs,phase,probe,mp7_probe
    saved=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())).resolve()
    check('isolated_save_directory',saved.is_relative_to((P/'qa_user').resolve()),directory=str(saved))
    check('map_loaded',level.load_level(MAP))
    gm=u.load_class(None,GM);check('isolated_game_mode_exists',bool(gm))
    editor.get_editor_world().get_world_settings().set_editor_property('default_game_mode',gm)
    level.editor_request_begin_play()
    def ready():
        global world,pawn,pc
        world=editor.get_game_world()
        if world:pawn=u.GameplayStatics.get_player_pawn(world,0);pc=u.GameplayStatics.get_player_controller(world,0)
        return pawn and pc
    phase='spawn';yield from wait(ready,120);yield from seconds(2)
    if 'SpectatorPawn' in path(pawn.get_class()):
        call(pc,'Spawn');yield from wait(lambda:ready() and 'BP_PlayerCharacter_Metahuman' in path(pawn.get_class()),90)
    check('oskar_possessed','BP_PlayerCharacter_Metahuman' in path(pawn.get_class()))
    for name in ('WB_ChooseLoadout_Menu','WB_LoadoutSelection'):
        wc=u.load_class(None,'/Game/FPS_Controller/UI/Loadout/'+name+'.'+name+'_C')
        if wc:
            for widget in u.WidgetLibrary.get_all_widgets_of_class(world,wc,False):widget.remove_from_parent()
    u.WidgetLibrary.set_input_mode_game_only(pc);pc.set_editor_property('show_mouse_cursor',False)
    inventory=pawn.get_component_by_class(u.load_class(None,'/Game/FPS_Controller/Blueprints/Components/AC_InventorySystem.AC_InventorySystem_C'))
    hands=next(c for c in pawn.get_components_by_class(u.SkeletalMeshComponent) if c.get_name()=='Mesh_FPP')
    library=u.get_default_object(u.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
    inputs=call(library,'GetLocalPlayerSubSystemFromPlayerController',pc,u.EnhancedInputLocalPlayerSubsystem.static_class())
    command('r.SetRes 1600x900w');command('r.HighResScreenshotDelay 2')
    yield from seconds(8)
    if call(pc,'IsUsingTPP'):yield from action('IA_TogglePerspective');yield from seconds(2)
    select='IA_SecondaryWeapon' if SIDEARM else'IA_PrimaryWeapon'
    if not is_selected():yield from action(select)
    yield from seconds(3)
    check('new_weapon_equipped',weapon() and ('BP_'+GUN) in weapon().get_class().get_name(),weapon=path(weapon()))
    # Resolve streaming/compilation once while idle, before any active-clip trace.
    phase='finish_asset_loading'
    u.AutomationUtilsBlueprintLibrary.finish_all_asset_compilation()
    u.AutomationLibrary.finish_loading_before_screenshot()
    yield from seconds(2)
    R['asset_loading_barrier']={'completed':True,'game_time':now(),'settle_game_seconds':2}
    R['components']=[{'name':c.get_name(),'mesh':path(prop(c,'skeletal_mesh_asset')),'socket':str(c.get_attach_socket_name()),
        'location':str(prop(c,'relative_location')),'rotation':str(prop(c,'relative_rotation'))}for c in weapon().get_components_by_class(u.SkeletalMeshComponent)]
    R['materials']=[path(mesh().get_material(i))for i in range(mesh().get_num_materials())]
    check('owned_materials',R['materials'] and all(p and ('/Arsenal/'+GUN+'/')in p for p in R['materials']))
    probe=GameplayProbe(GUN,pawn,hands,weapon(),world,OUT)
    R['pawn_skeletal_components']=probe.components()
    R['pose_reference']={'source':probe.expected_source,'error':probe.reference_error,'bones':probe.bones}
    if BOLT_GUN or PISTOL or INTEGRATED_MAG or GUN=='SwitchKnife':
        check('idle_reference_evaluated',probe.reference_error is None,fatal=False,error=probe.reference_error)
        ready_pose('initial_mechanism_matches_authored_idle')
    R['glove_geometry_idle']=probe.glove_snapshot('Idle')
    if GUN=='SwitchKnife':switchknife_idle_delivery('initial')
    if BOLT_GUN:
        ballista_idle_delivery('initial')
        layer_report=Path(QA_CONFIG['layer_report']) if 'layer_report' in QA_CONFIG else (P.parents[2]/'outputs/bordertown_arsenal/Ballista/FPP_Layer_Integration.json' if GUN=='Ballista' else OUT/'FPP_Layer_Integration.json')
        layer_data=json.loads(layer_report.read_text()) if layer_report.is_file() else {}
        check('ballista_ads_grip_prepared',layer_data.get('ads_grip_corrected') is True,
              fatal=False,report=str(layer_report),policy=layer_data.get('ads_policy'),
              note='A corrected neutral pose with unchanged donor ADS is not a completed grip fix.')
    if MAG_ACTOR:
        mp7_probe=MP7AttachmentProbe(weapon(),hands,world,OUT,GUN)
        check('mp7_magazine_idle_reference_evaluated',mp7_probe.reference_error is None,fatal=False,error=mp7_probe.reference_error)
        mp7_attachments('initial');mp7_ready('mp7_initial_both_magazine_copies_idle')
    phase='idle';yield from capture('FirstPerson')
    if CAPTURE_ONLY:
        assert GUN=='Ballista','Pose-only collection is currently scoped to Ballista.'
        command('r.SetNearClipPlane 1');yield from seconds(.3)
        yield from capture('ClippingDiagnostic_Near1cm')
        command('r.SetNearClipPlane 5');yield from seconds(.3)
        yield from action('IA_Aim',1.5);inject('IA_Aim',1);yield
        R['glove_geometry_ads']=probe.glove_snapshot('ADS')
        yield from capture('ADS',True);inject('IA_Aim',0)
        R['status']='reference_poses_captured_not_gameplay_validated';checkpoint();return
    if GUN=='SwitchKnife':
        probe.begin('Switch_Inspect','Inspect');inspect_press()
        yield from seconds(.45);yield from capture('Inspect');yield from wait(lambda:montage()is None,15);yield from seconds(.25)
        paired_result('Switch_Inspect');ready_pose('inspect_returns_blade_open');switchknife_idle_delivery('after_inspect')
        phase='inspect_attack_interrupt';probe.begin('Switch_AttackInterrupt','Inspect');inspect_press();yield from seconds(.3)
        yield from action('IA_FireWeapon');yield from seconds(.18);yield from capture('Attack');yield from seconds(2)
        paired_result('Switch_AttackInterrupt');ready_pose('attack_interrupt_returns_blade_open')
        phase='inspect_switch_interrupt';probe.begin('Switch_SwitchInterrupt','Inspect');inspect_press();yield from seconds(.3)
        yield from action('IA_PrimaryWeapon');yield from wait(lambda:not is_selected(),8);yield from seconds(.4)
        # SwitchKnife has an authored closing unequip; hidden closed is valid.
        # Re-equipping must still restore the open idle, checked below.
        R['switch_hidden_after_unequip']={'mechanisms':probe.pose(),'note':'Closing unequip is allowed; open baseline is enforced after equip.'}
        yield from action(select);yield from wait(is_selected,8);yield from seconds(3)
        paired_result('Switch_SwitchInterrupt');ready_pose('inspect_switch_reequip_blade_open');switchknife_idle_delivery('after_inspect_interrupt')
    else:
        capacity=int(call(weapon(),'GetAmmoPerMag'));R['capacity']=capacity
        check('positive_capacity',capacity>0)
        if INTEGRATED_MAG:
            phase='inspect';probe.begin(GUN+'_Inspect','Inspect');inspect_press()
            yield from seconds(.5);yield from capture('Inspect');yield from wait(lambda:montage()is None,15);yield from seconds(.2)
            paired_result(GUN+'_Inspect');ready_pose('inspect_restores_integrated_mechanisms')
        before=pc.player_camera_manager.get_fov_angle()
        yield from action('IA_Aim',1.2)
        # Keep aim injected while the screenshot is captured.
        inject('IA_Aim',1);yield
        phase='ads';check('ads_zoom',pc.player_camera_manager.get_fov_angle()<before-2)
        R['glove_geometry_ads']=probe.glove_snapshot('ADS')
        yield from capture('ADS',True)
        # HighResShot and first-use effects can stall longer than a short SMG
        # shot. Warm that path, then measure a separate shot with no file work.
        if INTEGRATED_MAG:
            for tick in seconds(.35):inject('IA_Aim',1);yield tick
            end=now()+.07
            while now()<end:inject('IA_Aim',1);inject('IA_FireWeapon',1);yield
            inject('IA_FireWeapon',0)
            for tick in seconds(.65):inject('IA_Aim',1);yield tick
        aimed_before=int(call(weapon(),'GetCurrentAmmo'))
        if BOLT_GUN:probe.begin(GUN+'_AimedFire','FireBoltADS')
        if INTEGRATED_MAG:probe.begin(GUN+'_AimedFire','FireADS')
        end=now()+.07
        while now()<end:inject('IA_Aim',1);inject('IA_FireWeapon',1);yield
        inject('IA_FireWeapon',0)
        for tick in seconds(1.95 if BOLT_GUN else .9):inject('IA_Aim',1);yield tick
        check('aimed_shot_consumes_ammo',int(call(weapon(),'GetCurrentAmmo'))<aimed_before)
        if BOLT_GUN:
            trace=paired_result(GUN+'_AimedFire')
            check('aimed_bolt_cycles_in_aiming_space',trace['mechanism_ranges'][BOLT_SLIDE]['position_cm']>5 and trace['mechanism_ranges'][BOLT_ROTATE]['angle_deg']>40,fatal=False,measured=trace['mechanism_ranges'])
        if INTEGRATED_MAG:
            trace=paired_result(GUN+'_AimedFire')
            check('aimed_fire_keeps_weapon_near_aim_pose',trace['max_weapon_component_travel_cm']<8.,fatal=False,measured_cm=trace['max_weapon_component_travel_cm'],limit_cm=8.,note='Stationary aimed-fire smoke gate for a gross procedural/montage-space discontinuity; fine recoil polish is visually reviewed separately.')
        yield from capture('ADSAfireRecovery',True)
        inject('IA_Aim',0);yield from seconds(.6)
        before=int(call(weapon(),'GetCurrentAmmo'))
        phase='fire'
        if BOLT_GUN:probe.begin(GUN+'_Fire','FireBolt')
        if PISTOL:probe.begin(GUN+'_Fire','Fire')
        yield from action('IA_FireWeapon',.07)
        # Complete the diagnostic shot without screenshot/file work in its active
        # interval, so stalls cannot make a fixed bolt sample miss the full cycle.
        yield from seconds(1.9 if BOLT_GUN else.35)
        after=int(call(weapon(),'GetCurrentAmmo'));check('firing_consumes_ammo',0<before-after<=capacity,before=before,after=after)
        yield from seconds(.4)
        check('firing_stops_on_release',int(call(weapon(),'GetCurrentAmmo'))==after,fatal=False)
        if PISTOL:
            trace=paired_result(GUN+'_Fire')
            check('m1911_slide_cycles',trace['mechanism_ranges'].get('slide',{}).get('position_cm',0)>.5,fatal=False,measured=trace['mechanism_ranges'])
            yield from wait(lambda:montage()is None,4);yield from seconds(.1)
            ready_pose('m1911_loaded_slide_returns_closed')
        if BOLT_GUN:
            trace=paired_result(GUN+'_Fire');travel=trace['mechanism_ranges'][BOLT_SLIDE];rotation=trace['mechanism_ranges'][BOLT_ROTATE]
            check('actual_runtime_bolt_cycle',travel['position_cm']>5 and rotation['angle_deg']>40,fatal=False,measured=trace['mechanism_ranges'])
            ready_pose('shot_returns_bolt_closed_mag_seated')
            yield from action('IA_FireWeapon',.07);yield from seconds(.8)
        yield from capture('Fire');yield from seconds(2)
        phase='tactical_reload'
        if BOLT_GUN:probe.begin(GUN+'_TacticalReload','TacticalReload')
        if INTEGRATED_MAG:probe.begin(GUN+'_TacticalReload','Reload' if GUN=='UZI' else 'TacticalReload')
        if MAG_ACTOR:mp7_probe.begin(GUN+'_TacticalReload','TacticalReload')
        yield from action('IA_Reload');yield from seconds(.5)
        check('tactical_reload_montage',bool(montage()),montage=path(montage()))
        yield from capture('TacticalReload');yield from seconds(6)
        check('tactical_refill',int(call(weapon(),'GetCurrentAmmo'))==capacity)
        if MAG_ACTOR:
            mp7_trace_result(GUN+'_TacticalReload');mp7_ready('mp7_tactical_both_magazine_copies_reset')
        if INTEGRATED_MAG:
            trace=paired_result(GUN+'_TacticalReload')
            check('integrated_tactical_magazine_moves',trace['mechanism_ranges'].get(MAG_BONE,{}).get('position_cm',0)>5,fatal=False,measured=trace['mechanism_ranges'])
            ready_pose('integrated_tactical_magazine_reseated')
        if BOLT_GUN:
            trace=paired_result(GUN+'_TacticalReload');ranges=trace['mechanism_ranges']
            check('tactical_magazine_moves',ranges[MAG_BONE]['position_cm']>15,fatal=False,measured=ranges[MAG_BONE])
            check('tactical_bolt_stays_closed',ranges[BOLT_SLIDE]['position_cm']<.2 and ranges[BOLT_ROTATE]['angle_deg']<2,fatal=False,measured={'slide':ranges[BOLT_SLIDE],'rotation':ranges[BOLT_ROTATE]})
            ready_pose('tactical_reload_returns_mag_seated')
        # The pack can start empty reload automatically after the final shot.
        # Begin before emptying so its early magazine motion is not omitted.
        if BOLT_GUN:probe.begin(GUN+'_EmptyReload','Reload')
        if INTEGRATED_MAG:probe.begin(GUN+'_EmptyReload','Reload_Empty' if GUN=='UZI' else 'Reload')
        if MAG_ACTOR:mp7_probe.begin(GUN+'_EmptyReload','Reload')
        phase='empty_magazine';deadline=now()+capacity*3+10
        while int(call(weapon(),'GetCurrentAmmo'))>0:
            if now()>deadline:raise TimeoutError('cannot empty magazine by firing')
            yield from action('IA_FireWeapon',.10 if BOLT_GUN else .35)
            yield from seconds(1.7 if BOLT_GUN else .1)
        check('magazine_empty',int(call(weapon(),'GetCurrentAmmo'))==0)
        automatic_reload_seen=bool(montage() and 'reload' in path(montage()).lower())
        if PISTOL:
            yield from seconds(.2)
            check('m1911_empty_slide_locks_back',probe.mechanism_delta('slide')['position_cm']>1.,fatal=False,measured=probe.mechanism_delta('slide'))
            yield from capture('EmptySlideLocked')
        yield from seconds(2)
        phase='empty_reload'
        automatic_reload=bool(montage() and 'reload' in path(montage()).lower())
        automatic_reload_seen=automatic_reload_seen or any('reload' in h['montage'].lower() and 'tactical' not in h['montage'].lower() for h in R.get('observed_empty_reload_montages',[]))
        automatic_reload_completed=automatic_reload_seen and int(call(weapon(),'GetCurrentAmmo'))==capacity
        R['empty_reload_started_automatically']=automatic_reload or automatic_reload_seen
        R['empty_reload_completed_before_capture']=automatic_reload_completed
        if not automatic_reload and not automatic_reload_completed:yield from action('IA_Reload')
        yield from seconds(.5)
        check('empty_reload_montage',bool(montage())or automatic_reload_completed,montage=path(montage()),automatic_reload_completed=automatic_reload_completed)
        yield from capture('EmptyReload');yield from seconds(6)
        check('empty_refill',int(call(weapon(),'GetCurrentAmmo'))==capacity)
        if PISTOL:ready_pose('pistol_reload_closes_slide')
        if INTEGRATED_MAG:
            trace=paired_result(GUN+'_EmptyReload')
            check('integrated_empty_magazine_moves',trace['mechanism_ranges'].get(MAG_BONE,{}).get('position_cm',0)>5,fatal=False,measured=trace['mechanism_ranges'])
            ready_pose('integrated_empty_magazine_reseated')
            phase='integrated_reload_switch_interrupt'
            yield from action('IA_FireWeapon',.07);yield from seconds(.6)
            probe.begin(GUN+'_ReloadInterrupt','Reload' if GUN=='UZI' else 'TacticalReload');yield from action('IA_Reload')
            yield from wait(lambda:probe.mechanism_delta(MAG_BONE)['position_cm']>3 or montage()is None,6)
            check('interrupt_during_integrated_magazine_movement',probe.mechanism_delta(MAG_BONE)['position_cm']>3,fatal=False)
            yield from action('IA_SecondaryWeapon');yield from wait(lambda:not is_selected(),8);yield from seconds(.4)
            yield from action(select);yield from wait(is_selected,8);yield from seconds(3)
            paired_result(GUN+'_ReloadInterrupt');ready_pose('integrated_interrupt_reequip_seated');yield from capture('ReloadInterruptedReequipped')
        if MAG_ACTOR:
            mp7_trace_result(GUN+'_EmptyReload');mp7_ready('mp7_empty_both_magazine_copies_reset')
            phase='mp7_reload_switch_interrupt'
            yield from action('IA_FireWeapon',.07);yield from seconds(.6)
            mp7_probe.begin(GUN+'_ReloadInterrupt','TacticalReload');yield from action('IA_Reload')
            yield from wait(lambda:mp7_probe.displaced()>3 or montage()is None,6)
            check('mp7_interrupt_during_actual_magazine_movement',mp7_probe.displaced()>3,fatal=False,position_cm=mp7_probe.displaced())
            yield from action('IA_PrimaryWeapon' if SIDEARM else 'IA_SecondaryWeapon');yield from wait(lambda:not is_selected(),8);yield from seconds(.4)
            mp7_ready('mp7_hidden_interrupt_both_magazine_copies_reset')
            yield from action(select);yield from wait(is_selected,8);yield from seconds(3)
            mp7_trace_result(GUN+'_ReloadInterrupt',False);mp7_attachments('interrupt_reequipped');mp7_ready('mp7_interrupt_reequip_both_magazine_copies_reset')
            yield from capture('ReloadInterruptedReequipped')
        if BOLT_GUN:
            trace=paired_result(GUN+'_EmptyReload');ranges=trace['mechanism_ranges']
            check('empty_reload_magazine_and_bolt_move',ranges[MAG_BONE]['position_cm']>15 and ranges[BOLT_SLIDE]['position_cm']>7 and ranges[BOLT_ROTATE]['angle_deg']>45,
                  fatal=False,measured=ranges)
            ready_pose('empty_reload_returns_bolt_closed_mag_seated')
            phase='reload_switch_interrupt'
            yield from action('IA_FireWeapon',.07);yield from seconds(2)
            probe.begin(GUN+'_ReloadInterrupt','TacticalReload');yield from action('IA_Reload')
            yield from wait(lambda:probe.mechanism_delta(MAG_BONE)['position_cm']>3 or montage()is None,6)
            moved=probe.mechanism_delta(MAG_BONE)['position_cm']
            check('reload_interrupted_while_magazine_out',moved>3,fatal=False,magazine_distance_cm=moved)
            yield from action('IA_PrimaryWeapon' if SIDEARM else 'IA_SecondaryWeapon');yield from wait(lambda:not is_selected(),8);yield from seconds(.4)
            ready_pose('hidden_interrupted_ballista_resets_ready')
            yield from action(select);yield from wait(is_selected,8);yield from seconds(3)
            paired_result(GUN+'_ReloadInterrupt');ready_pose('reload_interrupt_reequips_seated')
            yield from capture('ReloadInterruptedReequipped')
    phase='switch_and_return'
    yield from action('IA_PrimaryWeapon'if SIDEARM else'IA_SecondaryWeapon');yield from seconds(2)
    check('switched_away',weapon() and ('BP_'+GUN)not in weapon().get_class().get_name())
    yield from action(select);yield from seconds(3)
    check('reequipped',weapon() and ('BP_'+GUN)in weapon().get_class().get_name())
    if BOLT_GUN or PISTOL or INTEGRATED_MAG or GUN=='SwitchKnife':ready_pose('final_reequip_matches_idle_mechanisms')
    if BOLT_GUN:ballista_idle_delivery('reequipped')
    if GUN=='SwitchKnife':switchknife_idle_delivery('reequipped')
    if MAG_ACTOR:mp7_attachments('final_reequipped');mp7_ready('mp7_final_reequip_both_magazine_copies_reset')
    yield from capture('Reequipped')
    phase='third_person';yield from action('IA_TogglePerspective');yield from seconds(1.5)
    check('third_person_enabled',bool(call(pc,'IsUsingTPP')),fatal=False)
    R['third_person_weapon_components']=[{'name':c.get_name(),'asset':path(prop(c,'skeletal_mesh_asset')),'visible':prop(c,'visible'),
        'world':str(c.get_world_transform()),'parent':path(c.get_attach_parent()),'socket':str(c.get_attach_socket_name())}for c in weapon().get_components_by_class(u.SkeletalMeshComponent)]
    yield from capture('ThirdPerson')
    yield from action('IA_TogglePerspective');yield from seconds(1.)
    check('first_person_restored',not bool(call(pc,'IsUsingTPP')),fatal=False)
    R['status']='runtime_checks_passed_pending_visual_review'if all(x['passed']for x in R['checks'])else'runtime_checks_failed';checkpoint()

generator=run()
def tick(delta):
    global busy,ending
    if busy:return
    busy=True
    try:
        if ending is not None:
            if time.monotonic()>ending:
                u.unregister_slate_post_tick_callback(handle);u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor()
            return
        try:
            if probe:probe.sample()
            if mp7_probe:mp7_probe.sample()
            if phase in ('empty_magazine','empty_reload') and hands:
                active=montage()
                if active and 'reload' in path(active).lower():
                    history=R.setdefault('observed_empty_reload_montages',[])
                    if not history or history[-1]['montage']!=path(active):history.append({'montage':path(active),'game_time':now()})
            next(generator)
        except StopIteration:
            level.editor_request_end_play();ending=time.monotonic()+2
        except Exception:
            R.update(status='failed',error=traceback.format_exc());checkpoint()
            level.editor_request_end_play();ending=time.monotonic()+2
    finally:busy=False
u.EditorPythonScripting.set_keep_python_script_alive(True)
handle=u.register_slate_post_tick_callback(tick)
