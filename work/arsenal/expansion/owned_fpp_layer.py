"""Clone a weapon's FPP layer and route its complete neutral pose explicitly.

Prepared offline; invoked only by an authorized owned-weapon importer. Dynamic
Idle/Aiming defaults are patched only after inspecting their inherited graph
connections. Unknown layouts fail before duplication. Shared layers are read-only.
"""
import json,hashlib,traceback,re
from pathlib import Path
import unreal as u
from ue_expansion_common import DEST,OUT,owned
from ue_common import asset,duplicate,compile_save,save
from layer_binding_text import parse_binding_export

from public_paths import PROJECT

def path(obj):return obj.get_path_name() if obj else None
def serial(value):
 if isinstance(value,u.Object):return path(value)
 if isinstance(value,(list,tuple,u.Array)):return [serial(v)for v in value]
 if hasattr(value,'export_text'):return value.export_text()
 if isinstance(value,(str,int,float,bool))or value is None:return value
 # Delegates include transient native addresses that change on recompilation.
 return re.sub(r'0x[0-9A-Fa-f]+','<address>',str(value))

def defaults(bp):
 cdo=u.get_default_object(bp.generated_class());result={}
 for name in u.BlueprintEditorLibrary.list_member_variable_names(bp,True):
  name=str(name).rsplit('.',1)[-1]
  try:result[name]=serial(cdo.get_editor_property(name))
  except Exception:pass
 return result

def hierarchy(bp):
 seen=set();result=[]
 while bp:
  p=bp.get_path_name().split('.')[0]
  assert p not in seen,'Cyclic Blueprint parent chain';seen.add(p);result.append(bp)
  # AnimBlueprint does not expose the parent_class editor property in UE5.8.
  # The supported BlueprintEditorLibrary accessor handles both BP varieties.
  parent=u.BlueprintEditorLibrary.get_blueprint_parent_class(bp);pp=path(parent)
  if not pp or not pp.startswith('/Game/'):break
  bp=u.load_asset(pp.split('.')[0]);assert isinstance(bp,u.AnimBlueprint),pp
 return result

def export_binding_metadata(binding):
 """Native read-only serialization exposes metadata hidden from Python fields."""
 folder=Path(__file__).resolve().parent/'layer_binding_text_exports';folder.mkdir(exist_ok=True)
 target=folder/(hashlib.sha256(path(binding).encode()).hexdigest()+'.t3d')
 task=u.AssetExportTask();task.object=binding;task.filename=str(target)
 task.automated=True;task.prompt=False;task.replace_identical=True
 task.exporter=u.ObjectExporterT3D()
 assert u.Exporter.run_asset_export_task(task),'Cannot inspect protected animation binding'
 result=parse_binding_export(target.read_text(encoding='utf-8-sig'),path(binding))
 return result,str(target)

def graph_dump(bp,names):
 result={}
 for graph in u.BlueprintEditorLibrary.list_graphs(bp):
  if graph.get_name()not in names:continue
  rows=[]
  for node in u.BlueprintGraphEditor.get_graph_editor(graph).list_all_nodes():
   row={'name':node.get_name(),'title':str(node.get_node_title()),'pins':[]}
   try:row['node']=node.get_editor_property('node').export_text()
   except Exception:pass
   binding=None
   try:
    binding=node.get_editor_property('binding')
    if binding:
     row['binding_object']=path(binding);row['property_bindings']={}
     # Binding_Base is wrapped through its public base class. Its private
     # UPROPERTY fields therefore require their native reflected names, not
     # Python snake-case aliases absent from that base wrapper's metadata.
     for key,value in binding.get_editor_property('PropertyBindings').items():
      row['property_bindings'][str(key)]={
       'property_name':str(value.get_editor_property('PropertyName')),
       'property_path':[str(x)for x in value.get_editor_property('PropertyPath')],
       'is_bound':bool(value.get_editor_property('bIsBound')),
       'type':str(value.get_editor_property('Type')),'text':value.export_text()}
   except Exception as e:
    row['binding_read_error']=str(e)
    # UE 5.8 serializes this map but protects direct editor-property access.
    # Inspect the exact live binding through its native exporter instead.
    if binding and row['name'].startswith(('AnimGraphNode_SequencePlayer','AnimGraphNode_SequenceEvaluator')):
     try:row['property_bindings'],row['binding_text_export']=export_binding_metadata(binding)
     except Exception as ex:row['binding_export_error']=str(ex)
   for pin in node.list_all_pins():
    row['pins'].append({'name':str(pin.get_pin_name()),'value':pin.get_pin_value(),
     'links':[{'node':p.get_owning_node().get_name(),'pin':str(p.get_pin_name())}for p in pin.list_connected_pins()]})
   rows.append(row)
  result[graph.get_name()]=rows
 return result

