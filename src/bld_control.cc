#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <signal.h>

#include <string>
#include <new>
#include <vector>

#include <cpsw_api_user.h>
#include <cpsw_yaml_keydefs.h>
#include <cpsw_yaml.h>

void usage(const char* p) {
  printf("Usage: %s [options]\n",p);
  printf("Options: -a <ip address, dotted notation>\n");
  printf("         -y <yaml file>[,<path to timing>]\n");
  printf("         -m <channel mask>\n");
  printf("         -p <words> (max packet size)\n");
  printf("         -d <packets> (dump packets and exit)\n");
  printf("         -D (disable BLD channel)\n");
}

namespace Bld {
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

  class BldEvent {
  public:
    uint64_t  timeStamp;
    uint64_t  pulseId;
    uint32_t  mask;
    uint32_t  beam;
    std::vector<uint32_t> channels;
    uint32_t  valid;
  };

  class BldEventIterator {
  public:
    BldEventIterator(const char* b, size_t sz) : 
      _buff(reinterpret_cast<const uint32_t*>(b)),
      _end (reinterpret_cast<const uint32_t*>(b+sz)),
      _next(_buff)
    { first(sz); }
  public:
    void first(size_t sz)
    {
      v.timeStamp = _buff[1]; v.timeStamp <<= 32; v.timeStamp += _buff[0];
      v.pulseId   = _buff[3]; v.pulseId   <<= 32; v.pulseId   += _buff[2];
      v.mask      = _buff[4];
      v.beam      = _buff[5];
      unsigned i=6;
      for(unsigned m=v.mask; m!=0; m&=(m-1))
        v.channels.push_back(_buff[i++]);
      v.valid     = _buff[i++];
      _next    = _buff+i;

      //
      //  Validate size of packet (sz = sizeof_first + n*sizeof_next)
      //
      unsigned sizeof_first = 4*i;
      unsigned sizeof_next  = 4*(i-4);
      if ( ((sz - sizeof_first)%sizeof_next) != 0 ) {
        printf("BldEventIterator:first() size of packet error : sz %u, first %u, next %u\n",
               sz, sizeof_first, sizeof_next);
        abort();
      }
    }
    bool next()
    {
      if (_next >= _end)
        return false;

      v.timeStamp = _buff[1]; v.timeStamp <<= 32; v.timeStamp += _buff[0];
      v.pulseId   = _buff[3]; v.pulseId   <<= 32; v.pulseId   += _buff[2];
      v.timeStamp+= ((_next[0]>> 0)&0xfffff);
      v.pulseId  += ((_next[0]>>20)&0xfff);
      v.mask      = _buff[4];
      v.beam      = _next[1];
      unsigned i=2;
      for(unsigned m=v.mask; m!=0; m&=(m-1))
        v.channels[i-2] = _next[i++];
      v.valid     = _next[i++];
      _next   += i;
      return true;
    }
    BldEvent        operator*() { return v; }
  private:
    const uint32_t* _buff;
    const uint32_t* _end;
    const uint32_t* _next;
    BldEvent v;
  };
};

using namespace Bld;

static int      count = 0;
static int      event = 0;
static int64_t  bytes = 0;
static unsigned lanes = 0;
static Path     core;

static void sigHandler( int signal ) 
{
  psignal(signal, "bld_control received signal");

  unsigned zero(0);
  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/Enable"))->setVal((uint32_t*)&zero);

  //  These are unnecessary
  // IScalVal::create(core->findByName("AmcCarrierCore/TimingUdpClient[0]/ClientRemoteIp"))->setVal((uint32_t*)&zero);
  // IScalVal::create(core->findByName("AmcCarrierCore/TimingUdpClient[0]/ClientRemotePort"))->setVal((uint32_t*)&zero);
  
  printf("BLD disabled\n");
  ::exit(signal);
}

void* countThread(void* args)
{
  int fd = *(int*)args;

  timespec tv;
  clock_gettime(CLOCK_REALTIME,&tv);
  unsigned ocount = count;
  unsigned oevent = event;
  int64_t  obytes = bytes;
  while(1) {
    //  Send a packet to open the connection
    ::send(fd, &fd, sizeof(fd), 0);

    usleep(1000000);
    timespec otv = tv;
    clock_gettime(CLOCK_REALTIME,&tv);
    unsigned ncount = count;
    unsigned nevent = event;
    int64_t  nbytes = bytes;

    double dt     = double( tv.tv_sec - otv.tv_sec) + 1.e-9*(double(tv.tv_nsec)-double(otv.tv_nsec));
    double rate   = double(ncount-ocount)/dt;
    double erate  = double(nevent-oevent)/dt;
    double dbytes = double(nbytes-obytes)/dt;
    double tbytes = dbytes/rate;
    unsigned dbsc = 0, rsc=0, ersc=0, tbsc=0;
    
    if (count < 0) break;

    static const char scchar[] = { ' ', 'k', 'M' };
    if (rate > 1.e6) {
      rsc     = 2;
      rate   *= 1.e-6;
    }
    else if (rate > 1.e3) {
      rsc     = 1;
      rate   *= 1.e-3;
    }

    if (erate > 1.e6) {
      ersc     = 2;
      erate   *= 1.e-6;
    }
    else if (erate > 1.e3) {
      ersc     = 1;
      erate   *= 1.e-3;
    }

    if (dbytes > 1.e6) {
      dbsc    = 2;
      dbytes *= 1.e-6;
    }
    else if (dbytes > 1.e3) {
      dbsc    = 1;
      dbytes *= 1.e-3;
    }
    
    if (tbytes > 1.e6) {
      tbsc    = 2;
      tbytes *= 1.e-6;
    }
    else if (tbytes > 1.e3) {
      tbsc    = 1;
      tbytes *= 1.e-3;
    }
    
    printf("Packets %7.2f %cHz [%u]:  Size %7.2f %cBps [%lld B] (%7.2f %cB/evt): Events %7.2f %cHz [%u]:  valid %02x\n", 
           rate  , scchar[rsc ], ncount, 
           dbytes, scchar[dbsc], (long long)nbytes, 
           tbytes, scchar[tbsc], 
           erate , scchar[ersc], nevent, 
           lanes);
    lanes = 0;

    ocount = ncount;
    oevent = nevent;
    obytes = nbytes;
  }
  return 0;
}

