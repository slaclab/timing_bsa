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
#include <time.h>

#include <cpsw_api_builder.h>
#include <cpsw_api_user.h>
#include <cpsw_yaml.h>
#include <cpsw_preproc.h>

#include <yaml-cpp/yaml.h>

using std::string;

class IYamlSetIP : public IYamlFixup {
public:
	IYamlSetIP( const char* ip_addr ) : ip_addr_(ip_addr) {}
	virtual void operator()(YAML::Node &node) 
	{
          node["ipAddr"] = ip_addr_;
	}

	virtual ~IYamlSetIP() {}
private:
	std::string ip_addr_;
};

void show_usage(const char* p)
{
  printf("Usage: %s -f <yaml file> -r <reg name> [-a <ip addr>]\n",p);
}

int main(int argc, char* argv[])
{
  const char* ip=0;
  const char* filename=0;
  const char* reg=0;

  char* endPtr;
  int c;
  while( (c=getopt(argc,argv,"a:f:r:"))!=-1 ) {
    switch(c) {
    case 'a':
      ip = optarg; break;
    case 'f':
      filename=optarg; break;
    case 'r':
      reg = optarg;
      break;
    default:
      show_usage(argv[0]);
      exit(1);
    }
  }

  if (!filename || !reg) {
    show_usage(argv[0]);
    exit(1);
  }

  Path p;
  try {
    if (ip) {
      IYamlSetIP setIP(ip);
      p = IPath::loadYamlFile( filename, "NetIODev", NULL, &setIP );
    } else {
      p = IPath::loadYamlFile( filename, "NetIODev" );
    }

    uint32_t val=0xffffffff;
    ScalVal_RO s = IScalVal_RO::create( p->findByName(reg) );
    s->getVal(&val);

    printf("Read %s [%x]\n",reg,val);
   } catch ( string error ) {
    printf("Caught Error: \n  %s\n",error.c_str());
   } 
   catch (CPSWError &e){
      printf("CPSWError: %s\n", e.getInfo().c_str());
   }

  return 0;
}
