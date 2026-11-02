"""プロジェクト専用の初期音素材を、外部音源なしで生成する。"""
import hashlib, json, math, pathlib, struct, wave
ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'Content' / 'Audio' / 'Source'
RATE = 22050

def emit(name, seconds, sample):
    frames = round(seconds * RATE)
    values = [max(-32767, min(32767, round(sample(i / RATE) * 32767))) for i in range(frames)]
    raw = struct.pack('<' + 'h' * len(values), *values)
    path = OUT / (name + '.wav')
    # 既存原本を無断で置換しない。
    if path.exists():
        with wave.open(str(path), 'rb') as existing:
            if existing.readframes(existing.getnframes()) != raw:
                raise RuntimeError('既存音素材が生成値と異なる: ' + path.name)
    else:
        with wave.open(str(path), 'wb') as output:
            output.setnchannels(1); output.setsampwidth(2); output.setframerate(RATE); output.writeframes(raw)
    return {'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'seconds': seconds, 'sample_rate': RATE}

def castle(t):
    beat = .625
    score = [62, 69, 67, 65, 62, 60, 62, 69, 74, 72, 69, 67, 65, 67, 62, 60,
             62, 65, 69, 72, 74, 69, 67, 65, 62, 60, 62, 67, 65, 62, 60, 62]
    sound = 0.
    for step in range(max(0, int(t/beat)-3), min(32, int(t/beat)+1)):
        age = t-step*beat
        frequency = 440*2**((score[step]-69)/12)
        attack = min(1., age/.008)
        sound += .11*attack*math.exp(-3*age)*(math.sin(2*math.pi*frequency*age)+.3*math.sin(4*math.pi*frequency*age)+.12*math.sin(6*math.pi*frequency*age))
    age = t % (beat*4)
    sound += .06*math.exp(-14*age)*math.sin(2*math.pi*(75*age-35*age*age))
    sound += .018*math.sin(2*math.pi*146.832384*t)
    return sound*min(1.,t/.025,(20-t)/.025)

def confirm(t):
    envelope = math.sin(math.pi*t/.16)**2
    return .19*envelope*(math.sin(2*math.pi*880*t)+.3*math.sin(2*math.pi*1320*t))

def impact(t):
    envelope = min(1.,t/.003)*math.exp(-24*t)*min(1.,(.24-t)/.015)
    return .2*envelope*(math.sin(2*math.pi*(150*t-170*t*t))+.35*math.sin(2*math.pi*1777*t)*math.sin(2*math.pi*2833*t))

if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    entries = [emit('bgm-castle-v1',20,castle), emit('sfx-confirm-v1',.16,confirm), emit('sfx-impact-v1',.24,impact)]
    (OUT/'manifest.json').write_text(json.dumps({'version':1,'method':'本プロジェクト専用の数式合成。外部音源なし。', 'clips':entries},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('初期音素材3件を生成/原本照合')
