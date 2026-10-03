"""Stateless interruption cleanup for an exact owned weapon mesh only."""
import unreal as u
from ue_common import compile_save
from ue_expansion_common import DEST
BEL=u.BlueprintEditorLibrary
INV='/Game/FPS_Controller/Blueprints/Components/AC_InventorySystem.AC_InventorySystem_C'
BASE='/Game/FPS_Controller/Blueprints/Weapons/BP_BaseWeapon.BP_BaseWeapon_C'

def inp(node,name):
    pin=node.find_input_pin(name);assert pin.is_valid(),(node.get_name(),name,'input');return pin
def out(node,name):
    pin=node.find_output_pin(name);assert pin.is_valid(),(node.get_name(),name,'output');return pin
def connect(a,b):
    assert a.is_valid() and b.is_valid()
    assert a.try_create_connection(b),(a.get_owning_node().get_name(),str(a.get_pin_name()),b.get_owning_node().get_name(),str(b.get_pin_name()))
def value(node,name,v):assert inp(node,name).set_pin_value(v),(node.get_name(),name,v)
def call(ed,path):
    node=ed.add_call_function_node(path);assert node,path;return node
def getter(ed,name,path):
    node=ed.add_get_member_variable_node(name,path);assert node,(name,path);return node
def clear_body(ed):
    nodes=list(ed.list_all_nodes())
    entry=next(n for n in nodes if n.get_class().get_name()=='K2Node_FunctionEntry')
    returns=[n for n in nodes if n.get_class().get_name()=='K2Node_FunctionResult']
    result=returns[0] if returns else ed.add_return_node()
    ed.remove_nodes([n for n in nodes if n not in [entry,result]])
    for n in [entry,result]:
        for p in n.list_all_pins():p.break_pin_links()
    return entry,result
def valid_branch(ed,execute,object_pin,fallback):
    valid=call(ed,'/Script/Engine.KismetSystemLibrary.IsValid')
    branch=ed.add_branch_node()
    connect(object_pin,inp(valid,'Object'))
    connect(valid.find_result_pin(),branch.find_condition_pin())
    connect(execute,branch.find_execute_pin());connect(branch.find_else_pin(),fallback)
    return branch.find_then_pin()
def equal(ed,a,b=None,constant=None):
    node=call(ed,'/Script/Engine.KismetMathLibrary.EqualEqual_ObjectObject')
    connect(a,inp(node,'A'))
    if b is not None:connect(b,inp(node,'B'))
    else:value(node,'B',constant)
    return node.find_result_pin()
def dump_graph(bp):
    result={}
    for graph in BEL.list_graphs(bp):
        rows=[];result[graph.get_name()]=rows
        for node in u.BlueprintGraphEditor.get_graph_editor(graph).list_all_nodes():
            rows.append({'name':node.get_name(),'title':str(node.get_node_title()),'pins':[
                {'name':str(p.get_pin_name()),'value':p.get_pin_value(),
                 'links':[{'node':q.get_owning_node().get_name(),'pin':str(q.get_pin_name())} for q in p.list_connected_pins()]}
                for p in node.list_all_pins()]})
    return result

def build_cleanup_state(directory, label, open_idle, mesh):
    assert directory.startswith(DEST + "/")
    STATE = directory + "/ANS_" + label + "Cleanup"
    REPORT = {}
    # load_asset on a missing path can leave an empty UPackage that this factory
    # then rejects as already existing. Check registry existence before loading.
    bp=u.load_asset(STATE) if u.EditorAssetLibrary.does_asset_exist(STATE) else BEL.create_blueprint_asset_with_parent(STATE,u.AnimNotifyState)
    assert bp
    # Const helper is callable from Received_NotifyEnd's const override. All
    # mutations affect the passed weapon component; the notify object is stateless.
    graph=BEL.find_graph(bp,'ResetOwnedWeapon') or BEL.add_function_graph(bp,'ResetOwnedWeapon')
    ed=u.BlueprintGraphEditor.get_graph_editor(graph);entry,result=clear_body(ed)
    if not entry.find_output_pin('Candidate').is_valid():
        ed.add_graph_input_parameter('Candidate',BEL.get_object_reference_type(u.load_class(None,BASE)))
    if not entry.find_output_pin('NotifyOwner').is_valid():
        ed.add_graph_input_parameter('NotifyOwner',BEL.get_object_reference_type(u.Actor))
    ed.set_is_const_function(True)
    candidate=out(entry,'Candidate');owner=out(entry,'NotifyOwner');done=result.find_execute_pin()
    execute=valid_branch(ed,entry.find_then_pin(),candidate,done)
    weapon_owner=call(ed,'/Script/Engine.Actor.GetOwner');connect(candidate,weapon_owner.find_self_pin())
    owns=ed.add_branch_node();connect(execute,owns.find_execute_pin())
    connect(equal(ed,weapon_owner.find_result_pin(),owner),owns.find_condition_pin())
    connect(owns.find_else_pin(),done)
    gun=getter(ed,'MeshFPP',BASE);connect(candidate,gun.find_self_pin())
    execute=valid_branch(ed,owns.find_then_pin(),gun.find_result_pin(),done)
    mesh_asset=call(ed,'/Script/Engine.SkeletalMeshComponent.GetSkeletalMeshAsset')
    connect(gun.find_result_pin(),mesh_asset.find_self_pin())
    matches=ed.add_branch_node();connect(execute,matches.find_execute_pin())
    connect(equal(ed,mesh_asset.find_result_pin(),constant=mesh.get_path_name()),matches.find_condition_pin())
    connect(matches.find_else_pin(),done)
    play=call(ed,'/Script/Engine.SkeletalMeshComponent.PlayAnimation')
    connect(gun.find_result_pin(),play.find_self_pin());connect(matches.find_then_pin(),play.find_execute_pin())
    value(play,'NewAnimToPlay',open_idle.get_path_name());value(play,'bLooping','false')
    connect(play.find_then_pin(),done)
    # Compile the helper before creating calls to its generated signature.
    assert BEL.compile_blueprint(bp),'cleanup helper compile failed'

    graph=BEL.add_function_override(bp,'Received_NotifyEnd');assert graph
    ed=u.BlueprintGraphEditor.get_graph_editor(graph);entry,result=clear_body(ed)
    value(result,'ReturnValue','true')
    done=result.find_execute_pin()
    execute=valid_branch(ed,entry.find_then_pin(),out(entry,'MeshComp'),done)
    owner_node=call(ed,'/Script/Engine.ActorComponent.GetOwner')
    connect(out(entry,'MeshComp'),owner_node.find_self_pin())
    execute=valid_branch(ed,execute,owner_node.find_result_pin(),done)
    component=call(ed,'/Script/Engine.Actor.GetComponentByClass')
    value(component,'ComponentClass',INV)
    connect(owner_node.find_result_pin(),component.find_self_pin())
    execute=valid_branch(ed,execute,component.find_result_pin(),done)
    for slot in ['PrimaryWeapon','SecondaryWeapon']:
        candidate=getter(ed,slot,INV)
        connect(component.find_result_pin(),candidate.find_self_pin())
        reset=call(ed,'ResetOwnedWeapon')
        connect(candidate.find_result_pin(),inp(reset,'Candidate'))
        connect(owner_node.find_result_pin(),inp(reset,'NotifyOwner'))
        connect(execute,reset.find_execute_pin());execute=reset.find_then_pin()
    connect(execute,done)
    compile_save(bp)
    REPORT['notify_state_graphs']=dump_graph(bp)
    return bp.generated_class()
