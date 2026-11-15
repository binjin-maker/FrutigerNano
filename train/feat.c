#include <stdarg.h>
#include <math.h>
#include "../test/hdr.h"
#include "../src/detect.inc"
int main(int argc,char**argv){ setlocale(LC_ALL,"C.UTF-8");
  FILE*f=fopen(argv[1],"r"); FILE*o=fopen(argv[2],"w");
  Str s={0}; sAdd(&s,L""); wint_t c; long cnt=0;
  for(;;){ c=fgetwc(f);
    if(c==WEOF||c==0x1e){ if(s.s[0]){ wchar_t lab=s.s[0]; wchar_t*t=s.s+1; double F[DT_NF]; dtF=F; Detect d; dtAnalyze(t,&d); dtF=NULL; dtFree(&d);
        fprintf(o,"%lc\t",lab); for(int k=0;k<DT_NF;k++) fprintf(o,"%s%.5g",k?",":"",F[k]); fprintf(o,"\t");
        wchar_t**v; int n=dtTokens(t,&v); for(int k=0;k<n;k++){ fprintf(o,"%s%ls",k?" ":"",v[k]); free(v[k]); } free(v); fprintf(o,"\n");
        if(++cnt%2000==0) fprintf(stderr,"%ld\n",cnt); }
      free(s.s); s.s=NULL; s.n=s.cap=0; sAdd(&s,L""); if(c==WEOF) break; continue; }
    sAddC(&s,(wchar_t)c); }
  fclose(o); }
