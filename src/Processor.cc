#include "Processor.hh"
#include "AmcCarrier.hh"
#include "AmcCarrierYaml.hh"
#include "BsaDefs.hh"

#include <cpsw_api_builder.h>

#include <cmath>
#include <math.h>
#include <ctime>
#include <queue>
#include <stdio.h>
#include <time.h>
#include <syslog.h>

#define DONE_WORKAROUND

#include <algorithm>
#include <cstddef>

static unsigned _nReadout = 1024 * 128;
static const unsigned MAXREADOUT = 1<<20;

static char* timestr() 
{
  time_t t = time(NULL);
  char* v = asctime(localtime(&t));
  *strchr(v,'\n')=0; // strip the carriage return
  return v;
}

namespace Bsa {

  class Reader {
  public:
    static void     set_nReadout(unsigned v) {_nReadout=v;} 
    static unsigned get_nReadout() { return _nReadout;}
  public:
    Reader() : _timestamp(0), _next(0), _last(0), _end(0), _preset(0), 
               _abort(false), _record(MAXREADOUT) {}
    ~Reader() {}
  public:
    void     abort() { _abort = true; }
    bool     done () const { return _next==_last; }
    void     preset (const ArrayState&    state) { _preset = state.wrAddr; }
    bool     reset(PvArray&              array, 
                   const ArrayState&     state,
                   AmcCarrierBase&       hw,
                   uint64_t              timestamp)
    {
      if (_abort) {
        _next = _last = state.wrAddr;
        _abort = false;
        return false;
      }

      //  Check if wrAddr pointer has moved.  If so, the acquisition isn't done
      if (state.wrAddr != _preset) {
        syslog(LOG_WARNING,"<W> %s:  %s:%-4d [reset]: not ready 0x%09llx  wrAddr 0x%09llx",
               timestr(),__FILE__,__LINE__,_preset,state.wrAddr);
        return false;
      }

      _timestamp = state.timestamp;
      //  If wrap is set, the buffer is complete from wrAddr -> end; start -> wrAddr
      //  Else, if the buffer was read before, continue from where we ended
      //  Else read from the start
      unsigned iarray = array.array();
      _start = hw._begin[iarray];
      _end   = hw._end  [iarray];
      _next = state.wrap ? state.wrAddr : (_last ? _last : _start);
      _last = state.wrAddr;

      //  Check if wrAddr is properly aligned to the record size.
      unsigned n0 = _last / sizeof(Entry);
      if (n0*sizeof(Entry) != _last) {
        syslog(LOG_WARNING,"<W> %s:  %s:%-4d [reset] misaligned _last 0x%09llx",
               timestr(),__FILE__,__LINE__,_last);
        return false;
      }

      array.reset(timestamp>>32,timestamp&0xffffffff);

      uint64_t hw_done = hw.done();
      syslog(LOG_DEBUG,"<D> %s:  %s:%-4d []  array %u  _timestamp 0x%016llx  _next 0x%09llx  _last 0x%09llx  __end 0x%09llx  done 0x%09llx",
             timestr(),__FILE__,__LINE__,array.array(),_timestamp,_next,_last,_end,hw_done);

      return true;
    }

