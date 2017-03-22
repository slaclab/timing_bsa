//
//  Utility to control BSA
//
//  Record is 128 bytes length:
//    PulseID     :   8 bytes
//    NWords      :   4 bytes
//    ?           :   4 bytes
//    Sensor data : 112 bytes
//
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <semaphore.h>
#include <new>
#include <arpa/inet.h>
#include <time.h>

#include <AmcCarrier.hh>
#include <AmcCarrierYaml.hh>

using Bsa::BeamSelect;
using Bsa::RateSelect;

class MyCallback : public Bsa::AmcCarrierCallback {
public:
  MyCallback(Bsa::AmcCarrier& host,
             unsigned    array) : 
    _host (host),
    _array(array),
    _stat (0) {}
public:
  int process(uint64_t complete) {
    printf("complete %lx\n",complete);
    return (complete&(1ULL<<_array))==0;
  }
  int processTmo() {
    unsigned v = _host.status(_array);
    if (v!=_stat) {
      printf("BsaStat: %x\n",_stat=v);
    }
    return 1;
  }
private:
  Bsa::AmcCarrier& _host;
  unsigned         _array;
  unsigned         _stat;
};

static void dump_bsa(const Bsa::Record& record,
                     unsigned chanmask = 0xffffffff)
{
  time_t s = record.time_secs;
  printf("array[%x]  tstamp[%11u.%09u]  %s\n", 
         record.buffer,
         record.time_secs, record.time_nsecs,
         ctime(&s));
  for(unsigned i=0; i<record.entries.size(); i++) {
    const Bsa::Entry& e = record.entries[i];
    uint64_t pid = e.pulseId();
    unsigned nch = e.nchannels();
    printf("pulseId[%016lx] nch[%08x]\n",pid,nch);
    for(unsigned i=0; i<nch; i++)
      if ((1<<i)&chanmask)
        printf("%08x:%08x:%08x  n[%u]  avg[%f]  rms2[%f]\n",
               e.channel_data[i].data[0],
               e.channel_data[i].data[1],
               e.channel_data[i].data[2],
               e.channel_data[i].n(),
               e.channel_data[i].mean(),
               e.channel_data[i].rms2());
    printf("\n");
  }
}

