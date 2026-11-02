"""既存原本をUE音素材へ取り込む。コマンドレット専用。"""
import pathlib, unreal
root=pathlib.Path(unreal.Paths.project_content_dir()).resolve()/'Audio'/'Source'
items=[('bgm-castle-v1.wav','BGM_Castle_V1',True),('sfx-confirm-v1.wav','SFX_Confirm_V1',False),('sfx-impact-v1.wav','SFX_Impact_V1',False)]
for filename,name,looping in items:
    asset_path='/Game/Audio/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        task=unreal.AssetImportTask();task.filename=str(root/filename);task.destination_path='/Game/Audio';task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset=unreal.load_asset(asset_path)
    if not isinstance(asset,unreal.SoundWave):raise RuntimeError('音源の取込失敗: '+name)
    asset.set_editor_property('sound_asset_compression_type',unreal.SoundAssetCompressionType.PCM)
    asset.set_editor_property('looping',looping)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log('GirlsSpire音源取込: '+name)
