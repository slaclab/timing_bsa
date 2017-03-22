#include <RamControl.hh>

using namespace Bsa;

#define ADD_U64(name,addr) {                                            \
    f = IIntField::create(#name, 64);                                   \
    v->CMMIODevImpl::addAtAddress(f, addr, nelms, 8);                   \
  }
#define ADD_U64R(name,addr) {                                            \
    f = IIntField::create(#name, 64, false, 0, IIntField::RO);          \
    v->CMMIODevImpl::addAtAddress(f, addr, nelms, 8);                   \
  }
#define ADD_U1(name,addr,lsbit) {                                       \
    f = IIntField::create(#name, 1, false, lsbit);                      \
    v->CMMIODevImpl::addAtAddress(f, addr, nelms, 4);                   \
  }

RamControl IRamControl::create(const char* name) 
{
  return IRamControl::create(name,false);
}

RamControl IRamControl::create(const char* name, bool lraw) 
{
  RamControlImpl v = CShObj::create<RamControlImpl>(name);
  Field f;

  unsigned nelms=4;
  unsigned addr =0;
  if (!lraw) {
    nelms = 64;
    ADD_U64R(TimeStamp  ,0);
    addr = 0x1000;
  }
  ADD_U64 (StartAddr  ,addr); addr += 0x200;
  ADD_U64 (EndAddr    ,addr); addr += 0x200;
  ADD_U64R(WrAddr     ,addr); addr += 0x200;
  ADD_U64R(TriggerAddr,addr); addr += 0x200;

  ADD_U1(Enabled     ,addr,0);
  ADD_U1(Mode        ,addr,1);
  ADD_U1(Init        ,addr,2);
  ADD_U1(SoftTrigger ,addr,3);

  f = IIntField::create("MsgDest", 4, false, 4);
  v->CMMIODevImpl::addAtAddress(f, addr, nelms, 4);
  f = IIntField::create("FramesAfterTrigger", 16, false, 0);
  v->CMMIODevImpl::addAtAddress(f, addr+2, nelms, 4);
  addr += 0x200;

  f = IIntField::create("Status", 32, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, addr, nelms, 4);

  ADD_U1(Empty       ,addr,0);
  ADD_U1(Full        ,addr,1);
  ADD_U1(Done        ,addr,2);
  ADD_U1(Triggered   ,addr,3);
  ADD_U1(Error       ,addr,4);

  f = IIntField::create("BurstSize", 4, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, addr+1);

  f = IIntField::create("FramesSinceTrigger", 16, false, 0, IIntField::RO);
  v->CMMIODevImpl::addAtAddress(f, addr+2, nelms, 4);
  return v;
}

CRamControlImpl::CRamControlImpl(Key& key, const char* name) : CMMIODevImpl(key, name, 0x2000, LE)
{
}
