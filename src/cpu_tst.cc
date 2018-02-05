#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>

class NetRecord {
public:
  uint64_t pulseId;
  uint64_t timestamp;
  uint64_t init;
  uint64_t acquire;
  uint64_t avgdone;
  uint64_t update;
  uint32_t channel[31];
};

class NetBsaChannel {
public:
  unsigned n;
  double sum;
  double sqsum;
  unsigned m;
  unsigned nacq[1<<15];
  double mean[1<<15], rms2[1<<15];
public:
  void init() { n=0; sum=0; sqsum=0; m=0; }
  void acquire(unsigned v) { n++; sum+=double(v); sqsum+=(double(v)*double(v)); }
  void avgdone() {
    double mn=0, r2=0;
    if (n) {
      mn = sum/double(n);
      if (n>1) 
        r2 = (sqsum - sum*mn)/double(n-1);
    }
    nacq[m] = n;
    mean[m] = mn;
    rms2[m] = r2;
    m++;
    n=0; sum=0; sqsum=0;
  }
};      

class NetBsa {
public:
  NetBsaChannel channel[31];
public:
  void init() { for(unsigned i=0; i<31; i++) channel[i].init(); }
  void acquire(const uint32_t* ch, bool avgdone) {
    //    printf("ch[0] %u\n",ch[0]);
    for(unsigned i=0; i<31; i++) {
      channel[i].acquire(ch[i]);
      if (avgdone) channel[i].avgdone();
    }
  }
};

int main(int argc, char* argv[])
{
  NetBsa* bsa = new NetBsa[64];

  //  Prepare inputs
  unsigned nrecords = 1<<20;
  uint64_t pulseId   = 0;
  uint64_t timestamp = 0;

  NetRecord* buff = new NetRecord[nrecords];
  buff[0].pulseId   = pulseId++;
  buff[0].timestamp = timestamp;
  buff[0].init      = -1ULL;
  buff[0].acquire   = 0;
  buff[0].avgdone   = 0;
  buff[0].update    = 0;
  for(unsigned j=0; j<31; j++)
    buff[0].channel[j] = 0;

  for(unsigned i=1; i<nrecords; i++) {
    buff[i].pulseId   = pulseId++;
    buff[i].timestamp = (timestamp += 1077);  // ns per pulse
    buff[i].init      = 0;
    uint64_t m = i^(i-1);
    if (m&0x10)
      m |= 0xFFFFFFFFFFFF0;
    buff[i].acquire   = m;
    buff[i].avgdone   = (i&0x1f)==0x1f ? -1ULL : 0;
    buff[i].update    = 0;
    for(unsigned j=0; j<31; j++)
      buff[i].channel[j] = i;
  }

  timespec begin;
  clock_gettime(CLOCK_REALTIME,&begin);

  for(unsigned i=0; i<nrecords; i++) {
    for(unsigned j=0; j<64; j++) {
      if (buff[i].init & (1ULL<<j))
        bsa[j].init();
      else if (buff[i].acquire & (1ULL<<j)) 
        bsa[j].acquire(buff[i].channel, buff[i].avgdone & (1ULL<<j));
    }
  }

  timespec end;
  clock_gettime(CLOCK_REALTIME,&end);
  double dt = double(end.tv_sec-begin.tv_sec) +
    1.e-9*(double(end.tv_nsec)-double(begin.tv_nsec));
  printf("%f sec\n",dt);

  printf("idx %u  n %u  avg %f  rms2 %f\n", 
         bsa[0].channel[0].m, 
         bsa[0].channel[0].nacq[0], 
         bsa[0].channel[0].mean[0], 
         bsa[0].channel[0].rms2[0]);
        
  return 1;
}