    Record* next(PvArray& array, AmcCarrierBase& hw)
    {
      if (_abort) {
        _next = _last;
        _record.entries.resize(0);
        return &_record;
      }

      unsigned n = _nReadout;
      //  _next : start of current read
      //  _last : end of final read
      //  _end  : end of buffer where we need to wrap
      uint64_t next = _next + n*sizeof(Entry);  // end of current read
      uint64_t nnext = next;                    // start of next read

      //  What are the possibilities?
      //    (start)                   (_last)                (_end)
      //     1.      _next      next                                        read _next:next
      //     2.      _next                      next                        read _next:_last
      //     3.      _next                                           next   read _next:_last
      //     4.                                _next    next                read _next:next
      //     5.                                _next                 next   read _next:_end; _next=start
      if ((_next <  _last && next < _last) ||
          (_next >= _last && next < _end)) {
        //  read _next:next
      }
      else if (_next < _last) {
        //  read _next:_last
        n = (_last - _next) / sizeof(Entry);
        nnext = _last;
        next  = _last;
      }
      else {
        //  read _next:_end
        next = _end;
        n = (next - _next) / sizeof(Entry);
        nnext = _start;
      }

      if (n > MAXREADOUT) {
        syslog(LOG_ERR,"<E> Reader::next allocating record with %u entries", n);
        throw(std::string("Too many entries"));
      }

      Record& record = _record;
      record.entries.resize(n);

      hw._fill(record.entries.data(), _next, next);

      //  Detect mis-aligned entries
      unsigned n0 = _next / sizeof(Entry);
      if (n0*sizeof(Entry) != _next) {
        const Entry& e = record.entries[0];
        syslog(LOG_ERR,"<E> %s  %s:%-4d [Misaligned record]  _next 0x%09llx  nch %u  pid 0x%09llx",
               timestr(),__FILE__,__LINE__,_next,e.nchannels(),e.pulseId());
      }
      
      //  _last or _end dont occur at an Entry boundary
      if (_next + n*sizeof(Entry) != next) {
        syslog(LOG_ERR,"<E> %s:  %s:%-4d [Truncated record]  _next 0x%09llx  next 0x%09llx  _last 0x%09llx  _end 0x%09llx  _next+n 0x%09llx  n %u",
               timestr(),__FILE__,__LINE__,_next,next,_last,_end,_next+n*sizeof(Entry),n);
      }

      _next = nnext;

      if (done()) {
        hw   .reset(array.array());
        array.set(_timestamp>>32, _timestamp&0xffffffff);
        uint64_t hw_done = hw.done();
        syslog(LOG_DEBUG,"<D> %s:%-4d [done]:  array %u  hw.done 0x%09llx",__FILE__,__LINE__,array.array(),hw_done);
      }

      return &record;
    }
  private:
    AmcCarrierBase* _hw;
    uint64_t _start;
    uint64_t _end;
    uint64_t _timestamp;
    uint64_t _next;
    uint64_t _last;
    uint64_t _preset;
    bool     _abort;
    Record   _record;
  };

  class ProcessorImpl : public Processor {
  public:
    ProcessorImpl(Path reg,
                  Path ram,
                  bool lInit) : _hw(*new AmcCarrierYaml(reg,ram))
    {
      if (lInit) _hw.initialize();
      for(unsigned i=0; i<HSTARRAYN; i++) {
	_state[i].next = _hw._begin[i];
	syslog(LOG_DEBUG,"<D> %s:  %s:%-4d [ProcessorImpl] next[%u] 0x%09llx",
	       timestr(),__FILE__,__LINE__, i,_state[i].next);
      }
    }
    ProcessorImpl(const char* ip,
		  bool lInit,
		  bool lDebug) : _hw(*new AmcCarrier(ip))
    {
      if (lInit) _hw.initialize();
      for(unsigned i=0; i<HSTARRAYN; i++)
	_state[i].next = _hw._begin[i];
    }
    ProcessorImpl() : _hw(*AmcCarrier::instance())
    {
      syslog(LOG_WARNING,"<W> %s:  %s:%-4d [ProcessorImpl]",
	     timestr(),__FILE__,__LINE__);
    }
    ~ProcessorImpl();
  public:
    uint64_t pending();
    int      update(PvArray&);
    AmcCarrierBase *getHardware();
  private:
    void     abort (PvArray&);
    AmcCarrierBase&      _hw;
    ArrayState           _state [HSTARRAYN];
    Reader               _reader[HSTARRAYN-HSTARRAY0];
    std::queue<unsigned> _readerQueue;
    Record               _emptyRecord;

    void  procChannelData      (const Entry*, Pv*, Pv*, bool&, bool);
    void  llrfPerformChecks    (const Bsa::Pv*, const Bsa::Pv*, int);
    void  llrfCalcPhaseAmp     (signed short, signed short, double&, double&);
  };

};

using namespace std;
using namespace Bsa;

AmcCarrierBase *ProcessorImpl::getHardware()
{
    return &_hw;
}

uint64_t ProcessorImpl::pending()
{
  const std::vector<ArrayState>& s = _hw.state();

  uint64_t r = 0;
  for(unsigned i=0; i<HSTARRAY0; i++)
    if (s[i] != _state[i])
      r |= 1ULL<<i;

  r |= ~((1ULL<<HSTARRAY0)-1);

  uint64_t done = _hw.done();
  done |= (1ULL<<HSTARRAY0)-1;
  r &= done;

  return r;
}

