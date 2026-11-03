"""V1を保持したままV2をPCM音素材として取り込む。"""
import pathlib, unreal
root=pathlib.Path(unreal.Paths.project_content_dir()).resolve()/'Audio'/'Source'
for filename,name,loop in [('bgm-castle-v2.wav','BGM_Castle_V2',True),('sfx-confirm-v2.wav','SFX_Confirm_V2',False),('sfx-impact-v2.wav','SFX_Impact_V2',False)]:
    path='/Game/Audio/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task=unreal.AssetImportTask();task.filename=str(root/filename);task.destination_path='/Game/Audio';task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset=unreal.load_asset(path)
    if not isinstance(asset,unreal.SoundWave):raise RuntimeError('V2音素材の取込失敗: '+name)
    asset.set_editor_property('sound_asset_compression_type',unreal.SoundAssetCompressionType.PCM);asset.set_editor_property('looping',loop)
    unreal.EditorAssetLibrary.save_loaded_asset(asset);unreal.log('GirlsSpire V2取込: '+name)
