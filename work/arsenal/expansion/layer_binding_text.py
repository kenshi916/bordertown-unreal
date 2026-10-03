"""Strict parser for the ordinary UObject T3D property-binding export.

This reads metadata only. It never imports text or writes a Blueprint.
"""
import json,re

def split_fields(text):
    result=[];start=0;depth=0;quoted=False;escaped=False
    for i,char in enumerate(text):
        if escaped:escaped=False;continue
        if quoted and char=='\\':escaped=True;continue
        if char=='"':quoted=not quoted;continue
        if quoted:continue
        if char=='(':depth+=1
        elif char==')':
            depth-=1;assert depth>=0,'Unbalanced binding text'
        elif char==',' and depth==0:
            result.append(text[start:i].strip());start=i+1
    assert depth==0 and not quoted,'Incomplete binding text'
    if text[start:].strip():result.append(text[start:].strip())
    return result

def items(text):
    text=text.strip();assert text.startswith('(') and text.endswith(')'),text
    return split_fields(text[1:-1])

def parse_binding_export(text,binding_path):
    header=text.splitlines()[0]
    assert 'Class=/Script/AnimGraph.AnimGraphNodeBinding_Base ' in header,header
    assert 'ExportPath="/Script/AnimGraph.AnimGraphNodeBinding_Base\''+binding_path+'\'"' in header,'Binding export object mismatch'
    lines=re.findall(r'^\s*PropertyBindings=(.*)$',text,re.M)
    assert len(lines)<=1,'Ambiguous PropertyBindings export'
    result={}
    if not lines:return result
    for entry in items(lines[0]):
        pair=items(entry);assert len(pair)==2,pair
        key=json.loads(pair[0]);assert isinstance(key,str)
        fields={}
        for field in items(pair[1]):
            name,value=field.split('=',1);assert name not in fields,name
            fields[name]=value
        name=json.loads(fields.get('PropertyName','"None"'))
        route=[json.loads(x)for x in items(fields.get('PropertyPath','()'))]
        bound=fields.get('bIsBound','False');assert bound in ['True','False']
        assert key not in result,key
        result[key]={'property_name':name,'property_path':route,'is_bound':bound=='True',
            'type':fields.get('Type','default_property'),'text':pair[1],
            'read_method':'native_uobject_text_export'}
    return result
