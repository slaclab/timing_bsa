//
//  Utility to control BSA
//
//  Record is 128 bytes length:
//    PulseID     :   8 bytes
//    NWords      :   4 bytes
//    ?           :   4 bytes
//    Sensor data : 112 bytes
//
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <semaphore.h>
#include <new>
#include <arpa/inet.h>
#include <sstream>
#include <signal.h>

#include <BsaField.hh>
#include <Processor.hh>
#include <AmcCarrier.hh>

#include <cpsw_api_user.h>
#include <cpsw_yaml_keydefs.h>
#include <cpsw_yaml.h>

#define DBUG

static void sigHandler( int signal ) {
  Bsa::AmcCarrier::instance()->dump();
  ::exit(signal);
}

template <class T>
class CircularBuffer {
public:
  CircularBuffer(int n) :
    _next (0),
    _nset (0),
    _nelem(n),
    _v    (2*n)
  {}
public:
  void clear() { _nset=0; }
  void append(const T& v) {
    _v[_next] = _v[_next+_nelem] = v;
    _next++;
    if (_next == _nelem)
      _next = 0;
    if (_nset < _nelem)
      _nset++;
  }
public:
  size_t size() const { return _nset; }
  const T& operator[](unsigned i) const  { return _v[_nelem+_next-_nset+i]; }
private:
  unsigned    _next;
  unsigned    _nset;
  unsigned    _nelem;
  std::vector<T> _v;
};

class PVData {
public:
  PVData() :
    n(0), mean(0), rms2(0) {}
  PVData(unsigned k,
         double   m,
         double   r) :
    n(k), mean(m), rms2(r) {}
public:
  uint64_t n;
  double   mean;
  double   rms2;
};

//=================================================================
//  This is just for the text dump application
//  An EPICS implementation should replace this
//
static const CircularBuffer<uint64_t>* __pidGlobal = 0;

class TextPv : public Bsa::Pv {
public:
  TextPv(unsigned f,
         unsigned a) : 
    _a    (a), 
    _v    (a < 60 ? 20000 : 1000000)
  {
    std::ostringstream o;
    o << "Field_" << f;
    _f = o.str();
  }
  void clear() 
  { _v.clear();
    printf("clear Array[%02u]:%s\n", _a, _f.c_str()); 
  }
  void setTimestamp(unsigned sec, 
                    unsigned nsec)
  {
    _ts_sec = sec;
    _ts_nsec = nsec;
  }
  void append(unsigned n,
              double   mean,
              double   rms2)
  { _v.append(PVData(n,mean,rms2)); }
  void flush() {
    const CircularBuffer<uint64_t>& pid = *__pidGlobal;

    uint64_t pid0 = pid[0];
    uint64_t pidl = pid[1];
    uint64_t dpid = pidl-pid0;

    unsigned nerr=0;

#if 0
    printf("-- Array[%02u]:%20.20s [%u.%09u] [%u] [%016llx,%llu]--\n",
           _a, _f.c_str(), _ts_sec, _ts_nsec, pid.size(), pid0, dpid);
#endif

    for(unsigned i=2; i<pid.size(); i++) {
      uint64_t next = pidl+dpid;
      if (next != pid[i]) {
        printf("***** PID[%d] = %016llx [%016llx] [%lld %lld]\n",
               i,pid[i],next,int64_t(pid[i]-next),dpid);
        if (nerr++ > 4)
          break;
      }
      //      pidl = next;
      pidl = pid[i];
    }
    //    return;

    if (false)
    {
      unsigned n = _v.size() < 10 ? _v.size() : 10;
      unsigned n0 = _v.size() - n;
      for(unsigned i=n0; i<_v.size(); i++) {
        const PVData& v = _v[i];
        printf("[%u] %lu:%f:%f\n", i, v.n, v.mean, v.rms2);
      }
    }
  }
private:
  unsigned    _a;
  std::string _f;
  unsigned    _ts_sec;
  unsigned    _ts_nsec;
  CircularBuffer<PVData> _v;
};

class TextPvArray : public Bsa::PvArray {
public:
  TextPvArray(unsigned                array,
              const std::vector<Bsa::Pv*>& pvs) :
    _array(array),
    _pid  (array<60 ? 20000 : 1000000),
    _pvs  (pvs)
  {
  }
public:
  const CircularBuffer<uint64_t>& pid() const { return _pid; }
  unsigned array() const { return _array; }
  void     reset(unsigned sec,
                 unsigned nsec) {
    _pid.clear();
    _ts_sec = sec;
    _ts_nsec = nsec;
    for(unsigned i=0; i<_pvs.size(); i++) {
      _pvs[i]->clear();
      _pvs[i]->setTimestamp(sec,nsec);
    }
  }
  void     set(unsigned sec,
               unsigned nsec) {
    _ts_sec = sec;
    _ts_nsec = nsec;
    for(unsigned i=0; i<_pvs.size(); i++) {
      _pvs[i]->setTimestamp(sec,nsec);
    }
  }
  void     append(uint64_t pulseId) { _pid.append(pulseId); }
  std::vector<Bsa::Pv*> pvs() { return std::vector<Bsa::Pv*>(_pvs); }
  void flush() {
    unsigned n = _pid.size() < 50 ? _pid.size() : 50;
    printf("-- %20.20s [%u.%09u] [%u] --\n", "PulseID", _ts_sec, _ts_nsec, _pid.size());
    for(unsigned i=0; i<n; i++)
      printf("%016llx\n", _pid[i]);
  }
private:
  unsigned _array;
  unsigned _ts_sec;
  unsigned _ts_nsec;
  CircularBuffer<uint64_t> _pid;
  std::vector<Bsa::Pv*> _pvs;
};

