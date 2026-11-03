"""V2の実波形・署名・ステレオ・ループ端・音量を確認する。"""
import array, hashlib, json, math, pathlib, wave
root=pathlib.Path(__file__).resolve().parent.parent/'Content'/'Audio'/'Source'
manifest=json.loads((root/'manifest-v2.json').read_text(encoding='utf-8'))
assert len(manifest['clips'])==3
for clip in manifest['clips']:
    path=root/clip['file'];assert hashlib.sha256(path.read_bytes()).hexdigest()==clip['sha256']
    with wave.open(str(path),'rb') as audio:
        assert audio.getnchannels()==2 and audio.getsampwidth()==2 and audio.getframerate()==32000
        data=array.array('h',audio.readframes(audio.getnframes()))
    assert abs(len(data)/64000-clip['seconds'])<.0001
    rms=math.sqrt(sum((v/32768)**2 for v in data)/len(data));peak=max(abs(v) for v in data)/32768
    assert .015<rms<.3 and peak<.65 and abs(sum(data)/len(data)/32768)<.005
    assert any(data[i]!=data[i+1] for i in range(0,len(data),2))
    if clip['loop']:assert max(abs(data[ch]-data[-2+ch])/32768 for ch in range(2))<.05
    else:assert data[:2]==array.array('h',[0,0]) and data[-2:]==array.array('h',[0,0])
    print(f'{path.name}: {clip["seconds"]}秒 stereo peak={peak:.3f} rms={rms:.3f}')
