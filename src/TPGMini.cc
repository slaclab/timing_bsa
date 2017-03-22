#include <TPGMini.hh>

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
#define ADD_U32R(name,addr) {                                           \
    f = IIntField::create(#name, 32, false, 0, IIntField::RO);          \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_U64(name,addr) {                                            \
    f = IIntField::create(#name, 64, false, 0);                         \
    v->CMMIODevImpl::addAtAddress(f, addr);                             \
  }
#define ADD_2U(name,addr,nbits,stride) {                                \
    f = IIntField::create(#name, nbits, false, 0);                      \
    v->CMMIODevImpl::addAtAddress(f, addr, 2, stride);                  \
  }

TPGMini ITPGMini::create(const char* name) 
{
  TPGMiniImpl v = CShObj::create<TPGMiniImpl>(name);
  Field f;

  ADD_U1(TxReset     ,0x0000,0);
  ADD_U1(TxPolarity  ,0x0000,1);
  ADD_U3(TxLoopback  ,0x0000,2);
  ADD_U1(TxInhibit   ,0x0000,5);

  ADD_U16(BaseControl,0x0004);
  ADD_U64(PulseId    ,0x0008);
  ADD_U64(TStamp     ,0x0010);

  f = IIntField::create("FixedRateDiv", 32, false, 0);                         
  v->CMMIODevImpl::addAtAddress(f, 0x0018, 10, 4);                             
  ADD_U1(RateReload  ,0x0040,0);

  ADD_U8R(NBeamSeq   ,0x004c,0);
  ADD_U8R(NControlSeq,0x004d,0);
  ADD_U8R(NArraysBsa ,0x004e,0);
  ADD_U4R(SeqAddrLen ,0x004f,0);
  ADD_U4R(NAllowSeq  ,0x004f,4);

  ADD_U64(BsaComplete,0x0050);

  ADD_2U(BsaEventSel ,0x0200,32,8);
  ADD_2U(BsaStatSel  ,0x0204,32,8);

  ADD_2U(BsaStat     ,0x0400,32,4);

  ADD_U32R(PllCnt       ,0x0500);
  ADD_U32R(ClkCnt       ,0x0504);
  ADD_U32R(SyncErrCnt   ,0x0508);
  ADD_U32 (CountInterval,0x050c);
  ADD_U32R(BaseRateCount,0x0510);

  return v;
}

CTPGMiniImpl::CTPGMiniImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x1000, LE)
{
}
