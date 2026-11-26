import sys; sys.argv=['x', sys.argv[1] if len(sys.argv) > 1 else '.']
exec(open('mixdown.py').read().split("lead, dry, pad, kick =")[0])
x = rd("export_check.wav")
b=lambda n:n*BAR
xk = kweight(x)
e=lambda v:10*np.log10(np.mean(v**2)+1e-20)
def hf(s):
    M=(s[:,0]+s[:,1])/2; X=np.abs(np.fft.rfft(M*np.hanning(len(M))))**2; f=np.fft.rfftfreq(len(M),1/SR)
    return 10*np.log10(X[f>7000].sum()/X.sum())
print("EXPORT (AAC decoded)  loudness  width(side/mid)  air>7k   hall in rest beat   mono loss  L/R corr")
for name,b0 in [("DRY",2),("30%",4),("70%",6),("100%",8),("A/B DRY",12),("A/B EUPH",14),("A/B DRY 2",16),("A/B EUPH 2",18)]:
    s=x[int((b(b0)+.35)*SR):int(b(b0+2)*SR)]
    M,S=(s[:,0]+s[:,1])/2,(s[:,0]-s[:,1])/2
    r0=b(b0+2)-60/152+0.25; r1=b(b0+2)-0.02
    rest=x[int(r0*SR):int(r1*SR)]
    print(f"{name:12s} {loud(xk, b(b0)+.35, b(b0+2)):8.2f} {e(S)-e(M):12.1f} dB {hf(s):8.1f} {e(rest):14.1f} dB {e(M)-e(s.reshape(-1)):10.2f} {np.corrcoef(s[:,0],s[:,1])[0,1]:9.2f}")
# ducking: hall level right after each kick vs just before the next one, 100 % section, side channel (kick and pad are mono)
seg=x[int(b(8)*SR):int(b(12)*SR)]; S=(seg[:,0]-seg[:,1])/2
beat=60/152; early=[];late=[]
for k in range(16):
    t0=int(k*beat*SR); early.append(e(S[t0+int(0.03*SR):t0+int(0.10*SR)])); late.append(e(S[t0+int(0.28*SR):t0+int(0.36*SR)]))
print(f"ducking on the hall (side), 100 %: just after the kick {np.mean(early):.1f} dB, end of beat {np.mean(late):.1f} dB")
