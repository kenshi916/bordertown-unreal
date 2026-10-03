"""Buffered per-frame weapon QA; no project/asset mutations or timed-loop I/O."""
import unreal as u,json,math,time,traceback
from pathlib import Path
def path(o):return o.get_path_name()if o else None
def prop(o,k,d=None):
    try:return o.get_editor_property(k)
    except:return d
def tr(t):return [t.translation.x,t.translation.y,t.translation.z,t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]
def tr_srt(t):return tr(t)+[t.scale3d.x,t.scale3d.y,t.scale3d.z]
def mul(a,b):
    x,y,z,w=a;X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def relative(t,r):
    t=tr(t);r=tr(r);inv=[-r[3],-r[4],-r[5],r[6]];p=[t[i]-r[i]for i in range(3)]+[0]
    return mul(mul(inv,p),r[3:])[:3]+mul(inv,t[3:])
def delta(a,b):
    d=math.dist(a[:3],b[:3]);n=math.sqrt(sum(x*x for x in a[3:])*sum(x*x for x in b[3:]))
    angle=math.degrees(2*math.acos(min(1.,abs(sum(x*y for x,y in zip(a[3:],b[3:]))/n))))if n else 0.
    return {'position_cm':d,'angle_deg':angle}

class GameplayProbe:
    def __init__(self,gun,pawn,hands,weapon,world,out):
        self.gun=gun;self.pawn=pawn;self.hands=hands;self.weapon=weapon;self.world=world;self.out=Path(out)
        self.mesh=next(c for c in weapon.get_components_by_class(u.SkeletalMeshComponent)if c.get_name()=='MeshFPP')
        self.visible={c.get_name():c for c in pawn.get_components_by_class(u.SkeletalMeshComponent)if c.get_name()in ['Gloves_FPP','MetaHumanBody_FPP','Mesh_FPP']}
        wanted={'Ballista':['root','bolt_joint','rear_joint','magazine_joint','trigger_joint'],
                'PGM':['root','bolt_slide','bolt_rotate','magazine','trigger'],
                'M1911':['slide','barrel','hammer','trigger'],
                'Pistol9mm':['slide','hammer','trigger'],'TalonPistol':['slide','hammer','trigger'],
                'UZI':['root','charging_handle','magazine','trigger'],
                'AR15':['root','magazine_joint','bolt_joint','charging_handle_joint','trigger_joint'],
                'SwitchKnife':['blade','lock','button'],'MP7':['b_Bolt','b_Magazine','b_Trigger','slide','mag_01','mag_02']}.get(gun,[])
        self.bones=[b for b in wanted if self.mesh.does_socket_exist(b)]
        self.expected=self.pose();self.expected_source='initial runtime pose';self.reference_error=None
        idle='/Game/BorderTownWeapons/Arsenal/'+gun+'/Animations/'+{'Ballista':'A_Ballista_Idle','PGM':'A_PGM_Idle','M1911':'A_M1911_Weapon_Idle','Pistol9mm':'A_Pistol9mm_Weapon_Idle','TalonPistol':'A_TalonPistol_Weapon_Idle','UZI':'A_UZI_Idle','AR15':'A_AR15_Idle','SwitchKnife':'A_SwitchKnife_Idle_Weapon'}.get(gun,'')
        if gun in ['Ballista','PGM','M1911','Pistol9mm','TalonPistol','UZI','AR15','SwitchKnife']:
            try:
                seq=u.load_asset(idle);assert seq,idle
                opt=u.AnimPoseEvaluationOptions();opt.set_editor_property('evaluation_type',u.AnimDataEvalType.COMPRESSED)
                opt.set_editor_property('optional_skeletal_mesh',prop(self.mesh,'skeletal_mesh_asset'))
                pose=u.AnimPoseExtensions.get_anim_pose_at_time(seq,0.,opt)
                root=u.AnimPoseExtensions.get_bone_pose(pose,'root',u.AnimPoseSpaces.WORLD)
                self.expected={b:(tr(root) if b=='root' else relative(u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.WORLD),root))for b in self.bones}
                self.expected_source=path(seq)+' at 0 seconds, compressed component-space pose'
            except:self.reference_error=traceback.format_exc()
        self.stream=None;self.rows=[];self.start=0.;self.summaries={};self.sample_errors=[]
    def pose(self):
        root=self.mesh.get_socket_transform('root',u.RelativeTransformSpace.RTS_COMPONENT)
        # Ballista's corrected grip preserves gun placement with an authored
        # nonidentity root. Comparing every joint relative to root alone would
        # incorrectly accept a reset-to-identity root and a shifted whole gun.
        return {b:(tr(root) if b=='root' else relative(self.mesh.get_socket_transform(b,u.RelativeTransformSpace.RTS_COMPONENT),root))for b in self.bones}
    def idle_right_fingers(self,sequence_path):
        """Compare authored finger shape in wrist space, excluding global sway.

        This detects a stock right-hand idle surviving a left-hand-only override.
        It does not certify visible Oskar retargeting or surface clearance.
        """
        result={'sequence':sequence_path,'space':'relative to Manny hand_r',
                'scope':'finger-pose delivery only; visible glove contact requires separate review',
                'bones':{},'passed':False,'error':None}
        try:
            seq=u.load_asset(sequence_path);assert seq,sequence_path
            opt=u.AnimPoseEvaluationOptions();opt.set_editor_property('evaluation_type',u.AnimDataEvalType.COMPRESSED)
            opt.set_editor_property('optional_skeletal_mesh',prop(self.hands,'skeletal_mesh_asset'))
            pose=u.AnimPoseExtensions.get_anim_pose_at_time(seq,0.,opt)
            ref_hand=u.AnimPoseExtensions.get_bone_pose(pose,'hand_r',u.AnimPoseSpaces.WORLD)
            actual_hand=self.hands.get_socket_transform('hand_r',u.RelativeTransformSpace.RTS_COMPONENT)
            for digit in ['thumb','index','middle','ring','pinky']:
                for segment in ['01','02','03']:
                    b=digit+'_'+segment+'_r';assert self.hands.does_socket_exist(b),b
                    expected=relative(u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.WORLD),ref_hand)
                    actual=relative(self.hands.get_socket_transform(b,u.RelativeTransformSpace.RTS_COMPONENT),actual_hand)
                    result['bones'][b]=delta(expected,actual)
            result['passed']=len(result['bones'])==15 and all(e['position_cm']<.2 and e['angle_deg']<2. for e in result['bones'].values())
        except:result['error']=traceback.format_exc()
        return result
    def ready_errors(self):
        actual=self.pose();return {b:delta(self.expected[b],actual[b])for b in self.bones}
    def mechanism_delta(self,bone):return self.ready_errors().get(bone,{'position_cm':0.,'angle_deg':0.})
    def ready(self):return bool(self.bones)and all(e['position_cm']<.15 and e['angle_deg']<1.5 for e in self.ready_errors().values())
    def begin(self,label,clip_token):
        assert self.stream is None
        self.stream=label;self.token=clip_token;self.rows=[];self.start=u.GameplayStatics.get_time_seconds(self.world);self.sample()
    def sample(self):
        if not self.stream:return
        try:
            ai=self.hands.get_anim_instance();m=ai.get_current_active_montage();wa=self.mesh.get_anim_instance()
            asset=wa.get_animation_asset()if wa and hasattr(wa,'get_animation_asset')else None
            row={'engine_frame':u.SystemLibrary.get_frame_count(),'game_time':u.GameplayStatics.get_time_seconds(self.world),'wall':time.perf_counter(),
                 'montage':path(m),'hand_time':ai.montage_get_position(m)if m else None,'weapon_clip':path(asset),'weapon_time':self.mesh.get_position(),
                 'weapon_clip_duration':prop(asset,'sequence_length') if asset else None,
                 'mechanisms_relative_to_root':self.pose(),
                 'manny_hand_r_world':tr(self.hands.get_socket_transform('hand_r',u.RelativeTransformSpace.RTS_WORLD)),
                 'weapon_component_world':tr(self.mesh.get_world_transform())}
            glove=self.visible.get('Gloves_FPP');body=self.visible.get('MetaHumanBody_FPP')
            if glove:row['glove_hand_r_world']=tr(glove.get_socket_transform('hand_r',u.RelativeTransformSpace.RTS_WORLD))
            if body:row['body_hand_r_world']=tr(body.get_socket_transform('hand_r',u.RelativeTransformSpace.RTS_WORLD))
            self.rows.append(row)
        except:
            if not self.sample_errors:self.sample_errors.append(traceback.format_exc())
    def end(self):
        self.sample();label=self.stream;self.stream=None
        rows=self.rows;token=self.token
        paired=[r for r in rows if r['montage']and token.lower()in r['montage'].lower()and r['weapon_clip']and token.lower()in r['weapon_clip'].lower()and r['hand_time']is not None and min(r['hand_time'],r['weapon_time'])>.015]
        # A short mechanism clip can finish before the hand recovery montage.
        # Compare its held endpoint with the same clamped hand clock.
        errors=[abs(min(r['hand_time'],r.get('weapon_clip_duration') or r['hand_time'])-r['weapon_time'])for r in paired]
        # A trace may begin before automatic reload (and include final shots).
        # Only matching hand/weapon clips may establish mechanism travel.
        ranges={b:{key:max((delta(self.expected[b],r['mechanisms_relative_to_root'][b])[key]for r in paired),default=0.)for key in ['position_cm','angle_deg']}for b in self.bones}
        file=self.out/(label+'_FrameTrace.json');file.write_text(json.dumps({'label':label,'expected_clip_token':token,'rows':rows,'errors':self.sample_errors},separators=(',',':')))
        summary={'trace':str(file),'frames':len(rows),'paired_samples':len(paired),'max_paired_clock_error_seconds':max(errors)if errors else None,
                 'max_weapon_component_travel_cm':max((math.dist(rows[0]['weapon_component_world'][:3],r['weapon_component_world'][:3])for r in rows),default=0.),
                 'first_paired_game_delay_seconds':paired[0]['game_time']-self.start if paired else None,
                 'paired_hand_time_range':[min(r['hand_time']for r in paired),max(r['hand_time']for r in paired)]if paired else None,
                 'mechanism_ranges':ranges,'mechanism_range_scope':'matching paired clips only','clock_policy':'weapon clock matches hand clock clamped to mechanism clip duration','errors':list(self.sample_errors)}
        self.summaries[label]=summary;self.rows=[];return summary
    def components(self):
        result=[]
        for c in self.pawn.get_components_by_class(u.SkeletalMeshComponent):
            result.append({'name':c.get_name(),'asset':path(prop(c,'skeletal_mesh_asset')),'anim_class':path(prop(c,'anim_class')),
                'leader_pose_component':path(prop(c,'leader_pose_component')),'parent':path(c.get_attach_parent()),'socket':str(c.get_attach_socket_name()),
                'visible':prop(c,'visible'),'hidden_in_game':prop(c,'hidden_in_game'),'only_owner_see':prop(c,'only_owner_see'),'owner_no_see':prop(c,'owner_no_see')})
        return result
    def glove_snapshot(self,label):
        data={'space':'Unreal world centimetres','components':{},'errors':[]}
        data['srt_array_layout']=['tx_cm','ty_cm','tz_cm','qx','qy','qz','qw','sx','sy','sz']
        weapon_bones=[str(self.mesh.get_bone_name(i))for i in range(self.mesh.get_num_bones())]
        data['weapon']={'asset':path(prop(self.mesh,'skeletal_mesh_asset')),
            'first_person_primitive_type':str(prop(self.mesh,'first_person_primitive_type')),
            'world_srt':tr_srt(self.mesh.get_world_transform()),
            'all_bones_component_srt':{b:tr_srt(self.mesh.get_socket_transform(b,u.RelativeTransformSpace.RTS_COMPONENT))for b in weapon_bones},
            'all_bones_world_srt':{b:tr_srt(self.mesh.get_socket_transform(b,u.RelativeTransformSpace.RTS_WORLD))for b in weapon_bones}}
        camera=u.GameplayStatics.get_player_camera_manager(self.world,0)
        if camera:
            location=camera.get_camera_location();rotation=camera.get_camera_rotation()
            data['camera']={'location_cm':[location.x,location.y,location.z],
                'rotation':str(rotation),'rotation_pitch_yaw_roll':[rotation.pitch,rotation.yaw,rotation.roll],'fov':camera.get_fov_angle()}
        data['camera_components']=[{'name':c.get_name(),**{k:str(prop(c,k)) for k in ['field_of_view','first_person_field_of_view','first_person_scale','enable_first_person_field_of_view','enable_first_person_scale','override_perspective_near_clip_plane','perspective_near_clip_plane']}} for c in self.pawn.get_components_by_class(u.CameraComponent)]
        try:data['configured_engine_near_clip_cm']=u.get_default_object(u.load_class(None,'/Script/Engine.Engine')).get_editor_property('near_clip_plane')
        except Exception as e:data['engine_near_clip_read_error']=str(e)
        bones=['hand_r','thumb_03_r','index_03_r','middle_03_r','hand_l','thumb_03_l','index_03_l','middle_03_l']
        for name,c in self.visible.items():
            data['components'][name]={'asset':path(prop(c,'skeletal_mesh_asset')),'world':tr(c.get_world_transform()),
                'bones':{b:tr(c.get_socket_transform(b,u.RelativeTransformSpace.RTS_WORLD))for b in bones if c.does_socket_exist(b)}}
            names=[str(c.get_bone_name(i))for i in range(c.get_num_bones())]
            data['components'][name]['all_bones_component']={b:tr(c.get_socket_transform(b,u.RelativeTransformSpace.RTS_COMPONENT))for b in names}
            data['components'][name]['all_bones_world']={b:tr(c.get_socket_transform(b,u.RelativeTransformSpace.RTS_WORLD))for b in names}
            data['components'][name]['world_srt']=tr_srt(c.get_world_transform())
            data['components'][name]['all_bones_component_srt']={b:tr_srt(c.get_socket_transform(b,u.RelativeTransformSpace.RTS_COMPONENT))for b in names}
            data['components'][name]['all_bones_world_srt']={b:tr_srt(c.get_socket_transform(b,u.RelativeTransformSpace.RTS_WORLD))for b in names}
        glove=self.visible.get('Gloves_FPP')
        if glove:
            try:
                dm=u.DynamicMesh();opt=u.GeometryScriptCopyMeshFromComponentOptions();opt.set_editor_property('want_tangents',False)
                lod=u.GeometryScriptMeshReadLOD();lod.set_editor_property('lod_type',u.GeometryScriptLODType.RENDER_DATA);lod.set_editor_property('lod_index',0);opt.set_editor_property('requested_lod',lod)
                u.GeometryScript_SceneUtils.copy_mesh_from_component(glove,dm,opt,True)
                vs=u.GeometryScript_MeshQueries.get_all_vertex_positions(dm,False);vs=vs if isinstance(vs,u.GeometryScriptVectorList)else next(x for x in vs if isinstance(x,u.GeometryScriptVectorList))
                vs=u.GeometryScript_List.convert_vector_list_to_array(vs)
                ts=u.GeometryScript_MeshQueries.get_all_triangle_indices(dm,True);ts=ts if isinstance(ts,u.GeometryScriptTriangleList)else next(x for x in ts if isinstance(x,u.GeometryScriptTriangleList))
                ts=u.GeometryScript_List.convert_triangle_list_to_array(ts)
                data['components']['Gloves_FPP']['vertices']=[[p.x,p.y,p.z]for p in vs]
                data['components']['Gloves_FPP']['triangles']=[[t.x,t.y,t.z]for t in ts]
            except:data['errors'].append(traceback.format_exc())
        file=self.out/('GloveGeometry_'+label+'.json');file.write_text(json.dumps(data,separators=(',',':')));return {'file':str(file),'errors':data['errors']}

