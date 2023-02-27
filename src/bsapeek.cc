//
//  Utility to inspect BSA
//
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

#include <AmcCarrierBase.hh>
#include <RamControl.hh>
#include <cpsw_yaml_keydefs.h>
#include <cpsw_yaml.h>

using namespace Bsa;

static uint64_t GET_U1(Path pre, unsigned nelms)
{
  uint64_t r=0;
  ScalVal_RO s = IScalVal_RO::create( pre );
  for(unsigned i=0; i<nelms; i++) {
    IndexRange rng(i);
    unsigned v;
    s->getVal(&v,1,&rng);
    if (v)
      r |= 1ULL<<i;
  }
  return r;
}

static uint64_t GET_U1(ScalVal_RO s, unsigned nelms)
{
  uint64_t r=0;
  for(unsigned i=0; i<nelms; i++) {
    IndexRange rng(i);
    unsigned v;
    s->getVal(&v,1,&rng);
    if (v)
      r |= 1ULL<<i;
  }
  return r;
}

static Path _build(const char* ip)
{
  //
  //  Build
  //
  NetIODev  root = INetIODev::create("fpga", ip);
  {  //  Register access
#if 0
    INetIODev::PortBuilder bldr = INetIODev::createPortBuilder();
    bldr->setSRPVersion              ( INetIODev::SRP_UDP_V2 );
#else
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V2 );
#endif
    bldr->setUdpPort                 (                  8192 );
    bldr->useRssi                    (                 false );

    MMIODev   mmio = IMMIODev::create ("mmio", (1ULL<<32));
    {
      //  Build RamControl
      RamControl control = IRamControl::create("control");
      mmio->addAtAddress( control, 0x09000000);
    }
    {      
      //  Build RamControl
      RamControl control = IRamControl::create("waveform0",true);
      mmio->addAtAddress( control, 0x09010000);
    }
    {      
      //  Build RamControl
      RamControl control = IRamControl::create("waveform1",true);
      mmio->addAtAddress( control, 0x09020000);
    }
    root->addAtAddress( mmio, bldr);
  }
  return IPath::create( root );
}

namespace Bsa {
  class AmcCarrierPeek : public AmcCarrierBase {
  public:
    AmcCarrierPeek(const char* ip) 
    {
      _path      = _build(ip);
      // _sCmpl     = IScalVal   ::create( _path->findByName("mmio/tpg/BsaComplete") );
      // _sStat     = IScalVal_RO::create( _path->findByName("mmio/tpg/BsaStat") );
      _sEmpt     = IScalVal_RO::create( _path->findByName("mmio/control/Empty") );
      _sDone     = IScalVal_RO::create( _path->findByName("mmio/control/Done") );
      _sFull     = IScalVal_RO::create( _path->findByName("mmio/control/Full") );
      _sEnabled  = IScalVal   ::create( _path->findByName("mmio/control/Enabled") );
      _sMode     = IScalVal   ::create( _path->findByName("mmio/control/Mode") );
      _sInit     = IScalVal   ::create( _path->findByName("mmio/control/Init") );
      _sError    = IScalVal_RO::create( _path->findByName("mmio/control/Error") );
      _sStatus   = IScalVal_RO::create( _path->findByName("mmio/control/Status") );
      _tstamp    = IScalVal_RO::create( _path->findByName("mmio/control/TimeStamp") );
      _startAddr = IScalVal   ::create( _path->findByName("mmio/control/StartAddr") );
      _endAddr   = IScalVal   ::create( _path->findByName("mmio/control/EndAddr") );
      _wrAddr    = IScalVal_RO::create( _path->findByName("mmio/control/WrAddr") );
      _trAddr    = IScalVal_RO::create( _path->findByName("mmio/control/TriggerAddr") );
      // _dram      = IScalVal_RO::create( _path->findByName("strm/dram") );
      // _memEnd    = 0;
    }
    RingState ring  (unsigned array) const { return RingState(); }
    void     dump      () const
    {
      //  unsigned NArrays = nArrays();
      unsigned NArrays = 64;
      //
      //  Print BSA buffer status summary
      //
      uint64_t done, full, empty, error;
      done  = GET_U1(_sDone,NArrays);
      full  = GET_U1(_sFull,NArrays);
      empty = GET_U1(_sEmpt,NArrays);
      error = GET_U1(_sError,NArrays);

      printf("BufferDone [%016llx]\t  Full[%016llx]\t  Empty[%016llx]\n",
             (unsigned long long)done,(unsigned long long)full,(unsigned long long)empty);
      printf("%4.4s ","Buff");
      printf("%9.9s ","Start");
      printf("%9.9s ","End");
      printf("%9.9s ","Write");
      printf("%9.9s ","Trigger");
      printf("%10.10s ","sec");
      printf("%9.9s ","nsec");
      printf("%4.4s ","Done");
      printf("%4.4s ","Full");
      printf("%4.4s ","Empt");
      printf("%4.4s ","Erro");
      printf("\n");

      for(unsigned i=0; i<NArrays; i++) {
        _printBuffer(_path->findByName("mmio/control"),
                     _tstamp, i, done, full, empty, error);
      }

      done  = GET_U1(_path->findByName("mmio/waveform0/Done"),4);
      full  = GET_U1(_path->findByName("mmio/waveform0/Full"),4);
      empty = GET_U1(_path->findByName("mmio/waveform0/Empty"),4);

      for(unsigned i=0; i<4; i++) {
        _printBuffer(_path->findByName("mmio/waveform0"),i, done, full, empty, error);
      }

      done  = GET_U1(_path->findByName("mmio/waveform1/Done"),4);
      full  = GET_U1(_path->findByName("mmio/waveform1/Full"),4);
      empty = GET_U1(_path->findByName("mmio/waveform1/Empty"),4);

      for(unsigned i=0; i<4; i++) {
        _printBuffer(_path->findByName("mmio/waveform1"),i, done, full, empty, error);
      }

      _path->dump(stderr);
    }
  };
};

static void show_usage(const char* p)
{
  printf("** Collect BSA data **\n");
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <IP address dotted notation> : set carrier IP\n");

  time_t t = time(NULL);
  struct tm* ptm = localtime(&t);
  printf("asctime %s\n",asctime(ptm));
}


int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  unsigned array=0;
  uint64_t fetch=0;

  char* endPtr;
  int c;
  while( (c=getopt(argc,argv,"a:h"))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg; break;
    default:
      show_usage(argv[0]);
      exit(1);
    }
  }

  Bsa::AmcCarrierPeek hw(ip);
  hw.dump();
                 
  return 1;
}
