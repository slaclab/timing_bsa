#include "AmcCarrierBase.hh"
#include "BsaDefs.hh"

#include <stdio.h>

using namespace Bsa;

//#define BURSTSIZE 0x800
//#define DBUG

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

AmcCarrierBase::AmcCarrierBase() : _state(HSTARRAYN)
{
}

void     AmcCarrierBase::initialize()
{
  uint64_t p=0,pn=0,pe=0;
  uint32_t uzro=0,uone=1;

  //  Setup the standard BSA arrays (32k entries)
  //  Must be a multiple of bsaSize and burstSize(2kB)
  const uint64_t bsaSize = 3ULL<<7;  // one entry
  for(unsigned i=0; i<NBSAARRAYS; i++) {
#ifdef DBUG
    pn = p+(1ULL<<8)*bsaSize;
#else
    pn = p+(1ULL<<15)*bsaSize;
#endif
    //    pe = pn - BURSTSIZE;
    pe = pn;
    IndexRange rng(i);
    _startAddr->setVal(&p   ,1,&rng);
    _endAddr  ->setVal(&pe  ,1,&rng);
    _sEnabled ->setVal(&uone,1,&rng);
    _sMode    ->setVal(&uzro,1,&rng);
    _sInit    ->setVal(&uone,1,&rng);
    _sInit    ->setVal(&uzro,1,&rng);
#ifdef DBUG
    printf("DBUG:  array %u  startAddr 0x%016llx  endAddr 0x%016llx\n",
           i, p, pe);
#endif
    p = pn;
  }
  //  Setup the fault arrays to be LARGER (1M entries)
  for(unsigned i=HSTARRAY0; i<HSTARRAYN; i++) {
    pn = p+(1ULL<<20)*bsaSize;
    //    pe = pn - BURSTSIZE;
    pe = pn;
    IndexRange rng(i);
    _startAddr->setVal(&p   ,1,&rng);
    _endAddr  ->setVal(&pe  ,1,&rng);
    _sEnabled ->setVal(&uone,1,&rng);
    _sMode    ->setVal(&uzro,1,&rng);
    _sInit    ->setVal(&uone,1,&rng);
    _sInit    ->setVal(&uzro,1,&rng);
#ifdef DBUG
    printf("DBUG:  array %u  startAddr 0x%016llx  endAddr 0x%016llx\n",
           i, p, pe);
#endif
    p = pn;
  }

  _memEnd = p;
}

void     AmcCarrierBase::reset     (unsigned array)
{
  IndexRange rng(array);
  uint32_t uzro=0,uone=1;
  _sInit    ->setVal(&uone,1,&rng);
  _sInit    ->setVal(&uzro,1,&rng);
}

void     AmcCarrierBase::ackClear  (unsigned array)
{
  uint32_t   uzro=0;
  IndexRange rng(array);
  _sClear->setVal(&uzro,1,&rng);
}

uint64_t AmcCarrierBase::inprogress() const
{
  uint64_t r=0;
  unsigned v[64];
  _sEmpt->getVal(v,HSTARRAYN);
  for(unsigned i=0; i<HSTARRAYN; i++) {
    if (!v[i])
      r |= 1ULL<<i;
  }
  return r;
}

uint64_t AmcCarrierBase::done      () const
{
  uint64_t r=0;
  uint32_t v[HSTARRAYN];
  memset(v,0,HSTARRAYN*sizeof(uint32_t));
  _sStatus->getVal(v,HSTARRAYN);
  for(unsigned i=0; i<HSTARRAYN; i++) {
    if (v[i]&4)
      r |= 1ULL<<i;
  }
  return r;
}

bool AmcCarrierBase::done      (unsigned array) const
{
  IndexRange rng(array);
  unsigned v;
  _sDone->getVal(&v,1,&rng);
  return v;
}

uint32_t AmcCarrierBase::status    (unsigned array) const
{
  uint32_t v;
  IndexRange rng(array);
  _sDone->getVal(&v,1,&rng);
  return v;
}

ArrayState AmcCarrierBase::state   (unsigned array) const
{
  ArrayState s;
  IndexRange rng(array);
  _tstamp->getVal(&s.timestamp,1,&rng);
  _wrAddr->getVal(&s.wrAddr   ,1,&rng);
  _sClear->getVal(&s.clear    ,1,&rng);
  _sFull ->getVal(&s.wrap     ,1,&rng);
  return s;
}

const std::vector<ArrayState>& AmcCarrierBase::state()
{
  uint64_t tstamp[HSTARRAYN];
  uint64_t wrAddr[HSTARRAYN];
  unsigned clear [HSTARRAYN];
  _tstamp->getVal(tstamp,HSTARRAYN);
  _sClear->getVal(clear ,HSTARRAYN);
  _wrAddr->getVal(wrAddr,HSTARRAYN);
  for(unsigned i=0; i<HSTARRAYN; i++) {
    _state[i].timestamp = tstamp[i];
    _state[i].clear     = clear [i];
    _state[i].wrAddr    = wrAddr[i];
  }

#if 0
  for(unsigned i=0; i<HSTARRAYN; i++)
    printf("state [%u] %llx %llx\n",
           i, tstamp[i], wrAddr[i]);
#endif

  return _state;
}