def verify_property_route(graphs,graph_name,field,player_prefixes,target_pin):
 """Inspect the most-derived layer graph: wired getter or native property binding."""
 for bp_path,by_name in graphs.items():
  if graph_name not in by_name:continue
  rows=by_name[graph_name];by_node={r['name']:r for r in rows}
  def reaches_output(player):
   queue=[link for pin in by_node[player]['pins']if pin['name']=='Pose'for link in pin['links']];seen=set()
   while queue:
    link=queue.pop(0);key=(link['node'],link['pin'])
    if key in seen:continue
    seen.add(key)
    if link['node'].startswith('AnimGraphNode_Root')and link['pin']=='Result':return True
    if link['node'].startswith('K2Node_Knot'):
     queue.extend(p for x in by_node[link['node']]['pins']for p in x['links'])
   return False
  for row in rows:
   if row['name'].startswith(player_prefixes)and reaches_output(row['name']):
    for key,binding in row.get('property_bindings',{}).items():
     if key==target_pin and binding.get('property_name')==target_pin and binding.get('is_bound')is True and binding.get('property_path')==[field]:
      return {'blueprint':bp_path,'graph':graph_name,'field':field,'player':row['name'],
       'route':'native_property_binding','binding':binding,'output_verified':True,'verified':True}
   if not row['name'].startswith('K2Node_VariableGet'):continue
   for pin in row['pins']:
    if pin['name']!=field:continue
    queue=list(pin['links']);seen=set()
    while queue:
     link=queue.pop(0);key=(link['node'],link['pin'])
     if key in seen:continue
     seen.add(key)
     if link['node'].startswith(player_prefixes)and link['pin']==target_pin and reaches_output(link['node']):
      return {'blueprint':bp_path,'graph':graph_name,'field':field,'getter':row['name'],'player':link['node'],
       'route':'wired_property_getter','output_verified':True,'verified':True}
     if link['node'].startswith('K2Node_Knot'):
      queue.extend(p for x in by_node[link['node']]['pins']for p in x['links'])
  # A child-defined graph overrides its parent. Never accept a different parent
  # graph as proof when this more-derived graph has an unsupported route.
  raise AssertionError(f'{bp_path}:{graph_name} does not expose a verified {field} -> {target_pin} output route; inspect the saved readback before patching.')
 raise AssertionError(f'Inherited {graph_name} graph unavailable; no guessed node/default patch was made.')

def verify_dynamic_sequence_route(graphs,graph_name,field):
 return verify_property_route(graphs,graph_name,field,('AnimGraphNode_SequencePlayer','AnimGraphNode_SequenceEvaluator'),'Sequence')

def sample_rows(blendspace):
 return [{'animation':path(s.get_editor_property('animation')),
  'sample_value':s.get_editor_property('sample_value').export_text(),
  'rate_scale':float(s.get_editor_property('rate_scale')),
  'single_frame':bool(s.get_editor_property('use_single_frame_for_blending')),
  'frame_index':int(s.get_editor_property('frame_index_to_sample'))}
  for s in blendspace.get_editor_property('sample_data')]

