
#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_proto_mod_depack.h>

#include <AmcCarrier.hh>
#include <RamControl.hh>
#include <TPG.hh>
#include <TPGMini.hh>

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

#define SET_REG(name,val) {                                             \
    unsigned v(val);                                                    \
    ScalVal s = IScalVal::create( _path->findByName(name) );     \
    s->setVal(&v,1,&rng);                                               \
  }

#define GET_REG(name,val) {                                             \
    ScalVal s = IScalVal::create( _path->findByName(name) );     \
    s->getVal(&val,1,&rng);                                             \
  }

#define SET_REGL(name,val) {                                            \
    uint64_t v(val);                                                    \
    ScalVal s = IScalVal::create( _path->findByName(name) );     \
    s->setVal(&v,1,&rng);                                               \
  }

#define GET_REGL(name,val) {                                            \
    ScalVal_RO s = IScalVal_RO::create( _path->findByName(name) ); \
    s->getVal(&val,1,&rng);                                               \
  }


using namespace Bsa;

void AmcCarrier::build(MMIODev mmio)
{
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
}

static AmcCarrier* _instance = 0;

AmcCarrier* AmcCarrier::instance()
{
  return _instance;
}

AmcCarrier* AmcCarrier::instance(Path path)
{
  if (!_instance)
    _instance = new AmcCarrier(path);
  return _instance;
}

static Path _build(const char* ip,
                   bool        lTPG)
{
  //
  //  Build
  //
  NetIODev  root = INetIODev::create("fpga", ip);
  {  //  Register access
#ifndef FRAMEWORK_R3_4
    INetIODev::PortBuilder bldr = INetIODev::createPortBuilder();
    bldr->setSRPVersion              ( INetIODev::SRP_UDP_V3 );
#else
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V3 );
#endif
    bldr->setUdpPort                 (                  8193 );
    bldr->setSRPTimeoutUS            (                 90000 );
    bldr->setSRPRetryCount           (                    10 );
    bldr->setSRPMuxVirtualChannel    (                     0 );
    bldr->useDepack                  (                  true );
    bldr->useRssi                    (                  true );
    bldr->useTDestMux                (                     1 );
    bldr->setTDestMuxTDEST           (                     0 );
    bldr->setUdpNumRxThreads         (                     1 );
    bldr->setUdpOutQueueDepth        (                    40 );

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
    if (lTPG) {
      //  Build TPG
      TPG tpg = ITPG::create("tpg");
      mmio->addAtAddress( tpg, 0x80000000);
    }
    else {
      //  Build TPGMini
      TPGMini    tpg = ITPGMini::create("tpg");
      mmio->addAtAddress( tpg, 0x08030000);
    }
    root->addAtAddress( mmio, bldr);
  }

  {  // Streaming access
#ifndef FRAMEWORK_R3_4
    INetIODev::PortBuilder bldr = INetIODev::createPortBuilder();
    bldr->setSRPVersion              ( INetIODev::SRP_UDP_V3 );
#else
    ProtoStackBuilder bldr = IProtoStackBuilder::create();
    bldr->setSRPVersion              ( IProtoStackBuilder::SRP_UDP_V3 );
#endif
    bldr->setUdpPort                 (                  8194 );
    bldr->setSRPTimeoutUS            (                 90000 );
    bldr->setSRPRetryCount           (                     5 );
    bldr->setSRPMuxVirtualChannel    (                     0 );
    bldr->useDepack                  (                  true );
    bldr->useRssi                    (                  true );
    bldr->setTDestMuxTDEST           (                     4 );

    MMIODev   mmio = IMMIODev::create ("strm", (1ULL<<33));
    {
      //  Build DRAM
      Field      f = IIntField::create("dram", 64, false, 0);
      mmio->addAtAddress( f, 0, (1<<30));
    }
    root->addAtAddress( mmio, bldr);
  }

  return IPath::create( root );
}

