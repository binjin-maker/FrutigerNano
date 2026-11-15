import os, re, random, glob, sys
sys.path.insert(0, '/home/claude/aidata')
from modern_ai import AI as MODERN
from modern_ai2 import AI2
from modern_ai3 import AI3
FINAL = len(sys.argv) > 1 and sys.argv[1] == 'final'
random.seed(7)
HOLD = len(sys.argv) > 1
G = '/home/claude/aidata/gb'

def rd(p):
    try: return open(p, encoding='utf-8', errors='ignore').read()
    except: return ''

def clean(t):
    t = t.replace('\r', '')
    t = re.sub(r'[ \t]+', ' ', t)
    t = re.sub(r'\n{3,}', '\n\n', t)
    return t.strip()

def chunks(t, n=2, lo=40, hi=420):
    """random sentence-aligned spans of varied length"""
    t = clean(t)
    sents = re.split(r'(?<=[.!?])\s+(?=[A-Z"\'])', t)
    out = []
    if len(t.split()) < lo: return [t] if len(t.split()) >= 15 else []
    for _ in range(n):
        target = int(random.choice([random.uniform(lo, 120), random.uniform(80, 250), random.uniform(150, hi)]))
        if len(sents) <= 1: out.append(t); break
        st = random.randrange(len(sents)); seg = []; wc = 0
        i = st
        while i < len(sents) and wc < target:
            seg.append(sents[i]); wc += len(sents[i].split()); i += 1
        if wc < min(lo, len(t.split()) // 2):
            seg = sents[:];
        out.append(' '.join(seg) if '\n' not in ''.join(seg) else ' '.join(seg))
    # keep one with paragraph structure intact
    out.append(' '.join(t.split(' ')[:hi]) if len(t.split()) > hi else t)
    return out

def tex_from_tokens(ws):
    s = ' '.join(ws)
    s = re.sub(r" ([,.;:!?')\]])", r"\1", s); s = re.sub(r"([(\[]) ", r"\1", s)
    s = s.replace("``", '"').replace("''", '"').replace(" n't", "n't").replace(" 's", "'s").replace(" 're", "'re").replace(" 've", "'ve").replace(" 'll", "'ll").replace(" 'm", "'m").replace(" 'd", "'d")
    s = re.sub(r'" (.*?) "', r'"\1"', s)
    return s

docs = []  # (label, text, group)
for dom in ['essay', 'reuter', 'wp']:
    for sub in sorted(os.listdir(f'{G}/{dom}')):
        if sub in ('prompts',): continue
        lab = 0 if sub == 'human' else 1
        files = glob.glob(f'{G}/{dom}/{sub}/*.txt') + glob.glob(f'{G}/{dom}/{sub}/*/*.txt')
        files = [f for f in files if '/logprobs/' not in f]
        random.shuffle(files)
        k = 1000 if lab == 0 else 350
        for f in files[:k]: docs.append((lab, rd(f), f'{dom}'))
for sub in ['bawe', 'ets', 'lang8', 'pelic', 'toefl91']:
    for f in glob.glob(f'{G}/other/{sub}/*.txt')[:700]: docs.append((0, rd(f), 'esl'))
for f in glob.glob(f'{G}/other/gptzero/gpt/*'): docs.append((1, rd(f), 'gptzero'))
for f in glob.glob(f'{G}/other/gptzero/human/*'): docs.append((0, rd(f), 'gptzero'))
for f in glob.glob(f'{G}/other/undetectable/*.txt'): docs.append((1, rd(f), 'undet'))

from nltk.corpus import brown, gutenberg, webtext, nps_chat, movie_reviews, reuters, state_union
for fid in brown.fileids():
    sents = brown.sents(fid)
    for i in range(0, len(sents), 25): docs.append((0, tex_from_tokens([w for s in sents[i:i+25] for w in s]), 'brown'))
for fid in gutenberg.fileids():
    raw = gutenberg.raw(fid); paras = [p for p in raw.split('\n\n') if len(p.split()) > 30]
    random.shuffle(paras)
    for p in paras[:120]: docs.append((0, re.sub(r'\s*\n\s*', ' ', p), 'gutenberg'))
for fid in webtext.fileids():
    lines = webtext.raw(fid).split('\n')
    for i in range(0, len(lines), 12): docs.append((0, '\n'.join(lines[i:i+12]), 'webtext'))
posts = [p.text for p in nps_chat.xml_posts()]
for i in range(0, len(posts), 30): docs.append((0, '\n'.join(posts[i:i+30]), 'chat'))
for fid in movie_reviews.fileids()[::2]:
    t = movie_reviews.raw(fid)
    t = '. '.join(s.strip().capitalize() for s in t.replace('\n', ' ').split(' . '))
    docs.append((0, t, 'movie'))
for fid in reuters.fileids()[:1500]: docs.append((0, reuters.raw(fid), 'reuters'))
for fid in state_union.fileids():
    paras = state_union.raw(fid).split('\n\n')
    for i in range(0, len(paras), 6): docs.append((0, '\n\n'.join(paras[i:i+6]), 'sotu'))
if FINAL:
    for t in MODERN + AI2 + AI3: docs.append((1, t, 'modern'))
    for f in ['/mnt/user-data/uploads/d8428868-attachment.txt', '/mnt/user-data/uploads/9b47a0a2-attachment.txt']: docs.append((1, open(f).read(), 'modern'))
else:
    for t in MODERN + AI2: docs.append((1, t, 'modern'))
    for t in AI3: docs.append((1, t, 'modern_te'))

import pandas as pd
bm = pd.read_parquet('/home/claude/aidata/beemo/dataset.parquet')
for _, r in bm.iterrows():
    if isinstance(r.model_output, str): docs.append((1, r.model_output, 'beemo'))
    if isinstance(r.human_output, str): docs.append((0, r.human_output, 'beemo'))
ke = pd.read_csv('/home/claude/aidata/AI-VS-Human-Generated-Text-Dataset/train_essays.csv')
for _, r in ke.iterrows(): docs.append((int(r.generated), r.text, 'kaggle'))
raw = open('/home/claude/aidata/Ai-humanized-datset/Ai-Humanizer-dataset.txt', encoding='utf-8').read()
for blk in raw.split('\n---'):
    if 'AI_TEXT:' not in blk: continue
    src = re.search(r'SOURCE:\s*(.*)', blk); src = src.group(1).strip().lower() if src else ''
    a = blk.split('AI_TEXT:', 1)[1].split('HUMANIZED_TEXT:')
    g = 'hz_claude' if 'claude' in src else 'hz'
    docs.append((1, a[0].strip(), g))
    if len(a) > 1: docs.append((1, a[1].strip(), g + '_h'))


out_tr = open('/home/claude/aidata/train.rec', 'w'); out_te = open('/home/claude/aidata/test.rec', 'w'); meta = open('/home/claude/aidata/meta.tsv', 'w'); metate = open('/home/claude/aidata/metate.tsv', 'w')
cnt = {}
for lab, t, grp in docs:
    t = clean(t)
    if len(t.split()) < 20: continue
    test = grp == 'modern_te' or (random.random() < 0.15 and grp != 'modern') or (HOLD and not FINAL and grp.startswith('hz_claude'))
    reps = 3 if grp == 'modern' else 1
    for _ in range(reps):
        for c in chunks(t, 2 if lab == 1 or grp in ('esl', 'essay', 'wp') else 1):
            c = c.replace('\x1e', ' ')
            if len(c.split()) < 15: continue
            (out_te if test else out_tr).write(f'\x1e{lab}{c}')
            (meta if not test else metate).write(f'{lab}\t{grp}\n')
            cnt[(grp, lab, test)] = cnt.get((grp, lab, test), 0) + 1
for k in sorted(cnt): print(k, cnt[k])
