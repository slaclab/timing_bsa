#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/uio.h>
#include <net/if.h>
#include <string>
#include <sstream>

#include "socketAPI.h"

/*
 * Global C function definitions
 */
extern "C" 
{   

/* 
 * The following functions provide C wrappers for accesing SocketAPISpace::socketAPIInterface
 * and SocketAPISpace::socketAPIFactory
 */

/**
 * Init function: Use SocketAPISpace::socketAPIFactory to construct the socketAPI
 * and save the pointer in (*ppVoidsocketAPI)
 */ 
int socketAPIInitByInterfaceName(unsigned int uAddr, unsigned short uPort, 
    unsigned int uMaxDataSize, unsigned char ucTTL, const char* sInterfaceIp, 
    void** ppVoidsocketAPI)
{
    if ( ppVoidsocketAPI == NULL )
        return 1;
    
    SocketAPISpace::socketAPIInterface* psocketAPI = 
      SocketAPISpace::socketAPIFactory::createsocketAPI(uAddr, uPort, 
      uMaxDataSize, ucTTL, sInterfaceIp);
        
    *ppVoidsocketAPI = reinterpret_cast<void*>(psocketAPI);
    
    return 0;
}

int socketAPIInitByInterfaceAddress(unsigned int uAddr, unsigned short uPort, 
    unsigned int uMaxDataSize, unsigned char ucTTL, unsigned int uInterfaceIp, 
    void** ppVoidsocketAPI)
{
    if ( ppVoidsocketAPI == NULL )
        return 1;
    
    SocketAPISpace::socketAPIInterface* psocketAPI = 
      SocketAPISpace::socketAPIFactory::createsocketAPI(uAddr, uPort, 
      uMaxDataSize, ucTTL, uInterfaceIp);
        
    *ppVoidsocketAPI = reinterpret_cast<void*>(psocketAPI);
    
    return 0;
}

/**
 * Release function: Call C++ delete operator to delete the socketAPI
 */
int socketAPIRelease(void* pVoidsocketAPI)
{
    if ( pVoidsocketAPI == NULL )
        return 1;
        
    SocketAPISpace::socketAPIInterface* psocketAPI = 
      reinterpret_cast<SocketAPISpace::socketAPIInterface*>(pVoidsocketAPI);
    delete psocketAPI;
        
    return 0;
}

/**
 * Call the Send function defined in SocketAPISpace::socketAPIInterface 
 */
int socketAPISendRawData(void* pVoidsocketAPI, int iSizeData, char* pData)
{
    if ( pVoidsocketAPI == NULL || pData == NULL )
        return -1;

    SocketAPISpace::socketAPIInterface* psocketAPI = 
      reinterpret_cast<SocketAPISpace::socketAPIInterface*>(pVoidsocketAPI);      

    return psocketAPI->sendRawData(iSizeData, pData);
}



} // extern "C" 

using std::string;

/*
 * local class declarations
 */
namespace SocketAPISpace
{

/**
 * A Slim socket API class 
 *
 * Combination of socketAPIBasic, Client, Port and Ins
 */
class socketAPISlim : public socketAPIInterface
{
public:
    socketAPISlim(unsigned int uAddr, unsigned short uPort, unsigned int uMaxDataSize,
      unsigned char ucTTL = 32, const char* sInteraceIp = NULL);
    socketAPISlim(unsigned int uAddr, unsigned short uPort, unsigned int uMaxDataSize,
      unsigned char ucTTL = 32, unsigned int uInteraceIp = 0);
    virtual ~socketAPISlim();
    virtual int setPort(unsigned short uPort);
    virtual int setAddr(unsigned int uAddr);
    virtual int sendRawData(int iSizeData, const char* pData);
    
    // debug information control
    virtual void setDebugLevel(int iDebugLevel);
    virtual int getDebugLevel();
    
private:
    unsigned int _uAddr;
    unsigned short _uPort;
    int _iSocket;
    int _iDebugLevel;
    
    int _init( unsigned int uMaxDataSize, unsigned char ucTTL, 
      unsigned int uInterfaceIp);   
      
    static std::string addressToStr( unsigned int uAddr );      
};

} // namespace SocketAPISpace

/**
 * class member definitions
 */