class MP7AttachmentProbe:
    """Observe the actual magazine actor; both copies live in its FPP mesh."""
    D='/Game/BorderTownWeapons/Arsenal/MP7'
    def __init__(self,weapon,hands,world,out,gun='MP7'):
        self.gun=gun;self.D='/Game/BorderTownWeapons/Arsenal/'+gun
        self.weapon=weapon;self.hands=hands;self.world=world;self.out=Path(out)
        self.body=next(c for c in weapon.get_components_by_class(u.SkeletalMeshComponent)if c.get_name()=='MeshFPP')
        self.ac=weapon.get_component_by_class(u.load_class(None,'/Game/FPS_Controller/Blueprints/Components/AC_WeaponAttachments.AC_WeaponAttachments_C'))
        self.stream=None;self.rows=[];self.errors=[];self.expected={};self.reference_error=None
        self.refresh()
        try:
            assert self.mag_fpp,'Missing actual magazine FPP component'
            seq=u.load_asset(self.D+'/Animations/A_'+self.gun+'_Mag_Idle');assert seq
            opt=u.AnimPoseEvaluationOptions();opt.set_editor_property('evaluation_type',u.AnimDataEvalType.COMPRESSED)
            opt.set_editor_property('optional_skeletal_mesh',prop(self.mag_fpp,'skeletal_mesh_asset'))
            pose=u.AnimPoseExtensions.get_anim_pose_at_time(seq,0.,opt)
            root=u.AnimPoseExtensions.get_bone_pose(pose,'root',u.AnimPoseSpaces.WORLD)
            self.expected={b:relative(u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.WORLD),root)for b in ['mag_01','mag_02']}
            self.mag_duration=u.load_asset(self.D+'/Animations/A_'+self.gun+'_Mag_Reload').sequence_length
        except:self.reference_error=traceback.format_exc();self.mag_duration=0.
    def refresh(self):
        self.mag=prop(self.ac,'Magazine');self.foregrip=prop(self.ac,'Underbarrel')
        self.mag_fpp=next((c for c in self.mag.get_components_by_class(u.SkeletalMeshComponent)if c.get_name()=='FPP'),None)if self.mag else None
    def attachment_snapshot(self):
        self.refresh();result={}
        for key,actor in [('magazine',self.mag),('foregrip',self.foregrip)]:
            row={'actor':path(actor),'class':path(actor.get_class())if actor else None,'owner':path(actor.get_owner())if actor else None,'components':[]}
            if actor:
                for c in actor.get_components_by_class(u.MeshComponent):
                    chain=[];parent=c.get_attach_parent()
                    while parent and len(chain)<12:chain.append(path(parent));parent=parent.get_attach_parent()
                    row['components'].append({'name':c.get_name(),'mesh':path(prop(c,'skeletal_mesh_asset',prop(c,'static_mesh'))),'parent_chain':chain,
                        'attached_to_weapon_fpp':path(self.body)in chain,'socket':str(c.get_attach_socket_name()),'visible':prop(c,'visible'),'hidden_in_game':prop(c,'hidden_in_game'),
                        'materials':[path(c.get_material(i))for i in range(c.get_num_materials())]})
            result[key]=row
        return result
    def pose(self):
        if not self.mag_fpp:return {}
        root=self.mag_fpp.get_socket_transform('root',u.RelativeTransformSpace.RTS_COMPONENT)
        return {b:relative(self.mag_fpp.get_socket_transform(b,u.RelativeTransformSpace.RTS_COMPONENT),root)for b in ['mag_01','mag_02']if self.mag_fpp.does_socket_exist(b)}
    def ready_errors(self):
        actual=self.pose();return {b:delta(p,actual[b])for b,p in self.expected.items()if b in actual}
    def ready(self):
        e=self.ready_errors();return len(e)==2 and all(x['position_cm']<.15 and x['angle_deg']<1.5 for x in e.values())
    def displaced(self):return max((e['position_cm']for e in self.ready_errors().values()),default=0.)
    def begin(self,label,hand_clip):
        assert self.stream is None;self.stream=label;self.hand_clip='AM_FPP_'+self.gun+'_'+hand_clip;self.rows=[];self.sample()
    def sample(self):
        if not self.stream:return
        try:
            ai=self.hands.get_anim_instance();m=ai.get_current_active_montage();ma=self.mag_fpp.get_anim_instance()if self.mag_fpp else None
            clip=ma.get_animation_asset()if ma and hasattr(ma,'get_animation_asset')else None
            self.rows.append({'game_time':u.GameplayStatics.get_time_seconds(self.world),'engine_frame':u.SystemLibrary.get_frame_count(),
                'montage':path(m),'hand_time':ai.montage_get_position(m)if m else None,'mag_clip':path(clip),
                'mag_time':self.mag_fpp.get_position()if self.mag_fpp else None,'magazine_relative_to_root':self.pose()})
        except:
            if not self.errors:self.errors.append(traceback.format_exc())
    def end(self):
        self.sample();label=self.stream;self.stream=None
        active=[r for r in self.rows if r['montage']and r['montage'].split('.')[-1]==self.hand_clip and r['mag_clip']and r['mag_clip'].split('.')[-1]=='A_'+self.gun+'_Mag_Reload']
        # The 2.167-second magazine clip legitimately holds its end while the
        # 3-second empty-reload hand clip operates the charging handle.
        paired=[r for r in active if r['hand_time']is not None and r['mag_time']is not None and .015<r['hand_time']<self.mag_duration-.02]
        clock=[abs(r['hand_time']-r['mag_time'])for r in paired]
        ranges={b:{k:max((delta(self.expected[b],r['magazine_relative_to_root'][b])[k]for r in active if b in r['magazine_relative_to_root']),default=0.)for k in ['position_cm','angle_deg']}for b in self.expected}
        file=self.out/(label+'_MagazineTrace.json');file.write_text(json.dumps({'rows':self.rows,'reference':self.expected,'errors':self.errors},separators=(',',':')))
        result={'trace':str(file),'frames':len(self.rows),'matching_samples':len(active),'paired_samples':len(paired),'max_clock_error_seconds':max(clock)if clock else None,'magazine_duration':self.mag_duration,'mechanism_ranges':ranges,'errors':self.errors}
        self.rows=[];return result