static void* poll_irq(void* arg)
{
  AmcCarrier* p = reinterpret_cast<AmcCarrier*>(arg);
  p->handleIrq();
  return 0;
}

//
//  Build from just an IP address
//    Assumes we know the register addresses
//
AmcCarrier::AmcCarrier(const char* ip, bool lTPG)
{
  _instance = this;
  _path      = _build(ip,lTPG);
  _sCmpl     = IScalVal   ::create( _path->findByName("mmio/tpg/BsaComplete") );
  _sStat     = IScalVal_RO::create( _path->findByName("mmio/tpg/BsaStat") );
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
  _dram      = IScalVal_RO::create( _path->findByName("strm/dram") );
  _memEnd    = 0;
  //  pthread_t      thread_id;
  //  pthread_create(&thread_id, 0, poll_irq, (void*)this);
}

//
//  Build from one Path
//    Assumes we know the paths below
//
AmcCarrier::AmcCarrier(Path path)
{
  _path      = path;
  _sCmpl     = IScalVal   ::create( _path->findByName("mmio/tpg/BsaComplete") );
  _sStat     = IScalVal_RO::create( _path->findByName("mmio/tpg/BsaStat") );
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
  _dram      = IScalVal_RO::create( _path->findByName("strm/dram") );
  _memEnd    = 0;
}

AmcCarrier::~AmcCarrier()
{
}

unsigned AmcCarrier::nArrays   () const
{
  unsigned v;
  IScalVal_RO::create(_path->findByName("mmio/tpg/NArraysBsa"))->getVal(&v,1);
  return v;
}

uint32_t AmcCarrier::doneRaw   () const
{
  uint32_t done;
  done  = (GET_U1(_path->findByName("mmio/waveform0/Done"),4)<<0)
        | (GET_U1(_path->findByName("mmio/waveform1/Done"),4)<<4);
  return done;
}

RingState AmcCarrier::ring     (unsigned array) const
{
  RingState s;
  IndexRange rng(array);
  Path path = _path->findByName(array<4 ? "mmio/waveform0" : "mmio/waveform1");
  IScalVal_RO::create( path->findByName("StartAddr") )->getVal(&s.begAddr,1,&rng);
  IScalVal_RO::create( path->findByName("EndAddr"  ) )->getVal(&s.endAddr,1,&rng);
  IScalVal_RO::create( path->findByName("WrAddr"   ) )->getVal(&s.nxtAddr,1,&rng);
  return s;
}

void     AmcCarrier::initRaw   (unsigned index,
                                uint64_t bufferSize,
                                bool     doneWhenFull)
{
  const uint64_t BlockMask = (0x1ULL<<12)-1;  // Buffers must be in blocks of 4kB

  Path path = _path->findByName(index<4 ? "mmio/waveform0" : "mmio/waveform1");
    
  //  Setup the waveform memory
  uint64_t p = _memEnd;
  uint64_t pn = p+((bufferSize+BlockMask)&~BlockMask);
  uint32_t one(1), zero(0), mode(doneWhenFull ? 1:0);
  IndexRange rng(index%4);
  printf("Setup waveform memory %i %llx:%llx\n", index, p,pn);
  IScalVal::create( path->findByName("StartAddr"))->setVal(&p   ,1,&rng);
  IScalVal::create( path->findByName("EndAddr"  ))->setVal(&pn  ,1,&rng);
  IScalVal::create( path->findByName("Enabled"  ))->setVal(&one ,1,&rng);
  IScalVal::create( path->findByName("Mode"     ))->setVal(&mode,1,&rng);
  IScalVal::create( path->findByName("Init"     ))->setVal(&one ,1,&rng);
  IScalVal::create( path->findByName("Init"     ))->setVal(&zero,1,&rng);
  _memEnd = pn;
}

void     AmcCarrier::rearm(unsigned index)
{
  uint32_t one(1), zero(0), mode(1);
  Path path = _path->findByName(index<4 ? "mmio/waveform0" : "mmio/waveform1");
  IndexRange rng(index%4);
  IScalVal::create( path->findByName("Mode"     ))->setVal(&mode,1,&rng);
  IScalVal::create( path->findByName("Init"     ))->setVal(&one ,1,&rng);
  IScalVal::create( path->findByName("Init"     ))->setVal(&zero,1,&rng);
}

