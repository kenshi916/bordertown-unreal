"""Shared import helpers for the isolated BorderTown arsenal assets."""
import unreal as u
from pathlib import Path

from public_paths import WORKSPACE as ROOT
OUT=ROOT/'outputs/bordertown_arsenal'
DEST='/Game/BorderTownWeapons/Arsenal'
MANNY='/Game/FPS_Controller/Characters/Mannequins/Meshes/SK_Mannequin'

def cls(p):
    return u.load_class(None,p+'.'+p.rsplit('/',1)[-1]+'_C')

def asset(p):
    obj=u.load_asset(p)
    assert obj,p
    return obj

def save(obj):
    assert u.EditorAssetLibrary.save_loaded_asset(obj,False),str(obj)
    return obj

def compile_save(bp):
    assert u.BlueprintEditorLibrary.compile_blueprint(bp),str(bp)
    return save(bp)

def duplicate(src,dst):
    return u.load_asset(dst) or u.EditorAssetLibrary.duplicate_asset(src,dst)

def fbx(filename,directory,name,skeleton=None,animation=False):
    u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    ui=u.FbxImportUI()
    ui.set_editor_properties(dict(import_as_skeletal=True,import_mesh=not animation,
        import_animations=animation,import_materials=False,import_textures=False,
        create_physics_asset=False,automated_import_should_detect_type=False,
        override_full_name=True,
        mesh_type_to_import=u.FBXImportType.FBXIT_ANIMATION if animation else u.FBXImportType.FBXIT_SKELETAL_MESH))
    if skeleton:ui.set_editor_property('skeleton',skeleton)
    data=ui.get_editor_property('anim_sequence_import_data' if animation else 'skeletal_mesh_import_data')
    data.set_editor_properties(dict(convert_scene=True,force_front_x_axis=False,convert_scene_unit=True))
    if animation:
        data.set_editor_properties(dict(import_bone_tracks=True,animation_length=u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
            use_default_sample_rate=False,custom_sample_rate=30,remove_redundant_keys=False))
    else:
        data.set_editor_properties(dict(update_skeleton_reference_pose=False,use_t0_as_ref_pose=False,
            import_morph_targets=False,normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS))
    task=u.AssetImportTask();task.filename=str(filename);task.destination_path=directory
    task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=False
    task.options=ui;task.factory=u.FbxFactory()
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    obj=u.load_asset(directory+'/'+name)
    if not obj:
        objects=[u.load_asset(p) for p in task.imported_object_paths]
        objects=[o for o in objects if isinstance(o,u.AnimSequence if animation else u.SkeletalMesh)]
        assert len(objects)==1,list(task.imported_object_paths)
        obj=objects[0]
        assert u.EditorAssetLibrary.rename_asset(obj.get_path_name(),directory+'/'+name)
        obj=asset(directory+'/'+name)
    save(obj)
    return obj

def texture(filename,directory,name,normal=False,flip_green=False,mask=False):
    obj=u.load_asset(directory+'/'+name)
    if not obj:
        t=u.AssetImportTask();t.filename=str(filename);t.destination_path=directory;t.destination_name=name
        t.automated=True;t.replace_existing=True;t.save=False
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);obj=asset(directory+'/'+name)
    obj.set_editor_property('srgb',not(normal or mask))
    if normal:
        obj.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
        obj.set_editor_property('flip_green_channel',flip_green)
    elif mask:obj.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
    return save(obj)

def montage(sequence,directory,name,weapon_animation=None,reload_time=None):
    factory=u.AnimMontageFactory();factory.target_skeleton=sequence.get_editor_property('skeleton');factory.source_animation=sequence
    m=u.load_asset(directory+'/'+name) or u.AssetToolsHelpers.get_asset_tools().create_asset(name,directory,u.AnimMontage,factory)
    assert m
    # Refresh the single segment on reruns, preserving the ordinary pack DefaultSlot.
    tracks=m.get_editor_property('slot_anim_tracks')
    assert len(tracks)==1
    track=tracks[0];track.set_editor_property('slot_name','DefaultSlot');animtrack=track.anim_track
    segments=animtrack.anim_segments;segment=segments[0]
    segment.set_editor_properties(dict(anim_reference=sequence,anim_start_time=0.,anim_end_time=sequence.sequence_length,cached_play_length=sequence.sequence_length,anim_play_rate=1.,looping_count=1))
    animtrack.set_editor_property('anim_segments',[segment]);track.set_editor_property('anim_track',animtrack);m.set_editor_property('slot_anim_tracks',[track])
    for field,duration in [('blend_in',.03),('blend_out',.05)]:
        blend=m.get_editor_property(field);blend.set_editor_property('blend_time',duration);m.set_editor_property(field,blend)
    u.AnimationLibrary.remove_all_animation_notify_tracks(m)
    u.AnimationLibrary.add_animation_notify_track(m,'Arsenal')
    if weapon_animation:
        notify=u.AnimationLibrary.add_animation_notify_event(m,'Arsenal',0.001,cls('/Game/FPS_Controller/Animations/BlueprintsNotifies/BN_PlayWeaponAnimation'))
        notify.set_editor_property('AnimationToPlay',weapon_animation);notify.set_editor_property('TPP',False)
    if reload_time is not None:
        notify=u.AnimationLibrary.add_animation_notify_event(m,'Arsenal',reload_time,u.AnimNotify_PlayMontageNotify)
        notify.set_editor_property('notify_name','Reload')
    save(m)
    assert abs(m.sequence_length-sequence.sequence_length)<.05,(m.sequence_length,sequence.sequence_length)
    return m

def components(bp):
    sub=u.get_engine_subsystem(u.SubobjectDataSubsystem);lib=u.SubobjectDataBlueprintFunctionLibrary
    found={}
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        data=lib.get_data(h);name=str(lib.get_variable_name(data));obj=lib.get_object_for_blueprint(data,bp)
        if obj and name not in found:found[name]=obj
    return found

def weapon_meshes(bp,mesh,location=None,rotation=None):
    cs=components(bp)
    for name in ['MeshFPP','MeshTPP']:
        c=cs[name]
        assert c.get_path_name().startswith(bp.get_path_name().split('.')[0]+'.'),str(c)
        c.set_skeletal_mesh_asset(mesh);c.set_editor_property('override_materials',[])
        if location is not None:c.set_editor_property('relative_location',u.Vector(*location))
        if rotation is not None:c.set_editor_property('relative_rotation',u.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]))
    return cs