namespace SocketAPISpace
{
/**
 * class socketAPIFactory
 */
socketAPIInterface* socketAPIFactory::createsocketAPI(unsigned int uAddr, 
  unsigned short uPort, unsigned int uMaxDataSize, unsigned char ucTTL, 
  const char* sInteraceIp)
{
    return new socketAPISlim(uAddr, uPort, uMaxDataSize, ucTTL, sInteraceIp );
}

socketAPIInterface* socketAPIFactory::createsocketAPI(unsigned int uAddr, 
  unsigned short uPort, unsigned int uMaxDataSize, unsigned char ucTTL, 
  unsigned int uInterfaceIp)
{
    return new socketAPISlim(uAddr, uPort, uMaxDataSize, ucTTL, uInterfaceIp );
}

/**
 * class socketAPISlim
 */
socketAPISlim::socketAPISlim(unsigned int uAddr, unsigned short uPort, 
  unsigned int uMaxDataSize, unsigned char ucTTL, const char* sInterfaceIp) : 
  _uAddr(uAddr), _uPort(uPort), _iSocket(-1), _iDebugLevel(0)
{
    unsigned int uInterfaceIp = ( 
      (sInterfaceIp == NULL || sInterfaceIp[0] == 0)?
      0 : ntohl(inet_addr(sInterfaceIp)) );
    
    _init(uMaxDataSize, ucTTL, uInterfaceIp);   
}

socketAPISlim::socketAPISlim(unsigned int uAddr, unsigned short uPort, 
  unsigned int uMaxDataSize, unsigned char ucTTL, unsigned int uInterfaceIp) : 
  _uAddr(uAddr), _uPort(uPort), _iSocket(-1), _iDebugLevel(0)
{   
    _init(uMaxDataSize, ucTTL, uInterfaceIp);
}

int socketAPISlim::_init( unsigned int uMaxDataSize, unsigned char ucTTL, 
  unsigned int uInterfaceIp)
{
    int iRetErrorCode = 0;
try
{
    /*
     * socket
     */
    _iSocket    = socket(AF_INET, SOCK_DGRAM, 0);   
    if ( _iSocket == -1 ) throw string("socketAPISlim::socketAPISlim() : socket() failed");

    /*
     * set sender buffer size
     */
    int iSendBufferSize = uMaxDataSize + sizeof(struct sockaddr_in);

#ifdef VXWORKS // Out for Tornado II 7/21/00 - RiC
    // The following was found exprimentally with ~claus/bbr/test/sizeTest
    // The rule may well change if the mBlk, clBlk or cluster parameters change
    // as defined in usrNetwork.c - RiC
    iSendBufferSize += (88 + 32 * ((parm - 1993) / 2048));
#endif

    if(
      setsockopt(_iSocket, SOL_SOCKET, SO_SNDBUF, (char*)&iSendBufferSize, sizeof(iSendBufferSize))
      == -1 )
        throw string("socketAPISlim::socketAPISlim() : setsockopt(...SO_SNDBUF) failed");
    
    /*
     * socket and bind
     */
        
    sockaddr_in sockaddrSrc;
    sockaddrSrc.sin_family      = AF_INET;
    sockaddrSrc.sin_addr.s_addr = INADDR_ANY;
    sockaddrSrc.sin_port        = 0;

    if ( 
      bind( _iSocket, (struct sockaddr*) &sockaddrSrc, sizeof(sockaddrSrc) ) 
      == -1 )
        throw string("socketAPISlim::socketAPISlim() : bind() failed");
        
    /*
     * getsockname
     */     
    sockaddr_in sockaddrName;
#ifdef __linux__
    unsigned int iLength = sizeof(sockaddrName);
#elif defined(__rtems__)
    socklen_t iLength = sizeof(sockaddrName);
#else
    int iLength = sizeof(sockaddrName);
#endif      

    if(getsockname(_iSocket, (sockaddr*)&sockaddrName, &iLength) == 0) 
    {
        unsigned int uSockAddr = ntohl(sockaddrName.sin_addr.s_addr);
        unsigned int uSockPort = (unsigned int )ntohs(sockaddrName.sin_port);
        
        if ( _iDebugLevel > 1 )
            printf( "Local addr: %s Port %u\n", addressToStr(uSockAddr).c_str(), uSockPort );
    }
    else
        throw string("socketAPISlim::socketAPISlim() : getsockname() failed");      
        
    /*
     * set multicast TTL and interface
     */     
    if ( 
      setsockopt( _iSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ucTTL, sizeof(ucTTL) ) 
      < 0 ) 
        throw string("socketAPISlim::socketAPISlim() : setsockopt(...IP_MULTICAST_TTL) failed");

    if (uInterfaceIp != 0) 
    {           
        in_addr address;
        address.s_addr = htonl(uInterfaceIp);       
        
        if ( 
          setsockopt( _iSocket, IPPROTO_IP, IP_MULTICAST_IF, (char*)&address, 
          sizeof(address) ) 
          < 0) 
            throw string("socketAPISlim::socketAPISlim() : setsockopt(...IP_MULTICAST_IF) failed");         
    }       

}
catch (string& sError)
{
    printf( "[Error] %s, errno = %d (%s)\n", sError.c_str(), errno,
      strerror(errno) );
      
    iRetErrorCode = 1;
}
    
    return iRetErrorCode;   
}

socketAPISlim::~socketAPISlim()
{
    if (_iSocket != 0)
        close(_iSocket);
}

// debug information control
void socketAPISlim::setDebugLevel(int iDebugLevel)
{
    _iDebugLevel = iDebugLevel;
}

int socketAPISlim::getDebugLevel()
{
    return _iDebugLevel;
}

int socketAPISlim::setPort(unsigned short uPort)
{
  _uPort = uPort;
  return  0;
}


int socketAPISlim::setAddr(unsigned int uAddr)
{
  _uAddr = uAddr;
  return  0;
}


int socketAPISlim::sendRawData(int iSizeData, const char* pData)
{
    int iRetErrorCode = 0;
    /*
     * sendmsg
     */     

    sockaddr_in sockaddrDst;
    sockaddrDst.sin_family      = AF_INET;
    sockaddrDst.sin_addr.s_addr = htonl(_uAddr);
    sockaddrDst.sin_port        = htons(_uPort);    
    
    struct iovec iov[2];
    iov[0].iov_base = (caddr_t)(pData);
    iov[0].iov_len  = iSizeData;
    
    struct msghdr hdr;
    hdr.msg_iovlen      = 1;        
    hdr.msg_name        = (sockaddr*) &sockaddrDst;
    hdr.msg_namelen     = sizeof(sockaddrDst);
    hdr.msg_control     = (caddr_t)0;
    hdr.msg_controllen  = 0;
    hdr.msg_iov         = &iov[0];

    unsigned int uSendFlags = 0;
try
{
    if ( 
      sendmsg(_iSocket, &hdr, uSendFlags) 
      == -1 )
        throw string("socketAPISlim::sendRawData() : sendmsg failed");                  
}
catch (string& sError)
{
    printf( "[Error] %s, size = %u, errno = %d (%s)\n", sError.c_str(), iSizeData, errno,
      strerror(errno) );
    printf( "[Error] %s, hdr.msg_iovlen = %zu, hdr.msg_iov->iov_len = %zu \n", sError.c_str(), hdr.msg_iovlen, hdr.msg_iov->iov_len );
      
    iRetErrorCode = 1;
}

    return iRetErrorCode;   
}

/*
 * private static functions
 */
string socketAPISlim::addressToStr( unsigned int uAddr )
{
    unsigned int uNetworkAddr = htonl(uAddr);
    const unsigned char* pcAddr = (const unsigned char*) &uNetworkAddr;
    std::stringstream sstream;
    sstream << 
      (int) pcAddr[0] << "." <<
      (int) pcAddr[1] << "." <<
      (int) pcAddr[2] << "." <<
      (int) pcAddr[3];
      
     return sstream.str();
}

} // namespace SocketAPISpace

extern "C"
{


int socketAPISetPort(unsigned short uPort, void* pVoidsocketAPI)
{
    if ( pVoidsocketAPI == NULL )
        return -1;

    SocketAPISpace::socketAPISlim* psocketAPI = 
      reinterpret_cast<SocketAPISpace::socketAPISlim*>(pVoidsocketAPI);      

    return psocketAPI->setPort(uPort);  
}

int socketAPISetAddr(unsigned int uAddr, void* pVoidsocketAPI)
{
    if ( pVoidsocketAPI == NULL )
        return -1;

    SocketAPISpace::socketAPISlim* psocketAPI = 
      reinterpret_cast<SocketAPISpace::socketAPISlim*>(pVoidsocketAPI);      

    return psocketAPI->setAddr(uAddr);  
}
}