//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#ifndef CpswTemplate_hh
#define CpswTemplate_hh

#include <cpsw_mmio_dev.h>

#define CpswTemplate(T)                                         \
  class I##T;                                                   \
  typedef shared_ptr<I##T> T;                                   \
                                                                \
  class C##T##Impl;                                             \
  typedef shared_ptr<C##T##Impl> T##Impl;                       \
                                                                \
  class I##T : public virtual IMMIODev {                        \
  public:                                                       \
  static T create(const char*);                                 \
  };                                                            \
                                                                \
  class C##T##Impl : public CMMIODevImpl, public virtual I##T { \
  public:                                                       \
  C##T##Impl(Key &k, const char *name);                         \
  };                                                            


#endif
