
#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>

#include <AmcCarrierYaml.hh>
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

#define SET_REG(name,val) {                                             \
    unsigned v(val);                                                    \
    ScalVal s = IScalVal::create( _bpath->findByName(name) );     \
    s->setVal(&v,1,&rng);                                               \
  }

#define SET_REGL(name,val) {                                            \
    uint64_t v(val);                                                    \
    ScalVal s = IScalVal::create( _bpath->findByName(name) );     \
    s->setVal(&v,1,&rng);                                               \
  }


using namespace Bsa;

AmcCarrierYaml::AmcCarrierYaml(Path mmio,
                               Path dram) 
{
  _path   = mmio;
  _dram   = IScalVal_RO::create( dram );
  _bpath  = mmio->findByName("BsaBufferControl/BsaBuffers");
  _wpath0 = mmio->findByName("BsaWaveformEngine[0]/WaveformEngineBuffers");
  _wpath1 = mmio->findByName("BsaWaveformEngine[1]/WaveformEngineBuffers");

  _sEmpt     = IScalVal_RO::create( _bpath->findByName("Empty") );
  _sDone     = IScalVal_RO::create( _bpath->findByName("Done") );
  _sFull     = IScalVal_RO::create( _bpath->findByName("Full") );
  _sEnabled  = IScalVal   ::create( _bpath->findByName("Enabled") );
  _sMode     = IScalVal   ::create( _bpath->findByName("Mode") );
  _sInit     = IScalVal   ::create( _bpath->findByName("Init") );
  _sError    = IScalVal_RO::create( _bpath->findByName("Error") );
  _sStatus   = IScalVal_RO::create( _bpath->findByName("Status") );
  _tstamp    = IScalVal_RO::create( _path ->findByName("BsaBufferControl/Timestamps/MemoryArray") );
  _sClear    = IScalVal   ::create( _path ->findByName("BsaBufferControl/BufferInit/MemoryArray") );
  _startAddr = IScalVal   ::create( _bpath->findByName("StartAddr") );
  _endAddr   = IScalVal   ::create( _bpath->findByName("EndAddr") );
  _wrAddr    = IScalVal_RO::create( _bpath->findByName("WrAddr") );
  _trAddr    = IScalVal_RO::create( _bpath->findByName("TriggerAddr") );
  _memEnd    = 0;

  printf("dram array is (%u,%llu)\n", _dram->getNelms(), _dram->getSizeBits());
}

AmcCarrierYaml::~AmcCarrierYaml()
{
}

unsigned AmcCarrierYaml::nArrays   () const
{ return 64; }

uint32_t AmcCarrierYaml::doneRaw   () const
{
  uint32_t done;
  done  = (GET_U1(_wpath0->findByName("Done"),4)<<0)
        | (GET_U1(_wpath1->findByName("Done"),4)<<4);
  return done;
}

RingState AmcCarrierYaml::ring     (unsigned array) const
{
  RingState s;
  IndexRange rng(array%4);
  Path bpath = array < 4 ? _wpath0 : _wpath1;
  IScalVal_RO::create( bpath->findByName("StartAddr") )->getVal(&s.begAddr,1,&rng);
  IScalVal_RO::create( bpath->findByName("EndAddr"  ) )->getVal(&s.endAddr,1,&rng);
  IScalVal_RO::create( bpath->findByName("WrAddr"   ) )->getVal(&s.nxtAddr,1,&rng);
  return s;
}

void     AmcCarrierYaml::initializ_(unsigned index,
                                    uint64_t bufferSize,
                                    bool     doneWhenFull)
{
  const uint64_t BlockMask = (0x1ULL<<12)-1;  // Buffers must be in blocks of 4kB

  Path path = index < 4 ? _wpath0 : _wpath1;
    
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

void     AmcCarrierYaml::rearm(unsigned index)
{
  uint32_t one(1), zero(0), mode(1);
  Path path = index<4 ? _wpath0 : _wpath1;
  IndexRange rng(index%4);
  IScalVal::create( path->findByName("Mode"     ))->setVal(&mode,1,&rng);
  IScalVal::create( path->findByName("Init"     ))->setVal(&one ,1,&rng);
  IScalVal::create( path->findByName("Init"     ))->setVal(&zero,1,&rng);
}

void     AmcCarrierYaml::clear     (unsigned array)
{
  IndexRange rng(array);
  SET_REG ("Init"     , 1);
  SET_REG ("Init"     , 0);
}

void     AmcCarrierYaml::dump      () const
{
  //  unsigned NArrays = nArrays();
  unsigned NArrays = 64;
  //
  //  Print BSA buffer status summary
  //
  uint64_t done, full, empty, error;
  done  = GET_U1(_bpath->findByName("Done"),64);
  full  = GET_U1(_bpath->findByName("Full"),64);
  empty = GET_U1(_bpath->findByName("Empty"),64);
  error = GET_U1(_bpath->findByName("Error"),64);

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
    _printBuffer(_bpath, _tstamp, i, done, full, empty, error);
  }

  done  = GET_U1(_wpath0->findByName("Done"),4);
  full  = GET_U1(_wpath0->findByName("Full"),4);
  empty = GET_U1(_wpath0->findByName("Empty"),4);
  error = GET_U1(_wpath0->findByName("Error"),4);

  for(unsigned i=0; i<4; i++) {
    _printBuffer(_wpath0,i, done, full, empty, error);
  }

  done  = GET_U1(_wpath1->findByName("Done"),4);
  full  = GET_U1(_wpath1->findByName("Full"),4);
  empty = GET_U1(_wpath1->findByName("Empty"),4);
  error = GET_U1(_wpath1->findByName("Error"),4);

  for(unsigned i=0; i<4; i++) {
    _printBuffer(_wpath1,i, done, full, empty, error);
  }
}

