import json
import sys

f = r'D:\Clair Obscur Expedition 33\Exports\Sandfall\Content\Characters\Heros\Lune\ABP_PostProcess_Lune_V2.json'
try:
    with open(f, 'r', encoding='utf-8') as file:
        data = json.load(file)
        for obj in data:
            if obj.get('Type') == 'AnimGraphNode_KawaiiPhysics':
                props = obj.get('Properties', {})
                node = props.get('Node', {})
                root_bone = node.get('RootBone', {}).get('BoneName', '')
                print(root_bone)
except Exception as e:
    print(e)
