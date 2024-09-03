//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include "AmcCarrierBase.hh"
#include "BsaDefs.hh"

#include <stdio.h>
#include <syslog.h>

using namespace Bsa;

//#define BURSTSIZE 0x800
//#define DBUG

#ifdef DBUG
static const uint64_t BSASIZE_U   = 1<<8;
static const uint64_t BSASIZE_L   = 1ULL<<8;
#else
static const uint64_t BSASIZE_U   = 1<<15;
static const uint64_t BSASIZE_L   = 1ULL<<15;
#endif
static const unsigned FAULTSIZE   = 1<<20;
static const uint64_t FAULTSIZE_L = 1ULL<<20;

static char* timestr() 
{
  time_t t = time(NULL);
  char* v = asctime(localtime(&t));
  *strchr(v,'\n')=0; // strip the carriage return
  return v;
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

AmcCarrierBase::AmcCarrierBase() : _state(HSTARRAYN), _record(FAULTSIZE)
{
}

void     AmcCarrierBase::initialize()
{
  uint64_t p=0,pn=0,pe=0;
  uint32_t uzro=0,uone=1;

  //  Setup the standard BSA arrays (32k entries)
  //  Must be a multiple of bsaSize and burstSize(2kB)
  const uint64_t bsaSize = 3ULL<<7;  // one entry
  _begin.resize(HSTARRAYN);
  _end  .resize(HSTARRAYN);
  for(unsigned i=0; i<NBSAARRAYS; i++) {
    pn = p+BSASIZE_L*bsaSize;
    //    pe = pn - BURSTSIZE;
    pe = pn;
    _begin[i] = p;
    _end  [i] = pe;
    IndexRange rng(i);
    _startAddr->setVal(&p   ,1,&rng);
    _endAddr  ->setVal(&pe  ,1,&rng);
    _sEnabled ->setVal(&uone,1,&rng);
    _sMode    ->setVal(&uzro,1,&rng);
    _sInit    ->setVal(&uone,1,&rng);
    _sInit    ->setVal(&uzro,1,&rng);
    syslog(LOG_DEBUG,"<D>  array %u  startAddr 0x%09llx  endAddr 0x%09llx",
           i, p, pe);
    p = pn;
  }
  //  Setup the fault arrays to be LARGER (1M entries)
  for(unsigned i=HSTARRAY0; i<HSTARRAYN; i++) {
    pn = p+FAULTSIZE_L*bsaSize;
    //    pe = pn - BURSTSIZE;
    pe = pn;
    _begin[i] = p;
    _end  [i] = pe;
    IndexRange rng(i);
    _startAddr->setVal(&p   ,1,&rng);
    _endAddr  ->setVal(&pe  ,1,&rng);
    _sEnabled ->setVal(&uone,1,&rng);
    _sMode    ->setVal(&uzro,1,&rng);
    _sInit    ->setVal(&uone,1,&rng);
    _sInit    ->setVal(&uzro,1,&rng);
    syslog(LOG_DEBUG,"<D>  array %u  startAddr 0x%09llx  endAddr 0x%09llx",
           i, p, pe);
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

  return _state;
}

Record*  AmcCarrierBase::getRecord (unsigned  array) const
{  
  uint64_t next;
  return get(array,_begin[array],&next);
}

Record*  AmcCarrierBase::getRecord (unsigned  array,
                                    uint64_t* next ) const
{
  return get(array,_begin[array],next);
}

Record*  AmcCarrierBase::getRecord (unsigned array,
                                    uint64_t begin) const
{
  uint64_t next;
  return get(array,begin,&next);
}

//
//  This method is only used for testing.  So, allow the dynamic allocation.
//
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
  Record& record = _record;
  record.buffer = array;

  uint64_t start=_begin[array];
  uint64_t last =  _end[array];
  uint64_t end;
  unsigned wrap=0;
  {
    uint64_t v;
    IndexRange rng(array);
    _tstamp->getVal(&v,1,&rng);
    
    record.time_secs  = v>>32;
    record.time_nsecs = v&0xffffffff;

    if (begin < start || begin > last) { // This is an error
      syslog(LOG_ERR,"<E> %s  %s:%-4d [Begin out of bounds]  begin 0x%09llx  startAddr 0x%09llx  endAddr 0x%09llx",
             timestr(),__FILE__,__LINE__,begin,start,last);
      throw("fetch begin out of bounds");
    }

    _wrAddr->getVal(&end,1,&rng);

    if (end < start or end > last) {
      syslog(LOG_ERR,"<E> %s  %s:%-4d [End out of bounds]  wrAddr 0x%09llx  startAddr 0x%09llx  endAddr 0x%09llx",
             timestr(),__FILE__,__LINE__,end,start,last);
      throw("fetch end out of bounds");
    }

    _sFull->getVal(&wrap     ,1,&rng);

    if (end == begin && end == start && !wrap) {  // Trap a common error
      syslog(LOG_ERR,"<E> %s  %s:%-4d [No data to read]  wrAddr 0x%09llx",
             timestr(),__FILE__,__LINE__,end);
      throw("fetch no data to read");
    }

    if (end <= begin /*&& wrap*/) {
      uint64_t nb      = last-begin+end-start;
      unsigned entries = nb/sizeof(Entry);

      if(!wrap) {
        syslog(LOG_ERR,"<E> %s  %s:%-4d [Wrap flag issue] reading %u entries (array (%u), begin 0x%09llx, end 0x%09llx)",
               timestr(),__FILE__,__LINE__, entries, array, begin, end);
        throw("Wrap flag issue");
      }
      if ((array <  HSTARRAY0 && entries > BSASIZE_U) ||
          (array >= HSTARRAYN && entries > FAULTSIZE)) {
        syslog(LOG_ERR,"<E> %s  %s:%-4d [oversize] reading %u entries (array (%u), begin 0x%09llx, end 0x%09llx)",
               timestr(),__FILE__,__LINE__, entries, array, begin, end);
        throw("Entries > MAXSIZE");
      }
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
      if ((array <  HSTARRAY0 && entries > BSASIZE_U) ||
          (array >= HSTARRAYN && entries > FAULTSIZE)) {
        syslog(LOG_ERR,"<E> %s  %s:%-4d [oversize] reading %u entries (array (%u), begin 0x%09llx, end 0x%09llx)",
               timestr(),__FILE__,__LINE__, entries, array, begin, end);
        throw("Entries > MAXSIZE");
      }
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

static void printAddr(const Path& path, const char* name, IndexRange& rng) {
	uint64_t v;
	try {
		ScalVal_RO s = IScalVal::create(path->findByName(name));
		s->getVal(&v, 1, &rng);
		printf("%09llx ", (unsigned long long)(v));
	}
	catch(const CPSWError& e) {
		printf("%s print failed: %s\n", name, e.getInfo().c_str());
	}
	catch(...) {
		printf("%s print failed: unknown error\n", name);
	}
}

void AmcCarrierBase::_printBuffer(Path path, ScalVal_RO ts, unsigned i,
                                  uint64_t done , uint64_t full, 
                                  uint64_t empty, uint64_t error) const
{
    IndexRange rng(i);
    printf("%4.4x ",i);
    printAddr(path, "StartAddr", rng); 
    printAddr(path, "EndAddr", rng);
    printAddr(path, "WrAddr", rng);
    printAddr(path, "TriggerAddr", rng);
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
    printAddr(path, "StartAddr", rng); 
    printAddr(path, "EndAddr", rng);
    printAddr(path, "WrAddr", rng);
    printAddr(path, "TriggerAddr", rng);
    printf("%20.20s ", "-");

    printf("%4.4s ", (done &(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (full &(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (empty&(1ULL<<i)) ? "X": "-");
    printf("%4.4s ", (error&(1ULL<<i)) ? "X": "-");
    printf("\n");

}