void ProcessorImpl::llrfPerformChecks(const Bsa::Pv* pv, const Bsa::Pv* pvN, int bitIndex)
{
    // Check if channel pointers are nullptr
    if (pv == NULL || pvN == NULL)
    {
        printf("BsaPvArray::llrfPerformChecks(): ERROR - LLRF BSA channels are nullptr!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Check LLRF BSA channels!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Exiting ...\n");
        exit(EXIT_FAILURE);
    }
    // Ensure bit index points to the start of the channel
    if (bitIndex != 0)
    {
        printf("BsaPvArray::llrfPerformChecks(): ERROR - LLRF BSA channel not aligned with LSB!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Check LLRF BSA channel order!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Exiting ...\n");
        exit(EXIT_FAILURE);
    }
    // Ensure the types of consecutive channels are valid (phase->amp or amp->phase)
    if ((*pv->get_p_type() == llrfAmp   && *pvN->get_p_type() == llrfPhase) ||
        (*pv->get_p_type() == llrfPhase && *pvN->get_p_type() == llrfAmp  )   ){}
    else
    {
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Did NOT find consecutive Amplitude and Phase channels!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Check LLRF BSA channel types!!\n");
        printf("BsaPvArray::llrfPerformChecks(): ERROR - Exiting ...\n");
        exit(EXIT_FAILURE);
    }
}

void ProcessorImpl::llrfCalcPhaseAmp(signed short i, signed short q, double& amp, double& phase)
{
    // Calculate amplitude
    amp = (!isnan(i) && !isnan(q) && (i != 0))?sqrt((double)(i) * (double)(i) + (double)(q) * (double)(q)):NAN;
    // Calculate phase
    phase = (!isnan(i) && !isnan(q) && i != 0)?atan2((double)q, (double)i) * M_PI_DEGREES / M_PI:NAN;
}

void ProcessorImpl::procChannelData(const Entry* entry, Pv* pv, Pv* pvN, bool& skipNextPV, bool done)
{
    uint32_t       mask, val;
    signed short   iVal, qVal;
    bool           appendNAN;
    double         amp, phase, quant1, quant2;

    // Keep track of bit boundaries
    static unsigned bitSum = 0;

    // Keep track of hardware channel index
    static unsigned wordIndex = 0;
    
    // Get the data type of the PV (32-bit or less)
    bsaDataType_t *type = pv->get_p_type();
            
    // Partition 32-bit data and distribute to new PVs as needed
    appendNAN = false;
    switch(*type){
      case uint2:
        // Extract the 2-bit block
        if (!isnan(entry->channel_data[wordIndex].mean()))
        {
          val = (uint32_t)entry->channel_data[wordIndex].mean();
          mask = KEEP_LSB_2;val >>= bitSum;val &= mask;
        }
        else
          appendNAN = true;
        bitSum += BLOCK_WIDTH_2;
        break; 
      case uint16:
        // Extract the 16-bit block
        if (!isnan(entry->channel_data[wordIndex].mean()))
        {
          val = (uint32_t)entry->channel_data[wordIndex].mean();
          mask = KEEP_LSB_16;val >>= bitSum;val &= mask;
        }
        else
          appendNAN = true;
        bitSum += BLOCK_WIDTH_16;
        break;
      case llrfAmp:
      case llrfPhase:
        // Perform checks to ensure type validity
        llrfPerformChecks(const_cast<Bsa::Pv*>(pv),const_cast<Bsa::Pv*>(pvN),bitSum);
        // Get data and extract I/Q 
        if (!isnan(entry->channel_data[wordIndex].mean()))
        {
          // Read all 32 bits
          val  = (uint32_t)entry->channel_data[wordIndex].mean();
          // Extract lower 16 bits
          mask = KEEP_LSB_16; 
          iVal = static_cast<signed short>(val & mask);
          // Extract upper 16 bits
          qVal = static_cast<signed short>((val >> BLOCK_WIDTH_16) & mask);
          // Compute phase & amplitude
          llrfCalcPhaseAmp(iVal, qVal, amp, phase);
          // Append computed values to PVs
          quant1 = (*type == llrfAmp)?amp:phase;
          quant2 = (quant1 == amp   )?phase:amp;
        }
        else
        {
          quant1 = NAN;
          quant2 = NAN;
        }
        pv->append  (entry->channel_data[wordIndex].n(), 
                     quant1,
                     entry->channel_data[wordIndex].rms2());
        pvN->append (entry->channel_data[wordIndex].n(), 
                     quant2,
                     entry->channel_data[wordIndex].rms2());
        skipNextPV = true;
        bitSum += 2 * BLOCK_WIDTH_16;
        break;
      case int32:
      case uint32:
      case float32:
      default:
        // Send data as is (no partition required)
        if (!isnan(entry->channel_data[wordIndex].mean()))
          val = (uint32_t)entry->channel_data[wordIndex].mean();
        else
          appendNAN = true;
        bitSum += BLOCK_WIDTH_32;
    }
    // Append the value to the corresponding PV history buffer      
    if (*type != llrfAmp && *type != llrfPhase && !appendNAN)
      pv->append(entry->channel_data[wordIndex].n(), 
                 (double)val, 
                 entry->channel_data[wordIndex].rms2());
    else if (*type != llrfAmp && *type != llrfPhase && appendNAN)
      pv->append(entry->channel_data[wordIndex].n(), 
                 NAN, 
                 entry->channel_data[wordIndex].rms2());

    // Check if the 32-bit boundary has been violated
    if (bitSum == BLOCK_WIDTH_32)
    {
      // All good, move on to the next 32-bit word
      bitSum = 0;
      // Increment index to next channel
      wordIndex++;
    }
    else if (bitSum > BLOCK_WIDTH_32)
    {
      printf("ProcessorImpl::procChannelData(): ERROR - Please ensure BSA channels do not violate 32-bit boundaries!!\n");
      printf("ProcessorImpl::procChannelData(): ERROR - Check BSA channel type!!\n");
      printf("ProcessorImpl::procChannelData(): ERROR - Exiting ...\n");
      exit(EXIT_FAILURE);
    }

    // Reset hardware channel index if done with all hardware channels
    wordIndex = (done)?0:wordIndex;
}

