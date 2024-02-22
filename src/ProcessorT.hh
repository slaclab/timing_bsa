//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef Bsa_Processor_hh
#define Bsa_Processor_hh

#include "BsaField.hh"

#include <vector>

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
    virtual const Field& field() const = 0;
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
    virtual void append() = 0;  // exception in acquisition
    virtual void append(unsigned n, 
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
    //  The PV records assigned to this array number
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
    //  Create the interface to the AmcCarrier with IP address "ip"
    //
    static Processor* create(const char* ip, bool lInit=false);
  public:
    //
    //  Update the array of PV records for one BSA buffer
    //  Return value indicates if any records were changed
    //
    virtual int update(PvArray&) = 0;
  };
};

#endif