class IpAddrFixup : public IYamlFixup {
public:
  IpAddrFixup(const char* ip) : _ip(ip) {}
  ~IpAddrFixup() {}
  void operator()(YAML::Node& node, YAML::Node& dummy) {
    writeNode(node, YAML_KEY_ipAddr, _ip);
  }
private:
  const char* _ip;
};

//=======================================================

static void show_usage(const char* p)
{
  printf("** Fetch BSA data **\n");
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <IP address dotted notation> : set carrier IP\n");
  printf("         -y <yaml file, regpath, rampath>: use yaml file\n");
  printf("         -F <array>                      : force fetch of BSA array\n");
  printf("         -I <update interval>            : retries updates\n");
  printf("         -D                              : debug\n");
}

int main(int argc, char* argv[])
{
  const char* ip = "192.168.2.10";
  const char* yaml = 0;
  const char* reg_path = "mmio/AmcCarrierCore/AmcCarrierBsa";
  const char* ram_path = "strm/AmcCarrierDRAM/dram";
  uint64_t array=-1ULL;
  unsigned uinterval=1000000;
  bool     lInit=false;
  bool     lDebug=false;
  unsigned fields=(1<<1);
  int c;
  while( (c=getopt(argc,argv,"a:f:y:iF:I:D"))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg; break;
    case 'f':
      fields = strtoul(optarg,NULL,0);
      break;
    case 'i':
      lInit = true;
      break;
    case 'y':
      yaml = strtok(optarg,",");
      { const char* reg_p = strtok(NULL,",");
        const char* ram_p = strtok(NULL,",");
        if (reg_p) reg_path = reg_p;
        if (ram_p) ram_path = ram_p;
      }
      break;
    case 'F':
      array = strtoull(optarg,NULL,0);
      break;
    case 'I':
      uinterval = unsigned(1.e6*strtod(optarg,NULL));
      break;
    case 'D':
      lDebug = true;
      break;
    default:
      show_usage(argv[0]);
      exit(1);
    }
  }

  std::vector<TextPvArray*> pva;

  for(unsigned a = 0; a<64; a++) {
    if (((1ULL<<a)&array)==0) continue;

    //  We only need this vector for the "dump" function call
    std::vector<Bsa::Pv*> pvs;
    for(unsigned i=0; i<31; i++)
      if ((1<<i)&fields)
        pvs.push_back(new TextPv(i, a));
  
    pva.push_back(new TextPvArray(a, pvs));
  }

  Bsa::Processor* p;
  if (yaml) {
    IYamlFixup* fixup = new IpAddrFixup(ip);
    Path path = IPath::loadYamlFile(yaml,"NetIODev",0,fixup);

    Path path_reg(path->findByName(reg_path));
    Path path_ram(path->findByName(ram_path));
    p = Bsa::Processor::create(path_reg,
                               path_ram,
                               lInit);
  }
  else {
    p = Bsa::Processor::create(ip,lInit,lDebug);
  }

  //  ::signal( SIGINT, sigHandler );

  //
  //  We simulate periodic update requests here
  //
  while(1) {

#ifdef DBUG
    timespec ts_begin,ts_end;
    clock_gettime(CLOCK_REALTIME,&ts_begin);
#endif
    uint64_t pending = p->pending();

    for(unsigned a=0; a<pva.size(); a++) {
      if (!(pending&(1ULL<<pva[a]->array())))
        continue;

      if (p->update(*pva[a])) {
        const std::vector<Bsa::Pv*>& pvs = pva[a]->pvs();
        __pidGlobal = &pva[a]->pid();
        for(unsigned i=0; i<pvs.size(); i++)
          pvs[i]->flush();
      }
      else {
      }
    }
#ifdef DBUG
    clock_gettime(CLOCK_REALTIME,&ts_end);
    printf("\rpending [%016llx] array update = %f sec",
           pending,
           double(ts_end.tv_sec-ts_begin.tv_sec)+
           1.e-9*(double(ts_end.tv_nsec)-double(ts_begin.tv_nsec)));
    fflush(stdout);
#endif

    usleep(uinterval);
  }

  return 1;
}