Record*  AmcCarrierBase::getRecord (unsigned  array) const
{  
  uint64_t next;
  return get(array,0,&next);
}

Record*  AmcCarrierBase::getRecord (unsigned  array,
                                    uint64_t* next ) const
{
  return get(array,0,next);
}

Record*  AmcCarrierBase::getRecord (unsigned array,
                                    uint64_t begin) const
{
  uint64_t next;
  return get(array,begin,&next);
}

uint8_t*  AmcCarrierBase::getBuffer (uint64_t begin,
                                     uint64_t end  ) const
{
  uint8_t* p = new uint8_t[end-begin];
  _fill( p, begin, end);
  return p;
}

Record*  AmcCarrierBase::get       (unsigned array,
                                    uint64_t begin,
                                    uint64_t* next) const
{
  Record& record = *new Record;
  record.buffer = array;

  uint64_t start=0,end=0;
  unsigned wrap=0;
  {
    uint64_t v;
    IndexRange rng(array);
    _tstamp->getVal(&v,1,&rng);
    
    record.time_secs  = v>>32;
    record.time_nsecs = v&0xffffffff;

    _startAddr->getVal(&start,1,&rng);
    if (begin < start)
      begin = start;
    _wrAddr->getVal(&end,1,&rng);

    uint64_t last;
    _endAddr->getVal(&last,1,&rng);


#ifdef DBUG
    printf("DBUG:  array %u  done %u  startAddr 0x%016llx  endAddr 0x%016llx  wrAddr 0x%016llx  begin 0x%016llx\n",
           array, status(array), start, last, end, begin);
#endif

    if (end < start or end > last) {
      printf("Trap BSA ptr error:  array %u  startAddr 0x%016llx  endAddr 0x%016llx  wrAddr 0x%016llx  begin 0x%016llx.  Resetting\n",
             array, start, last, end, begin);
      uint32_t uzro=0,uone=1;
      _sInit    ->setVal(&uone,1,&rng);
      _sInit    ->setVal(&uzro,1,&rng);
      record.entries.resize(0);
      return &record;
    }

    if (begin > last)
      begin = last;

    _sFull->getVal(&wrap     ,1,&rng);
    if (end <= begin && wrap) {
      uint64_t nb      = last-begin+end-start;
      unsigned entries = nb/sizeof(Entry);
      end += sizeof(Entry)*entries - nb;
      record.entries.resize( entries );
      _fill( record.entries.data(), 
             begin, 
             last );
      _fill( reinterpret_cast<uint8_t*>(record.entries.data())+int(last-begin),
             start, 
             end );
    }
    else {
      unsigned entries = (end -begin)/sizeof(Entry);
      if (entries) {
        end = begin+entries*sizeof(Entry);
        record.entries.resize(entries);
        _fill( record.entries.data(),
               begin,
               end );
      }
    }
  }

  *next = end;

  return &record;
}

void AmcCarrierBase::_fill(void*    dst,
                           uint64_t begin,
                           uint64_t end) const
{
  begin >>= 3;
  end   >>= 3;

  const unsigned BLOCK_SIZE = 4096>>3;
  IndexRange rng(begin, begin + BLOCK_SIZE - 1);
  uint64_t* p = reinterpret_cast<uint64_t*>(dst);

  while( rng.getFrom() < int64_t(end) ) {
    
    if (rng.getTo() >= int64_t(end))
      rng.setTo( end-1 );
    
    _dram->getVal( &p[rng.getFrom()-begin], BLOCK_SIZE, &rng );
    ++rng;
  }
}

#define PR36(name) {                                                    \
    uint64_t v;                                                         \
    ScalVal_RO s = IScalVal_RO::create( path->findByName(#name));        \
    s->getVal(&v,1,&rng);                                               \
    printf("%09llx ",(unsigned long long)(v));                          \
  }


void AmcCarrierBase::_printBuffer(Path path, ScalVal_RO ts, unsigned i,
                                  uint64_t done , uint64_t full, 
                                  uint64_t empty, uint64_t error) const
{
    IndexRange rng(i);
    printf("%4.4x ",i);
    PR36(StartAddr); 
    PR36(EndAddr);
    PR36(WrAddr);
    PR36(TriggerAddr);
    uint64_t tstamp;
    ts->getVal(&tstamp,1,&rng);
    printf("%10.10u.%09u ",unsigned(tstamp>>32),unsigned(tstamp&0xffffffff));

    printf("%4.4s ", (done &(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (full &(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (empty&(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (error&(1ULL<<i)) ? "X": "-");
    printf("\n");

}

void AmcCarrierBase::_printBuffer(Path path, unsigned i,
                                  uint64_t done , uint64_t full, 
                                  uint64_t empty, uint64_t error) const
{
    IndexRange rng(i);
    printf("%4.4x ",i);
    PR36(StartAddr); 
    PR36(EndAddr);
    PR36(WrAddr);
    PR36(TriggerAddr);
    printf("%20.20s ", "-");

    printf("%4.4s ", (done &(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (full &(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (empty&(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (error&(1ULL<<i)) ? "X": "-");
    printf("\n");

}

