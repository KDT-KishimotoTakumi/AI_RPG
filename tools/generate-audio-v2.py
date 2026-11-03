"""専用の戦術RPG曲と効果音V2。外部曲・録音・音色ライブラリは使用しない。"""
import array, hashlib, json, math, pathlib, random, struct, wave

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / 'Content' / 'Audio' / 'Source'
RATE, SECONDS, BEAT = 32000, 40, .625
N = RATE * SECONDS
TAU = math.tau

def buffers(count):
    return array.array('f', [0]) * count, array.array('f', [0]) * count

def note(channels, start, midi, duration, gain, pan, voice):
    frequency = 440 * 2 ** ((midi - 69) / 12)
    begin, count = round(start * RATE), round(duration * RATE)
    gl, gr = gain * math.sqrt((1-pan)/2), gain * math.sqrt((1+pan)/2)
    for i in range(count):
        t = i / RATE
        edge = min(1., t/.018, (duration-t)/.12)
        phase = TAU * frequency * t
        if voice == 'strings':
            value = edge * (.64*math.sin(phase) + .22*math.sin(phase*1.003) + .14*math.sin(phase*2))
        elif voice == 'horn':
            value = edge * math.exp(-.75*t) * (math.sin(phase) + .24*math.sin(phase*2) + .09*math.sin(phase*3))
        elif voice == 'koto':
            value = edge * math.exp(-6*t) * (math.sin(phase) + .4*math.sin(phase*2) + .15*math.sin(phase*4))
        else:
            value = edge * math.exp(-1.8*t) * (math.sin(phase) + .15*math.sin(phase*2))
        at = (begin+i) % len(channels[0])
        channels[0][at] += value*gl
        channels[1][at] += value*gr

def drum(channels, start, kind, gain, rng):
    duration = .36 if kind == 'taiko' else .13
    for i in range(round(duration*RATE)):
        t = i/RATE
        if kind == 'taiko':
            value = (math.sin(TAU*(85*t-55*t*t))+.16*rng.uniform(-1,1))*math.exp(-15*t)
        else:
            value = rng.uniform(-1,1)*math.exp(-40*t)+.2*math.sin(TAU*1800*t)*math.exp(-30*t)
        value *= min(1.,t/.002,(duration-t)/.02)*gain
        at = (round(start*RATE)+i) % len(channels[0])
        channels[0][at] += value*.7
        channels[1][at] += value*.7

def music():
    channels=buffers(N);rng=random.Random(643)
    chords=[(50,53,57,62),(46,50,53,58),(53,57,60,65),(48,52,55,60)]
    phrases=[(74,77,81,79),(74,77,82,81),(72,77,81,79),(72,76,79,74)]
    answers=[(77,74,72,69),(77,74,72,70),(77,76,72,69),(76,74,72,69)]
    for bar in range(16):
        chord=chords[bar%4];start=bar*4*BEAT
        for pitch,pan in zip(chord,(-.7,-.25,.25,.7)):
            note(channels,start,pitch,3.15,.035,pan,'strings')
        for beat in range(4):
            phrase=phrases[bar%4] if (bar//4)%2==0 else answers[bar%4]
            note(channels,start+beat*BEAT,phrase[beat],.79,.11,0,'horn')
            if beat%2==0:note(channels,start+beat*BEAT,chord[0]-12,1.15,.08,0,'bass')
            drum(channels,start+beat*BEAT,'taiko' if beat%2==0 else 'rim',.11 if beat%2==0 else .028,rng)
        for eighth in range(8):
            note(channels,start+eighth*BEAT/2,chord[(eighth+bar)%4]+12,.66,.064,-.45 if eighth%2==0 else .45,'koto')
    # 前周回の残響も含めて周期的に混ぜ、ループ端で余韻を切らない。
    dry=(array.array('f',channels[0]),array.array('f',channels[1]))
    for delay,gain in ((.17,.13),(.29,.09),(.43,.055)):
        offset=round(delay*RATE)
        for i in range(N):
            channels[0][i] += dry[1][(i-offset)%N]*gain
            channels[1][i] += dry[0][(i-offset)%N]*gain
    return channels

def effect(impact):
    seconds=.24 if impact else .18
    channels=buffers(round(seconds*RATE));rng=random.Random(644 if impact else 645)
    if impact:
        drum(channels,0,'taiko',.5,rng)
        for i in range(len(channels[0])):
            t=i/RATE;noise=rng.uniform(-1,1)*math.exp(-22*t)*min(1,t/.003,(seconds-t)/.015)*.25
            channels[0][i]+=noise;channels[1][i]+=noise*.85
    else:
        note(channels,0,86,.18,.35,-.2,'koto')
        note(channels,.035,93,.14,.22,.2,'koto')
    return channels

def emit(name,channels,loop,peak_target):
    count=len(channels[0]);means=[sum(c)/count for c in channels]
    peak=max(max(abs(v-means[ch]) for v in channels[ch]) for ch in range(2))
    samples=array.array('h')
    for i in range(count):
        edge=1 if loop else min(1,i/(RATE*.004),(count-1-i)/(RATE*.008))
        for ch in range(2):samples.append(round((channels[ch][i]-means[ch])*edge/peak*peak_target*32767))
    raw=samples.tobytes();path=OUT/(name+'.wav')
    if path.exists():
        with wave.open(str(path),'rb') as old:
            if old.readframes(old.getnframes())!=raw:raise RuntimeError('既存原本を上書きしない: '+path.name)
    else:
        with wave.open(str(path),'wb') as output:
            output.setnchannels(2);output.setsampwidth(2);output.setframerate(RATE);output.writeframes(raw)
    return {'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'seconds':count/RATE,'sample_rate':RATE,'channels':2,'loop':loop,'rms':math.sqrt(sum((v/32768)**2 for v in samples)/len(samples))}

if __name__=='__main__':
    OUT.mkdir(parents=True,exist_ok=True)
    clips=[emit('bgm-castle-v2',music(),True,.58),emit('sfx-confirm-v2',effect(False),False,.38),emit('sfx-impact-v2',effect(True),False,.48)]
    (OUT/'manifest-v2.json').write_text(json.dumps({'version':2,'method':'独自の64拍。弦/金管風旋律/琴風分散和音/低音/和太鼓風打楽器/ステレオ残響を数式合成。外部曲・音源なし。','clips':clips},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(clips,ensure_ascii=False))
