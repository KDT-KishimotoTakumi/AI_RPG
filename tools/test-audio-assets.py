"""音源原本の署名・長さ・波形・音量を検査する。"""
import hashlib,json,math,pathlib,struct,wave
root=pathlib.Path(__file__).resolve().parent.parent/'Content'/'Audio'/'Source'
manifest=json.loads((root/'manifest.json').read_text(encoding='utf-8'))
assert len(manifest['clips'])==3
for item in manifest['clips']:
    path=root/item['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']
    with wave.open(str(path),'rb') as source:
        assert source.getnchannels()==1 and source.getsampwidth()==2 and source.getframerate()==22050
        samples=struct.unpack('<'+'h'*source.getnframes(),source.readframes(source.getnframes()))
        assert abs(len(samples)/22050-item['seconds'])<.0001
    peak=max(abs(v) for v in samples)/32768
    rms=math.sqrt(sum((v/32768)**2 for v in samples)/len(samples))
    assert .01<rms<.3 and peak<.8 and abs(sum(samples)/len(samples)/32768)<.01
    assert abs(samples[0])<10 and abs(samples[-1])<100
    print(f'{path.name}: duration={len(samples)/22050:.2f}s peak={peak:.3f} rms={rms:.3f}')
