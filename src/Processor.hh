#ifndef Bsa_Processor_hh
#define Bsa_Processor_hh

#include "BsaField.hh"
#include "AmcCarrierBase.hh"

#include <vector>
#include <stdint.h>

#include <cpsw_api_user.h>

namespace Bsa {
  //
  //  Class that defines the interface from the BSA processor to a PV record
  //
  class Pv {
  public:
    virtual ~Pv() {}
  public:
    //
    //  Field that this record represents
    //
    //    virtual const Field& field() const = 0;
  public:
    //
    //  Update the timestamp of the record
    //
    virtual void setTimestamp(unsigned sec, 
                              unsigned nsec) = 0;
    //
    //  Clear the data contents of the record
    //
    virtual void clear() = 0;
    //
    //  Append an entry to the record
    //
    //    virtual void append() = 0;  // exception during acquisition
    virtual void append(unsigned n,   // could be zero
                        double   mean,
                        double   rms2) = 0;
    //
    //  Flush the data out
    //
    virtual void flush() = 0;
  };

  //
  //  Class that consists of the PV records for one BSA array
  //
  class PvArray {
  public:
    virtual ~PvArray() {}
  public:
    //
    //  The BSA array number service by these PVs
    //
    virtual unsigned array() const = 0;
    //
    //  Clear array and start with new timestamp
    //
    virtual void reset(uint32_t sec,
                       uint32_t nsec) = 0;
    //
    //  Update timestamp
    //
    virtual void set(uint32_t sec,
                     uint32_t nsec) = 0;
    //
    //
    //
    virtual void append(uint64_t pulseId) = 0;
    //
    //  The PV records assigned to this array number
    //  (element order matches diagnostic bus element order)
    //
    virtual std::vector<Pv*> pvs() = 0;
  };

  //
  //  The interface for updating PV records from the AmcCarrier buffers
  //
  class Processor {
  public:
    virtual ~Processor() {}
    //
    //  Create the interface to the AmcCarrier with paths to register and ram access
    //
    static Processor* create(Path reg, Path ram, bool lInit=false);
    //
    //  Create the interface to the AmcCarrier with IP address "ip"
    //
    static Processor* create(const char* ip, bool lInit=false, bool lDebug=false);
    //
    //  Share the interface to the AmcCarrier
    //
    static Processor* create();
  public:
    //
    //  Fetch a bit mask of arrays with pending data
    //
    virtual uint64_t pending() = 0;
    //
    //  Update the array of PV records for one BSA buffer
    //  Return value indicates if any records were changed
    //
    virtual int update(PvArray&) = 0;
    //
    //  Abort an acquisition readout
    //
    virtual void abort(PvArray&) = 0;
    //
    //  
    //
    virtual AmcCarrierBase *getHardware() = 0;
  };
};

#endif
