"""Append validated expansion choices, preserving existing loadouts and selection."""
import sys,json,hashlib,shutil,datetime,traceback,re
from pathlib import Path
P=Path(__file__).resolve().parent
sys.path.insert(0,str(P))
from ue_expansion_common import *
from ue_common import cls
from public_paths import PROJECT
choice=re.search(r'-ExpansionPromote=([A-Za-z0-9,]+)',u.SystemLibrary.get_command_line())
assert choice,'Specify the exact validated weapons with -ExpansionPromote.'
GUNS=choice.group(1).split(',')
assert GUNS and len(GUNS)==len(set(GUNS)) and set(GUNS)<= {'Ballista','MP7','SwitchKnife','M1911','PGM','Pistol9mm','TalonPistol','UZI','AR15','MP5'},GUNS
R={'status':'running','preserved_existing_rows':True,'backups':[]}
stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
BACKUP=P/'backups'/('before_expansion_'+stamp)
def checkpoint():(OUT/'Loadout_Promotion.json').write_text(json.dumps(R,indent=2,default=str))
def snapshot(o):
    return o.export_text() if hasattr(o,'export_text') else str(o)
def backup_file(file,relative):
    target=BACKUP/relative;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(file,target);assert hashlib.sha256(file.read_bytes()).digest()==hashlib.sha256(target.read_bytes()).digest()
    R['backups'].append(str(target))
def backup_asset(path):backup_file(PROJECT/'Content'/(path.removeprefix('/Game/')+'.uasset'),Path('Content')/(path.removeprefix('/Game/')+'.uasset'))
def field(row,stem):return struct_field(row,stem)
def absent_rejected(rows):
    return all('/Arsenal/M40A3/'not in row.export_text()for row in rows)
def key(row):return tuple(row.get_editor_property(field(row,s)).get_path_name()for s in ['PrimaryWeapon','SecondaryWeapon'])
def append_unique(existing,new):
    rows=list(existing);keys={key(row)for row in rows}
    for row in new:
        if key(row)not in keys:rows.append(row.copy());keys.add(key(row))
    return rows
try:
    for gun in GUNS:
        evidence=json.loads((OUT/gun/'Gameplay_Validation.json').read_text())
        assert evidence['status']=='ready_for_user_test',(gun,evidence['status'])
        assert evidence.get('visual_review_passed')is True,gun
        assert all(x['passed']for x in evidence['checks']),gun
    saved=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())).resolve()
    assert saved.is_relative_to((ROOT/'work/arsenal/play_user').resolve()),str(saved)
    catalog_path='/Game/FPS_Controller/Blueprints/DataAssets/O_WeaponList'
    controller_path=DEST+'/BP_ArsenalController'
    backup_asset(catalog_path);backup_asset(controller_path)
    savefile=saved/'SaveGames/SaveSlot_01.sav'
    if savefile.is_file():backup_file(savefile,Path('SaveGames/SaveSlot_01.sav'))
    checkpoint()
    bp=asset(controller_path);lc=components(bp)['LoadoutComponent']
    existing=list(lc.get_editor_property('PlayerLoadouts'));assert existing and absent_rejected(existing)
    oldtexts=[x.export_text()for x in existing];oldindex=lc.get_editor_property('SelectedLoadoutIndex')
    R['previous_controller_count']=len(existing);R['previous_selected_index']=oldindex
    candidates=[];das={gun:asset(DEST+'/'+gun+'/DA_Weapon_'+gun)for gun in GUNS}
    ak=asset('/Game/FPS_Controller/Blueprints/DataAssets/WeaponsData/DA_Weapon_Ak')
    butterfly=asset(DEST+'/Butterfly/DA_Weapon_Butterfly')
    karambit=asset('/Game/BorderTownWeapons/FlinkyKarambit/DA_Weapon_FlinkyKarambit')
    for gun in GUNS:
        main,side=(ak,das[gun]) if gun in ['SwitchKnife','M1911','Pistol9mm','TalonPistol'] else (das[gun],butterfly if gun in ['MP7','UZI','MP5'] else karambit)
        row=existing[0].copy()
        for stem,value in [('PrimaryWeapon',main),('SecondaryWeapon',side),('PrimaryWeaponSkin',None),('SecondaryWeaponSkin',None)]:row.set_editor_property(field(row,stem),value)
        # Each new weapon supplies its own deliberately configured magazine and
        # grip defaults. Never inherit a donor row's attachment overrides.
        row.set_editor_property(field(row,'PrimaryWeaponAttachments'),main.get_editor_property('DefaultAttachments').copy())
        row.set_editor_property(field(row,'SecondaryWeaponAttachments'),side.get_editor_property('DefaultAttachments').copy())
        candidates.append(row)
    rows=append_unique(existing,candidates)
    assert [x.export_text()for x in rows[:len(existing)]]==oldtexts
    lc.set_editor_property('PlayerLoadouts',rows)
    lc.set_editor_property('MaxLoadouts',max(int(lc.get_editor_property('MaxLoadouts')),len(rows)))
    assert lc.get_editor_property('SelectedLoadoutIndex')==oldindex
    compile_save(bp)
    catalog=asset(catalog_path);cdo=u.get_default_object(catalog.generated_class())
    oldcatalog=list(cdo.get_editor_property('Weapons'));assert all('/Arsenal/M40A3/'not in x.get_path_name()for x in oldcatalog)
    newcatalog=list(oldcatalog)
    for da in das.values():
        if da not in newcatalog:newcatalog.append(da)
    cdo.set_editor_property('Weapons',newcatalog);compile_save(catalog)
    R['catalog']=[x.get_path_name()for x in newcatalog]
    R['controller_count']=len(rows)
    if savefile.is_file():
        game=u.GameplayStatics.load_game_from_slot('SaveSlot_01',0);assert game
        savebp=asset(game.get_class().get_path_name().split('.')[0])
        before={};changed=[];expected={}
        for full in u.BlueprintEditorLibrary.list_member_variable_names(savebp,True):
            name=full.rsplit('.',1)[-1]
            try:value=game.get_editor_property(name)
            except Exception:continue
            before[name]=snapshot(value)
            if isinstance(value,(u.Array,list,tuple)) and value and hasattr(value[0],'export_text') and 'PrimaryWeapon_'in value[0].export_text():
                assert absent_rejected(value)
                original=[r.export_text()for r in value];new=append_unique(value,candidates)
                assert [r.export_text()for r in new[:len(original)]]==original
                value.clear()
                for row in new:value.append(row)
                expected[name]=[r.export_text()for r in new];changed.append(name)
        assert changed,'No saved loadout array found; refusing to reset profile'
        assert u.GameplayStatics.save_game_to_slot(game,'SaveSlot_01',0)
        reloaded=u.GameplayStatics.load_game_from_slot('SaveSlot_01',0);assert reloaded
        for name,old in before.items():
            value=reloaded.get_editor_property(name)
            if name in changed:assert [r.export_text()for r in value]==expected[name]
            else:assert snapshot(value)==old,('Unrelated saved field changed',name)
        R['saved_profile']={'changed_arrays':changed,'all_other_fields_preserved':True,'counts':{n:len(v)for n,v in expected.items()}}
    R.update(status='appended_verified_choices',new_presets=[{'primary':key(r)[0],'secondary':key(r)[1]}for r in candidates])
    checkpoint()
except Exception:
    R.update(status='failed',error=traceback.format_exc());checkpoint();raise
