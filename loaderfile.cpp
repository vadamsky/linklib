#include <boost/algorithm/string.hpp>
//#include <sstream>
#include "loaderfile.h"
#include "udpnetobjectseekerworker.h"

using namespace boost::algorithm;

LoaderFile::LoaderFile(std::string filename)
{
    ifstream in(filename.c_str());
    if( !in )
    {
       cout << "Can't open file \"" << filename << '\"' << endl;
       return;
    }

    vector< string > lns;

    std::string s;
    while( !in.eof() )
    {
       std::getline( in, s );
       lns.push_back( s/*+='\n'*/ );
    }

    std::string text;
    std::string temp;
    long p1, p2, p3, p4;
    // first string
    temp = lns[0];
    receive_seekport = atoi(temp.substr(0,temp.find(' ')).c_str());
    temp = temp.substr(temp.find(' '), temp.size() - temp.find(' '));
    trim(temp);
    receive_shortport = atoi(temp.c_str());
    // second string
    temp = lns[1];
    multicastaddress = temp.substr( 0, temp.find(';') );
    trim(multicastaddress);
    getportsfromstring(p1, p2, p3, p4, temp);
    for(long p=p1; p<=p2; p++){
        PAddr pa(multicastaddress, p);
        multicastSeekToSend.push_back(pa);
    }
    for(long p=p3; p<=p4; p++){
        PAddr pa(multicastaddress, p);
        multicastShortToSend.push_back(pa);
    }
    // other strings
    for(unsigned int i=2; i<lns.size(); i++){
        text = lns[i];
        // IF ONE ADDRESS IN STRING
        if(text.find('-')==std::string::npos){
            std::string addr = text.substr(0, text.size()-text.find(';'));
            trim(addr);
            if(text.find('.')!=std::string::npos || text.find(':')!=std::string::npos){
                ////addressesToSend.push_back( text );
                getportsfromstring(p1, p2, p3, p4, text);
                for(long p=p1; p<=p2; p++){
                    PAddr pa(addr, p);
                    seekToSend.push_back(pa);
                }
                for(long p=p3; p<=p4; p++){
                    PAddr pa(addr, p);
                    shortToSend.push_back(pa);
                }
            }
        }
        // IF SEVERAL ADDRESSES IN STRING
        else{
            string addr1 = text.substr(0, text.find('-'));
            string addr2 = text.substr(text.find('-')+1, text.size()-1-text.find('-'));
            trim( addr1 );
            trim( addr2 );
            short int decimals[8];
            short int decimals1[8];
            short int decimals2[8];
            char ctemp[40];
            //string temp;
            //addr
            // ipv4 or ipv6
            if(addr1.find('.')!=std::string::npos){ // ipv4
                temp = addr1;
                decimals1[0]=atoi(temp.substr(0,temp.find('.')).c_str()); temp = temp.substr(temp.find('.')+1, temp.size()-1-temp.find('.'));
                decimals1[1]=atoi(temp.substr(0,temp.find('.')).c_str()); temp = temp.substr(temp.find('.')+1, temp.size()-1-temp.find('.'));
                decimals1[2]=atoi(temp.substr(0,temp.find('.')).c_str()); temp = temp.substr(temp.find('.')+1, temp.size()-1-temp.find('.'));
                decimals1[3]=atoi(temp.c_str());
                temp = addr2;
                decimals2[0]=atoi(temp.substr(0,temp.find('.')).c_str()); temp = temp.substr(temp.find('.')+1, temp.size()-1-temp.find('.'));
                decimals2[1]=atoi(temp.substr(0,temp.find('.')).c_str()); temp = temp.substr(temp.find('.')+1, temp.size()-1-temp.find('.'));
                decimals2[2]=atoi(temp.substr(0,temp.find('.')).c_str()); temp = temp.substr(temp.find('.')+1, temp.size()-1-temp.find('.'));
                decimals2[3]=atoi(temp.c_str());
                // level=0 => addr1=addr2. Next - by digits
                short int level = decimals1[3]==decimals2[3] ? 0 : decimals1[2]==decimals2[2] ? 1 : decimals1[1]==decimals2[1] ? 2 : decimals1[0]==decimals2[0] ? 3 : 4 ;
                level = level;
                decimals[0] = decimals1[0]-1;
                do{
                    decimals[0]++;
                    decimals[1] = decimals1[1]-1;
                    do{
                        decimals[1]++;
                        decimals[2] = decimals1[2]-1;
                        do{
                            decimals[2]++;
                            decimals[3] = decimals1[3]-1;
                            do{
                                decimals[3]++;
                                // ADDR + PORTS
                                sprintf(ctemp, "%u.%u.%u.%u", decimals[0], decimals[1], decimals[2], decimals[3]);
                                std::string addr = string(ctemp);
                                getportsfromstring(p1, p2, p3, p4, text);
                                for(long p=p1; p<=p2; p++){
                                    PAddr pa(addr, p);
                                    seekToSend.push_back(pa);
                                }
                                for(long p=p3; p<=p4; p++){
                                    PAddr pa(addr, p);
                                    shortToSend.push_back(pa);
                                }
                                //addressesToSend.push_back(string(ctemp));
                                if(decimals[3]>254) decimals[3] = decimals2[3];
                            }while(decimals[3]!=decimals2[3]);
                            if(decimals[2]>254) decimals[2] = decimals2[2];
                        }while(decimals[2]!=decimals2[2]);
                        if(decimals[1]>254) decimals[1] = decimals2[1];
                    }while(decimals[1]!=decimals2[1]);
                    if(decimals[0]>254) decimals[0] = decimals2[0];
                }while(decimals[0]!=decimals2[0]);
            }
            else{ // ipv6

            }
        }
    }
}

void LoaderFile::getportsfromstring(long & seekp1, long & seekp2, long & shortp1, long & shortp2, std::string str)
{
    seekp1 = 0; seekp2 = 0;
    shortp1= 0; shortp2= 0;
    int i1 = str.find(';');
    int i2 = str.find(',');
    std::string temp;
        str = str.substr( i1+1, str.size()-1-i1 );
        temp = str.substr( 0, i2-i1-1 );
        trim(temp);
    seekp1 = atoi(temp.c_str());
        str = str.substr( i2-i1, str.size()-i2+i1 );
        i1 = str.find(';');
        temp = str.substr( 0, i1 );
        trim(temp);
    seekp2 = atoi(temp.c_str());
        str = str.substr( i1+1, str.size()-1-i1 );
        i2 = str.find(',');
        temp = str.substr( 0, i2 );
        trim(temp);
    shortp1 = atoi(temp.c_str());
        str = str.substr( i2+1, str.size()-1-i2 );
        temp = str;
        trim(temp);
    shortp2 = atoi(temp.c_str());
}

std::string LoaderFile::getMulticastAddress()
{
    return multicastSeekToSend.at(0).addr;
}
