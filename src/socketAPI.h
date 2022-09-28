#ifndef MULTICAST_BLD_LIB_H
#define MULTICAST_BLD_LIB_H

namespace SocketAPISpace
{   
/**
 * Abastract Interface of Bld Multicast Client 
 * 
 * The interface class for providing Bld Mutlicast Client functions.
 *
 * Design Issue:
 * 1. The value semantics are disabled. 
 */
class socketAPIInterface
{
public:
    /**
     * Send raw data out to the Bld Server
     *
     * @param iSizeData  size of data (in bytes)
     * @param pData      pointer to the data buffer (char[] buffer)
     * @return  0 if successful,  otherwise the "errno" code (see <errno.h>)
     */
    virtual int sendRawData(int iSizeData, const char* pData) = 0;
    
    // debug information control
    virtual void setDebugLevel(int iDebugLevel) = 0;
    virtual int getDebugLevel() = 0;
    
    virtual ~socketAPIInterface() {} /// polymorphism support
protected:  
    socketAPIInterface() {} /// To be called from implementation classes
private:
    ///  Disable value semantics. No definitions (function bodies).
    socketAPIInterface(const socketAPIInterface&);
    socketAPIInterface& operator=(const socketAPIInterface&);
};

/**
 * Factory class of Bld Multicast Client 
 *
 * Design Issue:
 * 1. Object semantics are disabled. Only static utility functions are provided.
 * 2. Later if more factorie classes are needed, this class can have public 
 *    constructor, virtual destructor and virtual member functions.
 */
class socketAPIFactory
{
public:
    /**
     * Create a Bld Client object
     *
     * @param uAddr         mutlicast address. Usually with 239.0.0.1 ~ 239.255.255.255
     * @param uPort         UDP port
     * @param uMaxDataSize  Maximum Bld data size. Better to be less than MTU.
     * @param ucTTL         TTL value in UDP packet. Ideal value is 1 + (# of middle routers)
     * @param sInteraceIp   Specify the NIC by IP address (in c string format)
     * @return              The created Bld Client object
     */
    static socketAPIInterface* createsocketAPI(unsigned int uAddr, 
      unsigned short uPort, unsigned int uMaxDataSize, unsigned char ucTTL = 32, 
      const char* sInteraceIp = 0);
        
    /**
     * Create a Bld Client object
     *
     * Overloaded version
     * @param uInteraceIp   Specify the NIC by IP address (in unsigned int format)
     */
    static socketAPIInterface* createsocketAPI(unsigned int uAddr, 
      unsigned short uPort, unsigned int uMaxDataSize, unsigned char ucTTL = 32, 
      unsigned int uInteraceIp = 0);
private:
    /// Disable object instantiation (No object semantics).
    socketAPIFactory();
};

} // namespace SocketAPISpace

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
  void** ppVoidsocketAPI);
int socketAPIInitByInterfaceAddress(unsigned int uAddr, unsigned short uPort, 
  unsigned int uMaxDataSize, unsigned char ucTTL, unsigned int uInterfaceIp, 
  void** ppVoidsocketAPI);  

/**
 * @brief Updates the BLD port
 * 
 * @param uPort Port
 * @param ppVoidsocketAPI : pointer to context
 * @return int 
 */
int socketAPISetPort(unsigned short uPort, void* pVoidsocketAPI);  

/**
 * @brief Updates the BLD destination IP
 * 
 * @param uAddr IP address
 * @param ppVoidsocketAPI : pointer to context
 * @return int 
 */
int socketAPISetAddr(unsigned int uAddr, void* pVoidsocketAPI); 

/**
 * Release function: Call C++ delete operator to delete the socketAPI
 */
int socketAPIRelease(void* pVoidsocketAPI); 

/**
 * Call the Send function defined in SocketAPISpace::socketAPIInterface 
 */
int socketAPISendRawData(void* pVoidsocketAPI, int iSizeData, char* pData);

} // extern "C"


#endif 
