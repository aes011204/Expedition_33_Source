import os
import shutil

def main():
    map_dir = r"..\Client\Bin\Resources\Map"
    map_dir = os.path.abspath(map_dir)

    print(f"Searching for 'Textures' folders in: {map_dir}")
    
    total_copied = 0
    mixed_files = []

    for root, dirs, files in os.walk(map_dir):
        # Textures 폴더 내부인 경우에만 처리
        if os.path.basename(root).lower() == "textures":
            target_dir = os.path.join(root, "Normal")
            
            if not os.path.exists(target_dir):
                os.makedirs(target_dir)
                
            for file in files:
                if file.lower().endswith(".dds") or file.lower().endswith(".png"):
                    lower_name = file.lower()
                    
                    is_normal = False
                    if "normal" in lower_name:
                        is_normal = True
                    elif "_n." in lower_name or "-n." in lower_name:
                        is_normal = True
                    elif "_nor." in lower_name:
                        is_normal = True
                        
                    if is_normal:
                        src_path = os.path.join(root, file)
                        dest_path = os.path.join(target_dir, file)
                        
                        is_mixed = False
                        if "ao" in lower_name or "occlusion" in lower_name:
                            is_mixed = True
                        
                        if is_mixed:
                            mixed_files.append(os.path.join(root, file))
                            
                        if not os.path.exists(dest_path):
                            shutil.copy2(src_path, dest_path)
                        total_copied += 1

    print(f"\nTotal Normal maps copied: {total_copied}")
    
    if mixed_files:
        print("\n[WARNING] Found files that might contain mixed Normal and AO:")
        for mf in mixed_files:
            print(" - " + mf)
    else:
        print("\n[CLEAN] No mixed Normal+AO files were found.")

if __name__ == "__main__":
    main()
    input("\nPress Enter to exit...")
