"""Helpers restricted to newly owned expansion assets. No execution on import."""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import unreal as u
from ue_common import asset, save, compile_save, duplicate, components, weapon_meshes

ROOT = HERE.parents[2]
OUT = ROOT / 'outputs/bordertown_arsenal/Expansion'
DEST = '/Game/BorderTownWeapons/Arsenal'
OUT.mkdir(parents=True, exist_ok=True)


def owned(path):
    assert any(str(path).startswith(DEST + '/' + gun + '/')
               for gun in ('Ballista', 'MP7', 'SwitchKnife', 'PGM', 'M1911', 'UZI', 'AR15', 'Cobalt', 'MP5', 'Pistol9mm', 'TalonPistol', 'BarrettM82', 'Vector', 'MCXSpearLT', 'HoneyBadger', 'MP7A1', 'DJMSniper')), path


def import_fbx(filename, directory, name, skeleton=None, fps=60):
    """Import a mesh or exact-rate animation without modifying donor skeletons."""
    owned(directory + '/' + name)
    assert Path(filename).is_file(), filename
    animation = skeleton is not None
    u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    ui = u.FbxImportUI()
    ui.set_editor_properties(dict(import_as_skeletal=True, import_mesh=not animation,
        import_animations=animation, import_materials=False, import_textures=False,
        create_physics_asset=False, automated_import_should_detect_type=False,
        override_full_name=True,
        mesh_type_to_import=u.FBXImportType.FBXIT_ANIMATION if animation else u.FBXImportType.FBXIT_SKELETAL_MESH))
    if skeleton:
        ui.set_editor_property('skeleton', skeleton)
    data = ui.get_editor_property('anim_sequence_import_data' if animation else 'skeletal_mesh_import_data')
    data.set_editor_properties(dict(convert_scene=True, force_front_x_axis=False, convert_scene_unit=True))
    if animation:
        data.set_editor_properties(dict(import_bone_tracks=True,
            animation_length=u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
            use_default_sample_rate=False, custom_sample_rate=fps, remove_redundant_keys=False))
    else:
        data.set_editor_properties(dict(update_skeleton_reference_pose=False, use_t0_as_ref_pose=False,
            import_morph_targets=False,
            normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS))
    task = u.AssetImportTask()
    task.set_editor_properties(dict(filename=str(filename), destination_path=directory,
        destination_name=name, automated=True, replace_existing=True, save=False,
        options=ui, factory=u.FbxFactory()))
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    result = u.load_asset(directory + '/' + name)
    if not result:
        candidates = [u.load_asset(p) for p in task.imported_object_paths]
        candidates = [x for x in candidates if isinstance(x, u.AnimSequence if animation else u.SkeletalMesh)]
        assert len(candidates) == 1, list(task.imported_object_paths)
        assert u.EditorAssetLibrary.rename_asset(candidates[0].get_path_name(), directory + '/' + name)
        result = asset(directory + '/' + name)
    if animation:
        result.set_editor_property('enable_root_motion', False)
        result.set_editor_property('allow_frame_stripping', False)
        target_rate = result.get_editor_property('platform_target_frame_rate')
        if not re.search(r'Default=\(Numerator=' + str(fps) + r',Denominator=1\)', target_rate.export_text()):
            target_rate.set_editor_property('default', u.FrameRate(numerator=fps, denominator=1))
            result.set_editor_property('platform_target_frame_rate', target_rate)
        assert re.search(r'Default=\(Numerator=' + str(fps) + r',Denominator=1\)',
                         result.get_editor_property('platform_target_frame_rate').export_text())
    return save(result)