def validate_walk_overrides(gun,source,idle,overrides):
 """Require an explicit complete donor->owned map before any layer mutation."""
 if overrides is None:return None
 assert source is not None,'Walk sample overrides require the verified WalkBlendSpace route.'
 assert isinstance(overrides,dict)and overrides,'Walk overrides must be a complete explicit mapping.'
 normalized={str(k).split('.')[0]:v for k,v in overrides.items()}
 assert len(normalized)==len(overrides),'Duplicate normalized walk source keys.'
 donors={r['animation'].split('.')[0]for r in sample_rows(source)}
 assert set(normalized)==donors,{'missing':sorted(donors-set(normalized)),'extra':sorted(set(normalized)-donors)}
 for donor,replacement in normalized.items():
  assert isinstance(replacement,u.AnimSequence)and path(replacement).startswith(DEST+'/'+gun+'/'),path(replacement)
  original=asset(donor)
  assert replacement.get_editor_property('skeleton')==original.get_editor_property('skeleton')==idle.get_editor_property('skeleton')
  assert abs(replacement.sequence_length-original.sequence_length)<.001,('Walk duration mismatch',donor,replacement.sequence_length,original.sequence_length)
 return normalized

def configure_hold_blendspace(gun,source,idle,overrides=None):
 """Keep the verified WalkBlendSpace graph; replace samples only in an owned copy.

 By default all axes use the fitted hold. A complete explicit override map can
 preserve authored directional motion; source samples and layers stay untouched.
 """
 destination=DEST+'/'+gun+'/Animations/BS_'+gun+'_OwnedHold';owned(destination)
 assert isinstance(source,u.BlendSpace),path(source)
 assert source.get_editor_property('skeleton')==idle.get_editor_property('skeleton')
 donor_rows=sample_rows(source);assert donor_rows,'Source WalkBlendSpace has no samples.'
 blendspace=duplicate(path(source).split('.')[0],destination)
 assert isinstance(blendspace,u.BlendSpace)and blendspace.get_path_name().split('.')[0]==destination
 before=sample_rows(blendspace);assert len(before)==len(donor_rows)
 assert [x['sample_value']for x in before]==[x['sample_value']for x in donor_rows],'Owned sample topology differs from donor.'
 # Start from donor sample structs so reruns restore any source settings that a
 # previous constant-hold pass disabled. Sample positions remain identical.
 samples=[s.copy()for s in source.get_editor_property('sample_data')]if overrides is not None else blendspace.get_editor_property('sample_data')
 for index,sample in enumerate(samples):
  replacement=overrides[donor_rows[index]['animation'].split('.')[0]]if overrides is not None else idle
  sample.set_editor_property('animation',replacement)
  if overrides is None:
   # Donor single-frame indices can exceed the short fitted hold sequence.
   sample.set_editor_property('use_single_frame_for_blending',False)
   sample.set_editor_property('frame_index_to_sample',0)
  # Array iteration yields value-copy UStruct wrappers. Write each copy back.
  samples[index]=sample
 blendspace.set_editor_property('sample_data',samples);save(blendspace)
 after=sample_rows(blendspace)
 expected=[path(overrides[r['animation'].split('.')[0]])if overrides is not None else path(idle)for r in donor_rows]
 assert [x['animation']for x in after]==expected,{'expected':expected,'actual':after}
 assert [x['sample_value']for x in after]==[x['sample_value']for x in donor_rows]
 assert [x['rate_scale']for x in after]==[x['rate_scale']for x in donor_rows]
 if overrides is not None:
  assert [(x['single_frame'],x['frame_index'])for x in after]==[(x['single_frame'],x['frame_index'])for x in donor_rows]
 assert sample_rows(source)==donor_rows,'Source BlendSpace was unexpectedly changed.'
 return blendspace,{'source':path(source),'owned':path(blendspace),'source_samples':donor_rows,
  'before':before,'after':after,'policy':'Explicit fitted directional samples retain source sample settings.'if overrides is not None else'All existing walk-axis sample positions use the fitted hold; authored directional walk motion is pending.'}

