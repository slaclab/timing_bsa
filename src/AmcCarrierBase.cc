#include "AmcCarrierBase.hh"

#include <stdio.h>

using namespace Bsa;

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

AmcCarrierBase::AmcCarrierBase() : _state(64)
{
}

void     AmcCarrierBase::initialize()
{
  uint64_t p=0,pn=0;
  uint32_t uzro=0,uone=1;

#if 0
  for(unsigned i=0; i<64; i++) {
    IndexRange rng(i);
    _sEnabled ->setVal(&uzro,1,&rng);
  }
  usleep(1);
#endif

  //  Setup the standard BSA arrays (32k entries)
  const uint64_t bsaSize = 3ULL<<7;  // one entry
  for(unsigned i=0; i<60; i++) {
    pn = p+(1ULL<<8)*bsaSize;   // shrink to 256 entries for debugging purpose
    IndexRange rng(i);
    _startAddr->setVal(&p   ,1,&rng);
    _endAddr  ->setVal(&pn  ,1,&rng);
    _sEnabled ->setVal(&uone,1,&rng);
    _sMode    ->setVal(&uzro,1,&rng);
    _sInit    ->setVal(&uone,1,&rng);
    _sInit    ->setVal(&uzro,1,&rng);
    p = pn;
  }
  //  Setup the fault arrays to be LARGER (1M entries)
  for(unsigned i=60; i<64; i++) {
    pn = p+(1ULL<<18)*bsaSize;    // shrink to 256k entries for debugging purpose
    IndexRange rng(i);
    _startAddr->setVal(&p   ,1,&rng);
    _endAddr  ->setVal(&pn  ,1,&rng);
    _sEnabled ->setVal(&uone,1,&rng);
    _sMode    ->setVal(&uzro,1,&rng);
    _sInit    ->setVal(&uone,1,&rng);
    _sInit    ->setVal(&uzro,1,&rng);
    p = pn;
  }

  _memEnd = p;
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
  _sEmpt->getVal(v,64);
  for(unsigned i=0; i<64; i++) {
    if (!v[i])
      r |= 1ULL<<i;
  }
  return r;
}

uint64_t AmcCarrierBase::done      () const
{
  uint64_t r=0;
  uint32_t v[64];
  memset(v,0,64*sizeof(uint32_t));
  _sStatus->getVal(v,64);
  for(unsigned i=0; i<64; i++) {
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
  uint64_t tstamp[64];
  uint64_t wrAddr[64];
  unsigned clear [64];
  _tstamp->getVal(tstamp,64);
  _sClear->getVal(clear ,64);
  _wrAddr->getVal(wrAddr,64);
  for(unsigned i=0; i<64; i++) {
    _state[i].timestamp = tstamp[i];
    _state[i].clear     = clear [i];
    _state[i].wrAddr    = wrAddr[i];
  }

#if 0
  for(unsigned i=0; i<64; i++)
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

    if (end < start) {
      //  This should never happen
      printf("Trap BSA ptr error:  array %u  startAddr 0x%016llx  endAddr 0x%016llx  wrAddr 0x%016llx\n",
             array, start, last, end);
      record.entries.resize(0);
      return &record;
    }

    if (end > last)
      end = last;

    if (begin > last)
      begin = last;

    _sFull->getVal(&wrap     ,1,&rng);
    if (end <= begin && wrap) {
      unsigned tail_entries = (last-begin)/sizeof(Entry);
      unsigned head_entries = (end -start)/sizeof(Entry);
      end = start + head_entries*sizeof(Entry);
      record.entries.resize( tail_entries + head_entries );
      _fill( record.entries.data(), 
             begin, 
             last );
      _fill( record.entries.data()+tail_entries, 
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