void     AmcCarrier::start     (unsigned array,
                                unsigned rate,
                                unsigned nacq,
                                unsigned naccum,
                                unsigned sevr)
{
  //  Clear completion bit
  uint64_t cmpl=(1ULL<<array);
  _sCmpl->setVal(&cmpl,1);
  
  IndexRange rng(array);
  SET_REG("mmio/tpg/BsaEventSel",(2<<29)|(rate&0xf));
  SET_REG("mmio/tpg/BsaStatSel" ,(nacq<<16)|(naccum&0x1fff)|((sevr&0x3)<<14));
}

void     AmcCarrier::start     (unsigned   array,
                                BeamSelect beam, 
                                RateSelect rate,
                                unsigned   nacq,
                                unsigned   naccum,
                                unsigned   sevr)
{
  //  Clear completion bit
  uint64_t cmpl=(1ULL<<array);
  _sCmpl->setVal(&cmpl,1);
  
  IndexRange rng(array);
  SET_REG("mmio/tpg/BsaEventSel",(unsigned(beam)<<13) | (unsigned(rate)<<0));
  SET_REG("mmio/tpg/BsaStatSel" ,(nacq<<16)|(naccum&0x1fff)|((sevr&0x3)<<14));
}

void     AmcCarrier::poll      (AmcCarrierCallback& cb)
{
  uint64_t cmpl;
  while(1) {
    //  Poll for completion
    _sCmpl->getVal(&cmpl,1);
    int next=0;
    if (cmpl) 
      next=cb.process(cmpl);
    else
      next=cb.processTmo();
    _sCmpl->setVal(&cmpl,1);
    if (!next)
      break;
    usleep(10000);
  }
}

void     AmcCarrier::clear     (unsigned array)
{
  IndexRange rng(array);
  SET_REG ("mmio/control/Init"     , 1);
  SET_REG ("mmio/control/Init"     , 0);
}

void     AmcCarrier::dump      () const
{
  //  unsigned NArrays = nArrays();
  unsigned NArrays = 64;
  //
  //  Print BSA buffer status summary
  //
  uint64_t done, full, empty, error;
  done  = GET_U1(_sDone,64);
  full  = GET_U1(_sFull,64);
  empty = GET_U1(_sEmpt,64);
  error = GET_U1(_sError,64);

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


void AmcCarrier::handleIrq()
{
    printf("handleIrq: entered\n");

    timespec tvo; clock_gettime(CLOCK_REALTIME,&tvo);

    //  Async messages interfere with SRP!
    Stream           strm = IStream::create( _path->findByName("irq") );
    CTimeout         tmo(1000000);
    CAxisFrameHeader hdr;

    uint8_t buf[64];
    int v;
    while ((v=strm->read( buf, sizeof(buf), tmo, 0))>=0) {
      
      if (v>0) {
        printf("handleIrq: Received %d bytes\n",v);

        if(!hdr.parse(buf, sizeof(buf)))
          continue;

        unsigned irqStatus = *reinterpret_cast<uint32_t*>(&buf[hdr.getSize()]);

        abort();

        if (1) {
          timespec tv; clock_gettime(CLOCK_REALTIME,&tv);
          double dt = double(tv.tv_sec-tvo.tv_sec)+1.e-9*(double(tv.tv_nsec)-double(tvo.tv_nsec));
          tvo = tv; 
          printf("received irqStatus %x  %f\n",irqStatus,dt); 
        }

        const uint32_t* ubuf = reinterpret_cast<const uint32_t*>(buf);
        for(unsigned i=0; i<(v>>2); i++)
          printf(" %08x", ubuf[i]);
        printf("\n");
      }
      else
        printf("handleIrq tmo\n");
    }
}