int main(int argc, char* argv[])
{
  extern char* optarg;
  char c;

  const char* ip   = "192.168.2.10";
  unsigned short port = 8197;
  const char* yaml_file = 0;
  const char* yaml_path = "mmio/AmcCarrierEmpty";
  unsigned mask = 1;
  unsigned psize(0x3c0);
  int ndump = -1;
  bool lDisable = false;
  const char* endptr;

  while ( (c=getopt( argc, argv, "a:y:m:p:d:D")) != EOF ) {
    switch(c) {
    case 'a':
      ip = optarg;
      break;
    case 'y':
      if (strchr(optarg,',')) {
        yaml_file = strtok(optarg,",");
        yaml_path = strtok(NULL,",");
      }
      else
        yaml_file = optarg;
      break;
    case 'm':
      mask = strtoul(optarg,NULL,0);
      break;
    case 'p':
      psize = strtoul(optarg,NULL,0);
      break;
    case 'd':
      ndump = strtoul(optarg,NULL,0);
      break;
    case 'D':
      lDisable = true;
      break;
    default:
      usage(argv[0]);
      return 0;
    }
  }

  if (!yaml_file) {
    usage(argv[0]);
    return 0;
  }

  IYamlFixup* fixup = new IpAddrFixup(ip);
  Path path = IPath::loadYamlFile(yaml_file,"NetIODev",0,fixup);
  delete fixup;

  core = path->findByName(yaml_path);

  struct sigaction sa;
  sa.sa_handler = sigHandler;
  sa.sa_flags = SA_RESETHAND;

  sigaction(SIGINT ,&sa,NULL);
  sigaction(SIGABRT,&sa,NULL);
  sigaction(SIGKILL,&sa,NULL);
  sigaction(SIGSEGV,&sa,NULL);

  if (lDisable)
    abort();

  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("Open socket");
    return -1;
  }

  sockaddr_in saddr;
  saddr.sin_family = PF_INET;

  if (inet_aton(ip,&saddr.sin_addr) < 0) {
    perror("Converting IP");
    return -1;
  }
    
  saddr.sin_port = htons(port);

  if (connect(fd, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
    perror("Error connecting UDP socket");
    return -1;
  }

  sockaddr_in haddr;
  socklen_t   haddr_len = sizeof(haddr);
  if (getsockname(fd, (sockaddr*)&haddr, &haddr_len) < 0) {
    perror("Error retrieving local address");
    return -1;
  }

  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_t thr;
  if (pthread_create(&thr, &tattr, &countThread, &fd)) {
    perror("Error creating read thread");
    return -1;
  }

  //  Set the target address
  // unsigned lip   = ntohl(haddr.sin_addr.s_addr);
  // unsigned lport = ntohs(haddr.sin_port);
  // IScalVal::create(core->findByName("AmcCarrierCore/TimingUdpClient[0]/ClientRemoteIp"))->setVal((uint32_t*)&lip);
  // IScalVal::create(core->findByName("AmcCarrierCore/TimingUdpClient[0]/ClientRemotePort"))->setVal((uint32_t*)&lport);

  unsigned one(-1);
  uint64_t onell(-1ULL);

  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/ChannelMask"))->setVal((uint32_t*)&mask);
  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/ChannelSevr"))->setVal((uint64_t*)&onell);
  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/PacketSize"))->setVal((uint32_t*)&psize);

  {
    uint32_t v;
    IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/PacketSize"))->getVal(&v);
    printf("PacketSize: %u words\n",v);
    IScalVal_RO::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/WordCount"))->getVal(&v);
    printf("WordCount : %u words\n",v);
  }
    
  IScalVal::create(core->findByName("AmcCarrierCore/AmcCarrierBsa/BldAxiStream/Enable"))->setVal((uint32_t*)&one);
  
  const unsigned buffsize=8*1024;
  char* buff = new char[buffsize];

  uint64_t dpid = 0, opid=0;
  do {
    ssize_t ret = read(fd,buff,buffsize);
    if (ret < 0) abort();
    if (ndump==0) abort();
    if (ndump>0) {
      ret = (ret+3)>>2;
      const unsigned *p = reinterpret_cast<const unsigned*>(buff);
      for(unsigned i=0; i<ret; i++)
        printf("%08x%c", p[i], (i%8)==7 ?'\n':' ');
      printf("\n");
      ndump--;
    }
    else {
      count++;
      bytes += ret;
      BldEventIterator it(buff,ret);
      do {
        BldEvent ev = *it;
        lanes |= ev.valid;
        event++;

        uint64_t ndpid = ev.pulseId - opid;
        if (opid!=0 && (ndpid!=dpid)) {
          printf("Delta PID change: %u -> %u\n", dpid&0xffffffff, ndpid&0xffffffff);
          dpid = ndpid;
          opid = ev.pulseId;
        }
        else
          opid = ev.pulseId;
      } while(it.next());
    }
  } while(1);

  pthread_join(thr,NULL);
  free(buff);

  return 0;
}

