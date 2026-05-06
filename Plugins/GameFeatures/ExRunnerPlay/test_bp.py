import unreal
import json

def analyze_bp(path):
    info = {"Path": path, "Error": None, "Class": None, "Parent": None, "Components": [], "Functions": []}
    try:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            info["Error"] = f"Asset not found."
            return info
            
        gen_class = asset.generated_class()
        if not gen_class:
            info["Error"] = "Generated class not found."
            return info
            
        info["Class"] = gen_class.get_name()
        
        parent = gen_class.get_super_class()
        if parent:
            info["Parent"] = parent.get_name()
            
        cdo = unreal.get_default_object(gen_class)
        
        # Get components if possible
        if hasattr(cdo, "get_components_by_class"):
            comps = cdo.get_components_by_class(unreal.ActorComponent)
            info["Components"] = [c.get_name() for c in comps]
            
        # Get functions
        funcs = [f for f in dir(gen_class) if not f.startswith('_')]
        info["Functions"] = funcs
        
    except Exception as e:
        info["Error"] = str(e)
    return info

data1 = analyze_bp("/ExRunnerPlay/BluePrint/ExRunnerCharacter_Mover_Child")
data2 = analyze_bp("/ExCore/BluePrint/ExSandboxCharacter_Mover")

with open("C:/wz/ExFrameWork/Plugins/GameFeatures/ExRunnerPlay/python_output.txt", "w", encoding="utf-8") as f:
    f.write(json.dumps([data1, data2], indent=2))
