//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
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
#include <libgen.h>

#include <AmcCarrierYaml.hh>
#include <cpsw_api_user.h>

static void dump_bsa(const Bsa::Record& record)
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
  printf("         -f <filename>                   : write BSA to file (otherwise screen)\n");
  printf("         -F <array>                      : force fetch of BSA array\n");
  printf("         -D <begin,end>                  : fetch DRAM\n");
  printf("         -d <buffer>                     : write diagnostics to file and re-arm\n");
  printf("         -t <yamlFile>,<mmioRoot>,<dramRoot>\n");
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  unsigned array=0;
  uint64_t fetch=0;
  const char* filename=0;
  unsigned rate=0;
  uint64_t begin=0,end=0;
  unsigned ndiag=0;
  char* yamlArgs;
  const char* yamlFile = 0;
  const char* mmioPath = 0;
  const char* dramPath = 0;

  bool lInit=false;
  bool lTPG =false;
  bool lDiag=false;

  char* endPtr;
  int c;
  while( (c=getopt(argc,argv,"a:d:i:g:f:t:D:F:G"))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg; break;
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
    case 't':
      yamlArgs = strdup(optarg);
      yamlFile = strtok(yamlArgs,",");
      mmioPath = strtok(NULL,",");
      dramPath = strtok(NULL,",");
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
    default:
      show_usage(argv[0]);
      exit(1);
    }
  }

  if (yamlFile==0 || mmioPath==0 || dramPath==0) {
    show_usage(argv[0]);
    exit(1);
  }

  //  try {
  {
    printf("Loading Yamls [%s] %s %s\n",yamlFile,mmioPath,dramPath);
    Path root = IPath::loadYamlFile(yamlFile,"NetIODev");
    printf("Loaded root\n");
    Path mmio = root->findByName(mmioPath);
    printf("Loaded mmio\n");
    Path dram = root->findByName(dramPath);
    printf("Loaded dram\n");

    Bsa::AmcCarrierYaml hw(mmio,dram);

    unsigned NArrays = hw.nArrays();

    if (lInit) {
      hw.initialize();
      for(unsigned i=0; i<ndiag; i++)
        hw.initialize(i, (1ULL<<24), true);
    }

    if (lDiag) {
      if (hw.doneRaw()&(1<<ndiag)) {
        Bsa::RingState state = hw.ring(ndiag);

        uint8_t* p = hw.get(state.begAddr,state.endAddr);

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

    hw.dump();

    //
    //  Fetch all buffers which are done with acquisition
    //
    fetch |= hw.done();

    for(unsigned fetchArray=0; fetchArray<64; fetchArray++) {
      if (fetch&(1ULL<<fetchArray)) {
        Bsa::Record* record = hw.get(fetchArray);

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
          dump_bsa(*record);
        }
        delete record;

        hw.clear(fetchArray);
      }
    }

    if (begin!=end) {
      timespec begin_time, end_time;
      clock_gettime(CLOCK_REALTIME,&begin_time);
      uint8_t* p = hw.get(begin,end);
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