int ProcessorImpl::update(PvArray& array)
{
  unsigned      iarray  = array.array();
  std::vector<Pv*> pvs  = array.pvs();
  unsigned    numOfPVs  = pvs.size();

  ArrayState current(_hw.state(iarray));

  Record* record;

  try {

    // syslog(LOG_DEBUG,"<W> %s:  %s:%-4d []: array %u  next 0x%09llx",
    // 	   timestr(),__FILE__,__LINE__,iarray,_state[iarray].next);

    if (array.array() < HSTARRAY0) {
      if (current != _state[iarray]) {
        current.nacq = _state[iarray].nacq;
        if (current.clear) {
          //
          //  New acquisition;  start from the beginning of the circular buffer
          //
          _hw.ackClear(iarray);

          syslog(LOG_DEBUG,"<D> %s:  %s:%-4d [current %d]: NEW TS [%u.%09u -> %u.%09u]  wrAddr %09llx\n",
                 timestr(),__FILE__,__LINE__,iarray,
                 _state[iarray].timestamp>>32,
                 _state[iarray].timestamp&0xffffffff,
                 current.timestamp>>32,
                 current.timestamp&0xffffffff,
                 current.wrAddr);

          array.reset(current.timestamp>>32,
                      current.timestamp&0xffffffff);
          current.nacq = 0; 
          record = _hw.getRecord(iarray,&current.next);  // read from beginning
        }
        else {
          //
          //  Incremental update
          //
          array.set(current.timestamp>>32,
                    current.timestamp&0xffffffff);
          record = _hw.get(iarray,_state[iarray].next,&current.next);
        }

      }
    }
    else {  // >= HSTARRAY0

      syslog(LOG_DEBUG,"<D> %s:  %s:%-4d [current %d]: wrAddr %09llx  next %09llx  clear %u  wrap %u  nacq %u",
             timestr(),__FILE__,__LINE__,iarray,current.wrAddr,current.next,current.clear,current.wrap,current.nacq);

      unsigned ifltb = array.array()-HSTARRAY0;
      Reader& reader = _reader[ifltb];
      if (_readerQueue.empty()) {
        reader.preset(current);  // prepare check for erroneous hw.done signal
        _readerQueue.push(ifltb);
        record = &_emptyRecord;
      }
      else if (_readerQueue.front()==ifltb) {
        if (reader.done()) {
          //  A new fault was latched
          current.nacq = 0;
          if (reader.reset(array,current,_hw,current.timestamp-(1ULL<<32))) {
            record = reader.next(array,_hw);
          }
          else {
            // We got the wrong done signal.  Find the correct one and queue it.
            _hw.reset(iarray);
            _readerQueue.pop();
            return 0;  // don't try to correct anything, just skip
          }
        }
        else {
          //  A previous fault readout is in progress
          record = reader.next(array,_hw);
        }

        if (reader.done())
          _readerQueue.pop();
      }
      else {  // Some other fault buffer readout is in progress
        record = &_emptyRecord;
      }
    }

    // syslog(LOG_DEBUG,"<W> %s:  %s:%-4d []: array %u  entries %u",
    // 	   timestr(),__FILE__,__LINE__,iarray,record->entries.size());

    for(unsigned i=0; i<record->entries.size(); i++) {
      const Entry& entry = record->entries[i];
      //  fill pulseid waveform
      array.append(entry.pulseId());
      //  fill channel data waveforms
      for(unsigned j=0; j<pvs.size(); j++)
        pvs[j]->append(entry.channel_data[j].n(),
                       entry.channel_data[j].mean(),
                       entry.channel_data[j].rms2());
    }

  unsigned numOfEntries = record->entries.size();
  
  for(unsigned i = 0; i < numOfEntries; i++) 
  {
    // Process next entry (i.e. pulse ID)
    const Entry& entry = record->entries[i];

    // Fill pulseid waveform
    array.append(entry.pulseId());

    // Fill channel data waveforms
   
   /*
    // Method 1: Call Bsa::PvArray::procChannelData() 
    // Push data to a vector in bsaDriver and extract data in there
    for(unsigned j = 0; j < std::min(numChannelData, (const int&)numOfPVs); j++)
    {
      // Adding new call to procChannelData() here that will do the extraction of the channel data.
      // A boolean argument indicates if we are done sending all the channel data for the current pulse. 
      // Note the data are sent as 32-bit chunks and those are splitted as needed in the Asyn driver.
      array.procChannelData(entry.channel_data[j].n(),
                            entry.channel_data[j].mean(),
                            entry.channel_data[j].rms2(),
                            (j == std::min(numChannelData - 1, (const int&)numOfPVs - 1)));
    }
   */
    
    // Method 2: Call ProcessorImpl::procChannelData() 
    // Loop over PVs, extract data from hardware channels and assign to PVs 
    for(unsigned int j = 0; j < numOfPVs; j++)
    {
      // Call function to do the extraction
      Pv* pv  = pvs[j]; bool skip = false;
      Pv* pvN = ((j+1) < numOfPVs)?pvs[j+1]:pv;
      if (pv && pvN) procChannelData(&entry, pv, pvN, skip, j == (numOfPVs - 1));
      j = (skip)?j+1:j;
    }
  }

  current.nacq += numOfEntries;
  _state[iarray] = current;
  }
  catch(...) {
    // Something bad happened.  Abort this acquisition.
    syslog(LOG_ERR,"<E> %s:  %s:%-4d [current %d]: caught exception. abort. next 0x%09llx  wrAddr 0x%09llx  ts 0x%016llx",
           timestr(),__FILE__,__LINE__,iarray,_state[iarray].next,_state[iarray].wrAddr,_state[iarray].timestamp);
    abort(array);
    return 0;
  }

  return current.nacq;
}