def build_authored_materials(gun, mappings, normal_is_opengl=False):
    """Preserve source base/rough/metal/normal data, mapped explicitly by slot."""
    directory = DEST + '/' + gun
    owned(directory + '/Materials')
    lib = u.MaterialEditingLibrary
    at = u.AssetToolsHelpers.get_asset_tools()
    report = []
    materials = {}
    for mapping in mappings:
        slot = mapping['material']
        safe = re.sub(r'[^A-Za-z0-9_]', '_', slot)
        channels = mapping['files']
        textures = {}
        for channel in ('BaseColor', 'Roughness', 'Metallic', 'Normal'):
            filename = Path(channels[channel])
            assert filename.is_file(), filename
            name = 'T_' + gun + '_' + safe + '_' + channel
            path = directory + '/Textures/' + name
            tex = u.load_asset(path)
            if not tex:
                task = u.AssetImportTask()
                task.set_editor_properties(dict(filename=str(filename), destination_path=directory + '/Textures',
                    destination_name=name, automated=True, replace_existing=False, save=False))
                at.import_asset_tasks([task])
                tex = asset(path)
            tex.set_editor_property('srgb', channel == 'BaseColor')
            tex.set_editor_property('compression_settings',
                u.TextureCompressionSettings.TC_NORMALMAP if channel == 'Normal' else
                u.TextureCompressionSettings.TC_MASKS if channel in ('Roughness', 'Metallic') else
                u.TextureCompressionSettings.TC_DEFAULT)
            if channel == 'Normal':
                tex.set_editor_property('flip_green_channel', bool(normal_is_opengl))
            save(tex)
            textures[channel] = tex
        name = 'M_' + gun + '_' + safe
        material = u.load_asset(directory + '/Materials/' + name)
        if not material:
            material = at.create_asset(name, directory + '/Materials', u.Material, u.MaterialFactoryNew())
        lib.delete_all_material_expressions(material)
        material.set_editor_property('two_sided', False)
        for row, channel in enumerate(('BaseColor', 'Roughness', 'Metallic', 'Normal')):
            node = lib.create_material_expression(material, u.MaterialExpressionTextureSample, -500, row * 180)
            node.set_editor_property('texture', textures[channel])
            node.set_editor_property('sampler_type',
                u.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel == 'Normal' else
                u.MaterialSamplerType.SAMPLERTYPE_COLOR if channel == 'BaseColor' else
                u.MaterialSamplerType.SAMPLERTYPE_MASKS)
            prop = dict(BaseColor=u.MaterialProperty.MP_BASE_COLOR, Roughness=u.MaterialProperty.MP_ROUGHNESS,
                        Metallic=u.MaterialProperty.MP_METALLIC, Normal=u.MaterialProperty.MP_NORMAL)[channel]
            assert lib.connect_material_property(node, 'RGB' if channel in ('BaseColor', 'Normal') else 'R', prop)
        lib.set_material_usage(material, u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
        lib.recompile_material(material)
        save(material)
        materials[slot] = material
        report.append(dict(slot=slot, material=material.get_path_name(),
            textures={k: v.get_path_name() for k, v in textures.items()}, normal_is_opengl=normal_is_opengl))
    (OUT / (gun + '_Materials.json')).write_text(json.dumps(report, indent=2))
    return materials


def assign_slots(mesh, materials):
    slots = mesh.get_editor_property('materials')
    resolved = []
    for i, slot in enumerate(slots):
        names = [str(slot.get_editor_property(k)) for k in ('imported_material_slot_name', 'material_slot_name')]
        match = next((name for name in names if name in materials), None)
        if match is None:
            # FBX/Unreal may replace punctuation while preserving spaces.
            # Normalize both sides, and reject any ambiguous source collision.
            normalized = {re.sub(r'[^A-Za-z0-9_]', '_', k): k for k in materials}
            assert len(normalized) == len(materials), 'Ambiguous normalized material names'
            candidates = [re.sub(r'[^A-Za-z0-9_]', '_', name) for name in names]
            match = next((normalized[name] for name in candidates if name in normalized), None)
        assert match is not None, dict(mesh=mesh.get_path_name(), unmatched=names, known=list(materials))
        slot.set_editor_property('material_interface', materials[match])
        slots[i] = slot
        resolved.append(match)
    mesh.set_editor_property('materials', slots)
    save(mesh)
    for i, name in enumerate(resolved):
        assert mesh.get_editor_property('materials')[i].get_editor_property('material_interface') == materials[name], (mesh.get_path_name(), i, name)
    return resolved


def clear_attachments(value):
    """Only S_Attachments, with seven nullable attachment data references."""
    result = value.copy()
    fields = re.findall(r'(\w+_\d+_[A-F0-9]+)=', result.export_text())
    assert len(fields) == 7, fields
    for field in fields:
        result.set_editor_property(field, None)
    return result


def struct_field(value, stem):
    names = re.findall(r'(\w+_\d+_[A-F0-9]+)=', value.export_text())
    matches = [name for name in names if re.fullmatch(re.escape(stem) + r'_\d+_[A-F0-9]+', name)]
    assert len(matches) == 1, (stem, names)
    return matches[0]


def make_qa_loadout(gun, weapon_da):
    """Create one isolated test preset; existing catalog/controller stay untouched."""
    directory = DEST + '/' + gun
    owned(directory + '/QA')
    bp = duplicate(DEST + '/BP_ArsenalController', directory + '/QA/BP_' + gun + '_QAController')
    loadout = components(bp)['LoadoutComponent']
    rows = loadout.get_editor_property('PlayerLoadouts')
    assert rows
    row = rows[0].copy()
    knife = asset(DEST + '/Butterfly/DA_Weapon_Butterfly')
    is_sidearm = gun in ('SwitchKnife', 'M1911', 'Pistol9mm', 'TalonPistol')
    main = asset('/Game/FPS_Controller/Blueprints/DataAssets/WeaponsData/DA_Weapon_Ak') if is_sidearm else weapon_da
    side = weapon_da if is_sidearm else knife
    for stem, value in [('PrimaryWeapon', main), ('SecondaryWeapon', side),
                        ('PrimaryWeaponSkin', None), ('SecondaryWeaponSkin', None)]:
        row.set_editor_property(struct_field(row, stem), value)
    for stem in ('PrimaryWeaponAttachments', 'SecondaryWeaponAttachments'):
        field = struct_field(row, stem)
        row.set_editor_property(field, clear_attachments(row.get_editor_property(field)))
    loadout.set_editor_property('PlayerLoadouts', [row])
    loadout.set_editor_property('DefaultLoadout', row.copy())
    loadout.set_editor_property('SelectedLoadoutIndex', 0)
    compile_save(bp)
    gm = duplicate(DEST + '/BP_ArsenalGameMode', directory + '/QA/BP_' + gun + '_QAGameMode')
    u.get_default_object(gm.generated_class()).set_editor_property('player_controller_class', bp.generated_class())
    compile_save(gm)
    return {'game_mode': gm.generated_class().get_path_name(), 'controller': bp.get_path_name(), 'loadout': row.export_text()}
