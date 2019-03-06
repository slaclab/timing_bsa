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
}

namespace Tpr {
  class IpAddrFixup : public IYamlFixup {
  public:
    IpAddrFixup(const char* ip) : _ip(ip) {}
    ~IpAddrFixup() {}
    void operator()(YAML::Node& node) {
      writeNode(node, YAML_KEY_ipAddr, _ip);
    }
  private:
    const char* _ip;
  };

  class TprEvent {
  public:
    uint32_t  pulseId;
    uint32_t  eventCodes[8];
    uint16_t  dtype;
    uint16_t  version;
    uint32_t  dmod[6];
    uint64_t  epicsTime;
    uint32_t  edefAvgDn;
    uint32_t  edefMinor;
    uint32_t  edefMajor;
    uint32_t  edefInit;
  };

};

using namespace Tpr;

static int      count = 0;
static int64_t  bytes = 0;
static Path     core;

static void sigHandler( int signal ) 
{
  psignal(signal, "tpr_stream received signal");

  unsigned zero(0);
  IScalVal::create(core->findByName("AmcCarrierCore/TimingUdpClient[0]/ClientRemoteIp"))->setVal((uint32_t*)&zero);
  
  printf("TPR stream disabled\n");
  ::exit(signal);
}

void* countThread(void* args)
{
  int fd = *(int*)args;

  timespec tv;
  clock_gettime(CLOCK_REALTIME,&tv);
  unsigned ocount = count;
  int64_t  obytes = bytes;
  while(1) {
    send(fd, &fd, sizeof(fd), 0);
    usleep(1000000);
    timespec otv = tv;
    clock_gettime(CLOCK_REALTIME,&tv);
    unsigned ncount = count;
    int64_t  nbytes = bytes;

    double dt     = double( tv.tv_sec - otv.tv_sec) + 1.e-9*(double(tv.tv_nsec)-double(otv.tv_nsec));
    double rate   = double(ncount-ocount)/dt;
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
    
    printf("Packets %7.2f %cHz [%u]:  Size %7.2f %cBps [%lld B] (%7.2f %cB/evt)\n",
           rate  , scchar[rsc ], ncount, 
           dbytes, scchar[dbsc], (long long)nbytes, 
           tbytes, scchar[tbsc] );

    ocount = ncount;
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
  const char* endptr;

  while ( (c=getopt( argc, argv, "a:y:")) != EOF ) {
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

  ::signal( SIGINT  , sigHandler );
  ::signal( SIGABRT , sigHandler );
  ::signal( SIGKILL , sigHandler );
  ::signal( SIGSEGV , sigHandler );

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
    
  const unsigned buffsize=8*1024;
  char* buff = new char[buffsize];

  uint32_t dpid = 0, opid=0;
  do {
    ssize_t ret = read(fd,buff,buffsize);
    if (ret < 0) break;
    count++;
    bytes += ret;
    const TprEvent& ev = *reinterpret_cast<const TprEvent*>(buff);
    const uint32_t PID_MAX = 0x1ffdf;
    if ((opid < PID_MAX && ev.pulseId != (opid+1)) ||
        (opid== PID_MAX && ev.pulseId != 0))
      printf("Pulse ID jump: %x -> %x\n", opid, ev.pulseId);
    opid = ev.pulseId;
  } while(1);

  pthread_join(thr,NULL);
  free(buff);

  return 0;
}

