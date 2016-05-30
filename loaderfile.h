#ifndef LOADERFILE_H
#define LOADERFILE_H

#include <fstream>
#include <functional>
#include <numeric>
#include <list>
#include <string>
#include <vector>
#include <boost/asio.hpp>

using namespace std;
using namespace boost::asio;

// File format:
// first string - "[receive seekport] [receive shortport]"
// second string - "multicast address ; [seekport1],[seekport2] ; [shortport1],[shortport2]"
// other strings - addresses or areas of addresses in view as "[addr1]-[addr2] ; [seekport1],[seekport2] ; [shortport1],[shortport2]"
// second and other strings - datas to send

struct PAddr;

class LoaderFile
{
public:
    LoaderFile(std::string filename);

    std::string getMulticastAddress();
//private:
    void getportsfromstring(long & seekp1, long & seekp2, long & shortp1, long & shortp2, std::string str);
    std::string multicastaddress;
    long receive_seekport;
    long receive_shortport;
    std::vector<PAddr> multicastSeekToSend;
    std::vector<PAddr> multicastShortToSend;
    std::vector<PAddr> seekToSend;
    std::vector<PAddr> shortToSend;
    //boost::asio::ip::address addr;
};

#endif // LOADERFILE_H
