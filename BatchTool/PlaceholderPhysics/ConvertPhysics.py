import json
import sys
import os

def main():
    if len(sys.argv) < 3:
        print("Usage: python ConvertPhysics.py [InputJSON] [OutputJSON]")
        return
        
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    
    if not os.path.exists(input_path):
        print(f"Error: Input file not found: {input_path}")
        return
        
    with open(input_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
        
    out_colliders = []
    
    for item in data:
        if item.get("Type") == "SkeletalBodySetup":
            props = item.get("Properties", {})
            bone_name = props.get("BoneName", "Unknown")
            agg_geom = props.get("AggGeom", {})
            
            # We only extract SphylElems (Capsules) for cloth collision
            sphyls = agg_geom.get("SphylElems", [])
            for sphyl in sphyls:
                center = sphyl.get("Center", {"X": 0.0, "Y": 0.0, "Z": 0.0})
                rotation = sphyl.get("Rotation", {"Pitch": 0.0, "Yaw": 0.0, "Roll": 0.0})
                radius = sphyl.get("Radius", 0.0)
                length = sphyl.get("Length", 0.0)
                
                collider = {
                    "boneName": bone_name,
                    "center": {"x": center.get("X", 0.0), "y": center.get("Y", 0.0), "z": center.get("Z", 0.0)},
                    "rotation": {"pitch": rotation.get("Pitch", 0.0), "yaw": rotation.get("Yaw", 0.0), "roll": rotation.get("Roll", 0.0)},
                    "radius": radius,
                    "length": length
                }
                out_colliders.append(collider)
                
    # Create the output directory if it doesn't exist
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(out_colliders, f, indent=4)
        
    print(f"Successfully converted {len(out_colliders)} capsule colliders to {output_path}")

if __name__ == '__main__':
    main()