def configure_owned_fpp_layer(gun,animation_data,idle,aiming=None,*,source_layer_path,
 expected_source_idle=None,expected_source_aiming=None,report_path=None,
 aiming_validation=None,walk_sample_overrides=None,extra_pose_defaults=None):
 """Return a report and assign the owned class to AnimationData.FPP_WeaponAnims.

 aiming=None preserves donor ADS. A replacement requires a nonempty evidence
 description in aiming_validation; hip idle is never silently used for ADS.
 Optional walk overrides map exact donor sequence paths to owned sequences.
 Extra pose defaults map Run/JumpStart/JumpLoop/JumpEnd/TacticalSprint to
 {'source': exact donor path, 'animation': owned sequence}; all require runtime
 locomotion delivery checks before promotion.
 """
 destination=DEST+'/'+gun+'/Animations/ABP_FPP_'+gun+'_Layer';owned(destination)
 assert animation_data.get_path_name().startswith(DEST+'/'+gun+'/')
 assert isinstance(idle,u.AnimSequence)and idle.get_path_name().startswith(DEST+'/'+gun+'/')
 if aiming is not None:
  assert isinstance(aiming,u.AnimSequence)and aiming.get_path_name().startswith(DEST+'/'+gun+'/')
  assert isinstance(aiming_validation,str)and aiming_validation.strip(),'ADS replacement requires explicit fit-validation evidence.'
 report_path=Path(report_path)if report_path else OUT/gun/'FPP_Layer_Integration.json'
 report_path.parent.mkdir(parents=True,exist_ok=True)
 R={'status':'inspecting','source':source_layer_path,'owned_destination':destination,'project_scope':DEST+'/'+gun,
  'ads_policy':'validated_owned_replacement'if aiming is not None else'preserve_donor_aiming',
  'ads_grip_corrected':aiming is not None,'runtime_pose_delivery_verified':False,'aiming_validation':aiming_validation}
 hashes={}
 def checkpoint():
  R['protected_unchanged']={str(p):hashlib.sha256(p.read_bytes()).hexdigest()==h for p,h in hashes.items()}
  report_path.write_text(json.dumps(R,indent=2,default=str))
 try:
  source=asset(source_layer_path);assert isinstance(source,u.AnimBlueprint),source_layer_path
  chain=hierarchy(source)
  for b in chain:
   f=PROJECT/'Content'/Path(b.get_path_name().split('.')[0].removeprefix('/Game/')).with_suffix('.uasset')
   assert f.is_file(),str(f);hashes[f]=hashlib.sha256(f.read_bytes()).hexdigest()
  R['source_defaults']=defaults(source);R['hierarchy']=[path(b)for b in chain]
  R['graphs']={path(b):graph_dump(b,{'IdleState','AimingState','LeftHandPoseOverride'})for b in chain}
  scdo=u.get_default_object(source.generated_class());source_idle=scdo.get_editor_property('Idle')
  assert isinstance(source_idle,u.AnimSequence),'Source Idle is not an exposed animation-sequence default.'
  if expected_source_idle:assert path(source_idle).split('.')[0]==expected_source_idle.split('.')[0],path(source_idle)
  source_walk=None
  try:R['idle_route']=verify_dynamic_sequence_route(R['graphs'],'IdleState','Idle')
  except AssertionError:
   R['idle_route']=verify_property_route(R['graphs'],'IdleState','WalkBlendSpace',('AnimGraphNode_BlendSpacePlayer',),'BlendSpace')
   source_walk=scdo.get_editor_property('WalkBlendSpace');assert isinstance(source_walk,u.BlendSpace)
   f=PROJECT/'Content'/Path(path(source_walk).split('.')[0].removeprefix('/Game/')).with_suffix('.uasset')
   assert f.is_file();hashes[f]=hashlib.sha256(f.read_bytes()).hexdigest()
  source_aim=None
  try:source_aim=scdo.get_editor_property('Aiming')
  except Exception:
   assert aiming is None,'Source does not expose Aiming; inspect its actual ADS route.'
  if aiming is not None:
   assert isinstance(source_aim,u.AnimSequence),'Source Aiming is not an exposed sequence default.'
   if expected_source_aiming:assert path(source_aim).split('.')[0]==expected_source_aiming.split('.')[0]
   R['aiming_route']=verify_dynamic_sequence_route(R['graphs'],'AimingState','Aiming')
  R['donor_aiming']=path(source_aim)
  assert idle.get_editor_property('skeleton')==source_idle.get_editor_property('skeleton'),'Idle skeleton differs from source layer.'
  if aiming is not None:assert aiming.get_editor_property('skeleton')==source_aim.get_editor_property('skeleton')
  walk_overrides=validate_walk_overrides(gun,source_walk,idle,walk_sample_overrides)
  if walk_overrides is not None and path(source_idle).split('.')[0]in walk_overrides:
   assert walk_overrides[path(source_idle).split('.')[0]]==idle,'Walk idle sample must use the declared owned idle.'
  extras=extra_pose_defaults or {};assert isinstance(extras,dict)
  assert set(extras)<=set(['Run','JumpStart','JumpLoop','JumpEnd','TacticalSprint']),set(extras)
  R['extra_pose_defaults']={}
  for field,spec in extras.items():
   assert isinstance(spec,dict)and set(spec)=={'source','animation'},field
   original=scdo.get_editor_property(field);replacement=spec['animation']
   assert isinstance(original,u.AnimSequence)and path(original).split('.')[0]==spec['source'].split('.')[0],field
   assert isinstance(replacement,u.AnimSequence)and path(replacement).startswith(DEST+'/'+gun+'/'),field
   assert replacement.get_editor_property('skeleton')==original.get_editor_property('skeleton')==idle.get_editor_property('skeleton'),field
   assert abs(replacement.sequence_length-original.sequence_length)<.001,('Pose duration mismatch',field)
   R['extra_pose_defaults'][field]={'source':path(original),'replacement':path(replacement),'runtime_delivery_verified':False}
  checkpoint()
  layer=duplicate(source_layer_path,destination);assert layer.get_path_name().split('.')[0]==destination
  before=defaults(layer);cdo=u.get_default_object(layer.generated_class());cdo.set_editor_property('Idle',idle)
  hold=None
  if source_walk:
   hold,R['walk_blendspace']=configure_hold_blendspace(gun,source_walk,idle,walk_overrides)
   cdo.set_editor_property('WalkBlendSpace',hold)
  if aiming is not None:cdo.set_editor_property('Aiming',aiming)
  # Preserve any already-authorized ADS override on a rerun only when the caller
  # provides it explicitly. With preserve policy restore the donor default.
  elif source_aim is not None:cdo.set_editor_property('Aiming',source_aim)
  for field,spec in extras.items():cdo.set_editor_property(field,spec['animation'])
  compile_save(layer)
  cdo=u.get_default_object(layer.generated_class());assert cdo.get_editor_property('Idle')==idle
  if hold:assert cdo.get_editor_property('WalkBlendSpace')==hold
  if source_aim is not None:assert cdo.get_editor_property('Aiming')==(aiming or source_aim)
  for field,spec in extras.items():assert cdo.get_editor_property(field)==spec['animation'],field
  after=defaults(layer);allowed={'Idle','Aiming','WalkBlendSpace'}|set(extras)
  unexpected={k:[before[k],v]for k,v in after.items()if k in before and before[k]!=v and k not in allowed}
  assert not unexpected,('Unexpected owned layer default changes',unexpected)
  animation_data.set_editor_property('FPP_WeaponAnims',layer.generated_class());save(animation_data)
  assert animation_data.get_editor_property('FPP_WeaponAnims')==layer.generated_class()
  R.update(status='configured_pending_runtime_pose_qa',owned_class=path(layer.generated_class()),
   animation_data=path(animation_data),owned_defaults_before=before,owned_defaults_after=after,
   idle=path(idle),aiming=path(aiming or source_aim),
   limitations=['Runtime neutral pose must be compared against the owned idle, including right wrist and fingers.']+
    (['WalkBlendSpace uses the fitted hold at every source sample position; directional walk/bob animation requires later fitting.']if hold and walk_overrides is None else[])+
    (['Explicit directional walk and locomotion defaults require runtime movement/jump/sprint pose-delivery checks.']if walk_overrides is not None or extras else[])+
    ([]if aiming is not None else['Donor ADS remains unchanged; custom neutral grip correction is not certified for ADS.']))
  checkpoint();assert all(R['protected_unchanged'].values()),'Shared layer bytes changed unexpectedly.'
  return R
 except Exception:
  R.update(status='failed_closed',error=traceback.format_exc());checkpoint();raise
