//////////////////////////////////////////////////////////////////////////////
// This file is part of 'timing_bsa'.
// It is subject to the license terms in the LICENSE.txt file found in the 
// top-level directory of this distribution and at: 
//    https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. 
// No part of 'timing_bsa', including this file, 
// may be copied, modified, propagated, or distributed except according to 
// the terms contained in the LICENSE.txt file.
//////////////////////////////////////////////////////////////////////////////
#include <TPG.hh>

using namespace Bsa;

#define ADD_U1(name,addr,lsbit) {                                       \
    f = IIntField::create(#name, 1, false, lsbit);                      \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U3(name,addr,lsbit) {                                       \
    f = IIntField::create(#name, 3, false, lsbit);                      \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U4R(name,addr,lsbit) {                                      \
    f = IIntField::create(#name, 4, false, lsbit, IIntField::RO);       \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U8R(name,addr,lsbit) {                                      \
    f = IIntField::create(#name, 8, false, lsbit, IIntField::RO);       \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U16(name,addr) {                                            \
    f = IIntField::create(#name, 16, false, 0);                         \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U32(name,addr) {                                            \
    f = IIntField::create(#name, 32, false, 0);                         \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U64(name,addr) {                                            \
    f = IIntField::create(#name, 64, false, 0);                         \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U32R(name,addr) {                                           \
    f = IIntField::create(#name, 32, false, 0, IIntField::RO);          \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_64U(name,addr,nbits,lsbit) {                                \
    f = IIntField::create(#name, nbits, false, lsbit);                  \
    v->CMMIODevImpl::addAtAddress(f, addr, 64, 8);                     \
  }

TPG ITPG::create(const char* name) 
{
  TPGImpl v = CShObj::create<TPGImpl>(name);
  Field f;

  ADD_U8R(NBeamSeq   ,0x0000,0);
  ADD_U8R(NControlSeq,0x0001,0);
  ADD_U8R(NArraysBsa ,0x0002,0);
  ADD_U4R(SeqAddrLen ,0x0003,0);
  ADD_U4R(NAllowSeq  ,0x0003,4);

  ADD_U16(ClockPeriod,0x0004);
  ADD_U32(BaseControl,0x0008);
  ADD_U64(PulseId    ,0x0010);
  ADD_U64(TStamp     ,0x0018);

  f = IIntField::create("FixedRateDiv", 32, false, 0);                         
  v->CMMIODevImpl::addAtAddress(f, 0x0040, 10, 4);                             
  ADD_U1(RateReload  ,0x0068,0);

  ADD_U64(BsaComplete,0x01f8);

  ADD_64U(BsaEventSel ,0x0200,32,0);
  ADD_64U(BsaStatSel  ,0x0204,32,0);

  ADD_U32R(PllCnt     ,0x0100);
  ADD_U32R(ClkCnt     ,0x0104);
  ADD_U32R(SyncErrCnt ,0x0108);
  ADD_U32 (CountInterval,0x010c);
  ADD_U32R(BaseRateCount,0x0110);

  ADD_64U(BsaStat     ,0x0400,32,0);

  return v;
}

CTPGImpl::CTPGImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x1000, LE)
{
}
