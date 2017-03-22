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

#include <RamControl.hh>

#include <cpsw_api_builder.h>
#include <cpsw_api_user.h>

using std::string;

class ISy56040;
typedef shared_ptr<ISy56040> Sy56040;
                                                             
class CSy56040Impl;
typedef shared_ptr<CSy56040Impl> Sy56040Impl;

class ISy56040 : public virtual IMMIODev {
public:
  static Sy56040 create(const char*);
};

class CSy56040Impl : public CMMIODevImpl, public virtual ISy56040 {
public:
  CSy56040Impl(Key &k, const char *name);
};

Sy56040 ISy56040::create(const char* name)
{
  Sy56040Impl v = CShObj::create<Sy56040Impl>(name);
  Field f;
  f = IIntField::create("Out", 32, false, 0);
  v->CMMIODevImpl::addAtAddress(f, 0, 4, 4);
  return v;
}

CSy56040Impl::CSy56040Impl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x2000, LE)
{
}

static bool bldr_init=false;
#ifndef FRAMEWORK_R3_4
static INetIODev::PortBuilder bldr;
#else
static ProtoStackBuilder bldr;
#endif

static Path build_bsa(const char* ip)
{
  //
  //  Build
  //
  NetIODev root = INetIODev::create("fpga", ip);

  {  //  Register access
    if (!bldr_init) {
      bldr_init=true;
#ifndef FRAMEWORK_R3_4
      bldr = INetIODev::createPortBuilder();
      bldr->setSRPVersion              ( INetIODev::SRP_UDP_V3 );
#else
      bldr = IProtoStackBuilder::create();
      bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V3 );
#endif
      bldr->setUdpPort                 (                  8193 );
      bldr->setSRPTimeoutUS            (                 90000 );
      bldr->setSRPRetryCount           (                     5 );
      bldr->setSRPMuxVirtualChannel    (                     0 );
      bldr->useDepack                  (                  true );
      bldr->useRssi                    (                  true );
      bldr->setTDestMuxTDEST           (                     0 );
    }

    MMIODev   mmio = IMMIODev::create ("mmio", (1ULL<<32));
    {      
      //  Build RamControl
      Bsa::RamControl control = Bsa::IRamControl::create("control");
      mmio->addAtAddress( control, 0x09000000);
    }
    root->addAtAddress( mmio, bldr);
  }
  return IPath::create( root );
}

static Path build_tpg(const char* ip)
{
  NetIODev root = INetIODev::create("fpga", ip);

  {  //  Register access
    if (!bldr_init) {
      bldr_init=true;
#ifndef FRAMEWORK_R3_4
      bldr = INetIODev::createPortBuilder();
      bldr->setSRPVersion              ( INetIODev::SRP_UDP_V3 );
#else
      bldr = IProtoStackBuilder::create();
      bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V3 );
#endif
      bldr->setUdpPort                 (                  8193 );
      bldr->setSRPTimeoutUS            (                 90000 );
      bldr->setSRPRetryCount           (                     5 );
      bldr->setSRPMuxVirtualChannel    (                     0 );
      bldr->useDepack                  (                  true );
      bldr->useRssi                    (                  true );
      bldr->setTDestMuxTDEST           (                     0 );
    }

    MMIODev   mmio = IMMIODev::create ("mmio", (1ULL<<32));
    {
      //  Timing Crossbar
      Sy56040 xbar = ISy56040::create("xbar");
      mmio->addAtAddress( xbar, 0x03000000);
    }

    root->addAtAddress( mmio, bldr);
  }
  return IPath::create( root );
}

void show_usage(const char* p)
{
  printf("Usage: %s -f <yaml file> -r <reg name> [-a <ip addr>]\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip=0;

  char* endPtr;
  int c;
  while( (c=getopt(argc,argv,"a:"))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg; break;
    default:
      show_usage(argv[0]);
      exit(1);
    }
  }

  uint32_t val=0xffffffff;
  IndexRange rng(0);

#if 1
  Path bsa = build_bsa(ip);
  { ScalVal_RO s = IScalVal_RO::create( bsa->findByName("mmio/control/StartAddr") );
    s->getVal(&val,1,&rng); }
  printf("StartAddr[0] %x\n",val);
  //#else
  Path tpg = build_tpg(ip);
  { ScalVal_RO s = IScalVal_RO::create( tpg->findByName("mmio/xbar/Out") );
    s->getVal(&val,1,&rng); }
  printf("Out[0] %x\n",val);
#endif  

  return 0;
}