//
//  Clear the state of this acquisition so it doesn't retry later.
//
void ProcessorImpl::abort(PvArray& array)
{
  int iarray = array.array();
  
  ArrayState current(_hw.state(iarray));
  array.reset(current.timestamp>>32,
              current.timestamp&0xffffffff);

  if (iarray < HSTARRAY0) {
  }
  else {
    unsigned ifltb = iarray-HSTARRAY0;
    Reader& reader = _reader[ifltb];
    if (!_readerQueue.empty() &&
        _readerQueue.front()==ifltb) {
      Reader& reader = _reader[ifltb];
      reader.abort();
    }
    else {
      // nothing to clear
    }
  }

  //  Still a firmware bug that allows WrAddr to go out of bounds.  Let's reset upon detection.
  _hw.reset(iarray);
  _state[iarray].next = _hw._begin[iarray];
}

Processor* Processor::create(Path reg,
                             Path ram,
                             bool lInit)
{
  return new ProcessorImpl(reg,ram,lInit);
}

Processor* Processor::create(const char* ip,
                             bool lInit,
                             bool lDebug)
{
  return new ProcessorImpl(ip,lInit,lDebug);
}

Processor* Processor::create()
{
  return new ProcessorImpl();
}

ProcessorImpl::~ProcessorImpl()
{
}
