import bpy
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=r"C:\Users\a9018\Desktop\MFMeshs\Optimized\Bedrock.fbx")
mesh = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.name != "Cube"][0]
m = mesh.data
print(f"-- OPTIMIZED FBX STATS --")
print(f"Vertices: {len(m.vertices)}")
print(f"Polygons: {len(m.polygons)}")
print(f"Loops: {len(m.loops)}")
print(f"Materials: {len(m.materials)}")
print(f"UV Layers: {[uv.name for uv in m.uv_layers]}")
has_color = hasattr(m, 'color_attributes')
color_len = len(m.color_attributes) if has_color else len(m.vertex_colors)
print(f"Vertex Colors: {color_len}")
