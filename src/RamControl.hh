//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
//
//  Builder for the RamControl module registers
//
#ifndef RamControl_hh
#define RamControl_hh

#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>

namespace Bsa {
  class IRamControl;
  typedef shared_ptr<IRamControl> RamControl;

  class CRamControlImpl;
  typedef shared_ptr<CRamControlImpl> RamControlImpl;

  class IRamControl : public virtual IMMIODev {
  public:
    static RamControl create(const char*);
    static RamControl create(const char*, bool lraw);
  };

  class CRamControlImpl : public CMMIODevImpl, public virtual IRamControl {
  public:
    CRamControlImpl(Key &k, const char* name);
  };
};

#endif