static void show_usage(const char* p)
{
  printf("** Collect BSA data **\n");
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <IP address dotted notation> : set carrier IP\n");
  printf("         -i <ndiag>                      : initialize BSA and diagnostics DRAM\n");
  printf("         -g <rate>,<nacq>,<nacc>,<buffer>: acquire <nacq> events at <rate> into <buffer>\n");
  printf("                                           rate=0 (1MHz), rate=1 (71kHz), rate=2 (10kHz),\n"
         "                                           rate=3 (1kHz), rate=4 (100Hz), rate=5 (10Hz),\n"
         "                                           rate=6 (1Hz)\n"
         "                                           rate=-1 (360HzA), rate=-2 (180HzA)\n");
  printf("         -f <filename>                   : write BSA to file (otherwise screen)\n");
  printf("         -F <array>                      : force fetch of BSA array\n");
  printf("         -D <begin,end>                  : fetch DRAM\n");
  printf("         -d <buffer>                     : write diagnostics to file and re-arm\n");
  printf("         -c <channel mask>               : bit mask of channels to dump\n");
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  unsigned array=0;
  uint64_t fetch=0;
  const char* filename=0;
  int      rate=0;
  unsigned nacq=0;
  unsigned naccum=1;
  uint64_t begin=0,end=0;
  unsigned ndiag=0;
  unsigned channelMask = 0xffffffff;

  bool lInit=false;
  bool lTPG =false;
  bool lDiag=false;
  bool lNoFetch=false;

  char* endPtr;
  int c;
  while( (c=getopt(argc,argv,"a:c:d:i:g:f:D:F:GN"))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg; break;
    case 'c':
      channelMask = strtoul(optarg, &endPtr, 0);
      break;
    case 'd':
      lDiag=true;
      ndiag = strtoul(optarg, &endPtr, 0);
      break;
    case 'i':
      lInit=true;
      if (optarg) ndiag = strtoul(optarg, &endPtr, 0);
      break;
    case 'f':
      filename=optarg; break;
    case 'g':
      rate  = strtol (optarg  ,&endPtr,0);
      nacq  = strtoul(endPtr+1,&endPtr,0);
      if (*endPtr==',') {
        naccum  = strtoul(endPtr+1,&endPtr,0);
        if (*endPtr==',')
          array = strtoul(endPtr+1,&endPtr,0);
      }
      printf("Acquire %d records of %d sums at %d rate into array %d\n",
             nacq,naccum,rate,array);
      break;
    case 'D':
      begin = strtoull(optarg,&endPtr,0);
      end   = strtoull(endPtr+1,&endPtr,0);
      break;
    case 'F':
      fetch = (1ULL<<strtoul(optarg,NULL,0));
      break;
    case 'G':
      lTPG = true;
      break;
    case 'N':
      lNoFetch = true;
      break;
    default:
      show_usage(argv[0]);
      exit(1);
    }
  }


  //  try {
  {
    Bsa::AmcCarrier hw(ip,lTPG);

    unsigned NArrays = hw.nArrays();

    if (lInit) {
      hw.initialize();
      for(unsigned i=0; i<ndiag; i++)
        hw.initRaw(i, (1ULL<<24), true);
    }

    if (lDiag) {
      if (hw.doneRaw()&(1<<ndiag)) {
        Bsa::RingState state = hw.ring(ndiag);

        uint8_t* p = hw.getBuffer(state.begAddr,state.endAddr);

        FILE* f = 0;
        if (filename) {
          char buff[256];
          sprintf(buff,"%s_%llx_%llx",filename,state.begAddr,state.endAddr);
          f = fopen(buff,"w");
          if (!f) {
            perror("Opening output file");
          }
          else {
            fwrite(p, 1, state.endAddr-state.begAddr, f);
            fclose(f);
          }
        }

        delete p;
      }
      hw.rearm(ndiag);
    }

    if (nacq) {
      MyCallback cb(hw, array);
      BeamSelect beam;
      RateSelect rsel;
      if (rate>=0) rsel = RateSelect((RateSelect::FixedRate)rate);
      else         rsel = RateSelect((RateSelect::ACRate)(-rate),0x1);

      printf("RateSelect [%08x]\n",unsigned(rsel));

      hw.start(array,beam,rsel,nacq,naccum,0);
      hw.poll(cb);
    }

    hw.dump();

    if (lNoFetch) return 0;

    //
    //  Fetch all buffers which are done with acquisition
    //
    fetch |= hw.done();

    for(unsigned fetchArray=0; fetchArray<64; fetchArray++) {
      if (fetch&(1ULL<<fetchArray)) {
        Bsa::Record* record = hw.getRecord(fetchArray);

        FILE* f = 0;
        if (filename) {
          char buff[256];
          sprintf(buff,"%s_%u",filename,fetchArray);
          f = fopen(buff,"w");
          if (!f) {
            perror("Opening output file");
            continue;
          }
        }

        if (f) {
          fwrite(record->entries.data(), sizeof(Bsa::Entry), record->entries.size(), f);
          fclose(f);
        }
        else {
          dump_bsa(*record,channelMask);
        }
        delete record;

        hw.clear(fetchArray);
      }
    }

    if (begin!=end) {
      timespec begin_time, end_time;
      clock_gettime(CLOCK_REALTIME,&begin_time);
      uint8_t* p = hw.getBuffer(begin,end);
      clock_gettime(CLOCK_REALTIME,&end_time);
      double dt = double(end_time.tv_sec-begin_time.tv_sec) + 
        1.e-9*(double(end_time.tv_nsec)-double(begin_time.tv_nsec));

      printf("Fetch %lx bytes in %f secs [%f MB/s]\n",
             end-begin, dt, 1.e-6*double(end-begin)/dt);

      FILE* f = 0;
      if (filename) {
        char buff[256];
        sprintf(buff,"%s_%llx_%llx",filename,begin,end);
        f = fopen(buff,"w");
        if (!f) {
          perror("Opening output file");
        }
        else {
          fwrite(p, 1, end-begin, f);
          fclose(f);
        }
      }

      delete p;
    }
  // } catch (CPSWError &e) {
  //   printf("CPSW Error: %s\n", e.getInfo().c_str());
  //   throw;
  }
                 
  return 1;
}
