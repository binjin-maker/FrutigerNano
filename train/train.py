import numpy as np, sys, json, math
from sklearn.feature_extraction.text import CountVectorizer
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import roc_auc_score, accuracy_score
import scipy.sparse as sp

def load(p):
    y, D, T = [], [], []
    for line in open(p, encoding='utf-8'):
        a = line.rstrip('\n').split('\t')
        if len(a) < 3: continue
        y.append(int(a[0])); D.append([float(x) for x in a[1].split(',')]); T.append(a[2])
    return np.array(y), np.array(D), T

ytr, Dtr, Ttr = load('train.tsv'); yte, Dte, Tte = load('test.tsv')
meta = [l.split('\t')[1].strip() for l in open('meta.tsv')]
print('train', len(ytr), ytr.mean(), 'test', len(yte), yte.mean())
MW = float(sys.argv[3]) if len(sys.argv) > 3 else 4.0
w = np.ones(len(ytr))
for i, g in enumerate(meta):
    if g == 'modern': w[i] = MW
w[ytr == 0] *= (ytr == 1).sum() / (ytr == 0).sum()

def ngr(doc):
    t = ['<s>'] + doc.split(' ')
    return t[1:] + [t[i] + ' ' + t[i + 1] for i in range(len(t) - 1)]

cv = CountVectorizer(analyzer=ngr, min_df=25, max_features=40000)
Xtr = cv.fit_transform(Ttr); Xte = cv.transform(Tte)
vocab = cv.get_feature_names_out()
print('vocab', len(vocab))

def tfnorm(X):
    X = X.astype(np.float64).tocsr(); X.data = 1 + np.log(X.data)
    n = np.sqrt(np.asarray(X.multiply(X).sum(1)).ravel()); n[n == 0] = 1
    return sp.diags(1 / n) @ X

mu = Dtr.mean(0); sd = Dtr.std(0) + 1e-6
def dz(D): return np.clip((D - mu) / sd, -5, 5)

# 1) pattern algorithm: dense features only
alg = LogisticRegression(C=1.0, max_iter=3000)
alg.fit(dz(Dtr), ytr, sample_weight=w)
pa = alg.predict_proba(dz(Dte))[:, 1]
print('ALGO   auc %.4f acc %.4f' % (roc_auc_score(yte, pa), accuracy_score(yte, pa > .5)))

# 2) select n-grams with L1
X1 = sp.hstack([tfnorm(Xtr) * 3, sp.csr_matrix(dz(Dtr))]).tocsr()
l1 = LogisticRegression(penalty='l1', C=float(sys.argv[1]) if len(sys.argv) > 1 else 2.0, solver='liblinear', max_iter=2000)
l1.fit(X1, ytr, sample_weight=w)
keep = np.where(np.abs(l1.coef_[0][:len(vocab)]) > 1e-6)[0]
print('kept ngrams', len(keep))
Xk = Xtr[:, keep]; Xke = Xte[:, keep]
X2 = sp.hstack([tfnorm(Xk) * 3, sp.csr_matrix(dz(Dtr))]).tocsr()
X2e = sp.hstack([tfnorm(Xke) * 3, sp.csr_matrix(dz(Dte))]).tocsr()
mdl = LogisticRegression(C=float(sys.argv[2]) if len(sys.argv) > 2 else 4.0, max_iter=5000)
mdl.fit(X2, ytr, sample_weight=w)
pm = mdl.predict_proba(X2e)[:, 1]
print('MODEL  auc %.4f acc %.4f' % (roc_auc_score(yte, pm), accuracy_score(yte, pm > .5)))
for a in (0.5, 0.7, 0.85):
    pc = a * pm + (1 - a) * pa
    print('BLEND %.2f auc %.4f acc %.4f' % (a, roc_auc_score(yte, pc), accuracy_score(yte, pc > .5)))
# FPR / TPR at 0.5
print('model FPR %.3f TPR %.3f' % ((pm[yte == 0] > .5).mean(), (pm[yte == 1] > .5).mean()))
json.dump({'mu': mu.tolist(), 'sd': sd.tolist(),
           'alg_w': alg.coef_[0].tolist(), 'alg_b': float(alg.intercept_[0]),
           'ng': [vocab[i] for i in keep], 'ng_w': mdl.coef_[0][:len(keep)].tolist(),
           'd_w': mdl.coef_[0][len(keep):].tolist(), 'b': float(mdl.intercept_[0])}, open('model.json', 'w'))
mt = [l.split('\t')[1].strip() for l in open('metate.tsv')]
import collections
acc = collections.defaultdict(list)
for g, yy, p1, p2 in zip(mt, yte, pm, pa): acc[(g, yy)].append(((p1 > .5) == yy, (p2 > .5) == yy))
for k in sorted(acc): a = np.array(acc[k]); print('%-14s y=%d n=%5d model %.3f algo %.3f' % (k[0], k[1], len(a), a[:, 0].mean(), a[:, 1].mean()))
mte = np.array(mt)
for a in (1.0, 0.7, 0.5, 0.35):
    pc = a * pm + (1 - a) * pa
    m = mte == 'modern_te'; hum = yte == 0
    print('blend %.2f: acc %.4f FPR %.3f TPR %.3f modern_te %.3f hz_claude %.3f essay-human %.3f wp-human %.3f' % (a, accuracy_score(yte, pc > .5), (pc[hum] > .5).mean(), (pc[yte == 1] > .5).mean(),
          (pc[m] > .5).mean(), (pc[mte == 'hz_claude'] > .5).mean(), (pc[(mte == 'essay') & hum] < .5).mean(), (pc[(mte == 'wp') & hum] < .5).mean()))
