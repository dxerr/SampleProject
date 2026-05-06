import unreal

task1 = unreal.AssetExportTask()
task1.object = unreal.EditorAssetLibrary.load_asset("/ExRunnerPlay/BluePrint/ExRunnerCharacter_Mover_Child")
task1.filename = "C:/wz/ExFrameWork/Plugins/GameFeatures/ExRunnerPlay/BP_Child.copy"
task1.automated = True
task1.options = unreal.T3DExporter()

task2 = unreal.AssetExportTask()
task2.object = unreal.EditorAssetLibrary.load_asset("/ExCore/BluePrint/ExSandboxCharacter_Mover")
task2.filename = "C:/wz/ExFrameWork/Plugins/GameFeatures/ExRunnerPlay/BP_Parent.copy"
task2.automated = True
task2.options = unreal.T3DExporter()

unreal.Exporter.run_asset_export_task(task1)
unreal.Exporter.run_asset_export_task(task2)
