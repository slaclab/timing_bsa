#include <BsaDefs.hh>

#include <math.h>
 
using namespace Bsa;

BeamSelect::BeamSelect() :
  _value( (2<<16) ) {}

BeamSelect::BeamSelect(unsigned mask, bool excl) :
  _value( (excl?(1<<16):0) | (mask&0xffff) ) {}

BeamSelect::operator unsigned() const { return _value; }

RateSelect::RateSelect() :
  _value(0) {}

RateSelect::RateSelect(FixedRate frate) :
  _value( (0<<11) | (frate&0x3ff)) {}

RateSelect::RateSelect(ACRate    arate, unsigned ats) :
  _value( (1<<11) | (arate&0x7) | (ats<<3)) {}

RateSelect::RateSelect(unsigned  idx  , unsigned bit) :
  _value( (2<<11) | ((idx&0x1f)<<5) | (bit&0x1f)) {}

RateSelect::operator unsigned() const { return _value; }

 
double   ChannelData::mean() const
{
  if (fixed() && n())
    return double(raw());
  if ((data[0]&(1<<13))==0 && (data[0]&(1<<14))==0 && n())
    return double(int32_t(((data[1]&0xffff)<<16) | (data[0]>>16)))/double(n());
  return NAN;
}

double   ChannelData::rms2() const
{
  unsigned nv = n();
  
  if (nv==0) return NAN;
  if (excpt()) return NAN;
  
  if (fixed() || nv==1)
    return 0;

  double var = double((int64_t(data[2])<<16) | (data[1]>>16));
  double sum = double(int32_t(((data[1]&0xffff)<<16) | (data[0]>>16)));
  return (var - sum*sum/double(nv))/double(nv-1);
}

unsigned Entry::nchannels() const
{
  return data[0]>>16;
}

uint64_t Entry::pulseId() const
{
  uint64_t v = data[2];
  v <<= 32;
  v |= data[1];
  return v;
}
