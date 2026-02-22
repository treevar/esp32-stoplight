//Copyright 2026 Treevar
//All Rights Reserved
#ifndef DNS_SERVER_H
#define DNS_SERVER_H
#include <cstdint>
#include <AsyncUDP.h>
//Simple DNS server that only replies to A record queries
//Always in captive mode and replies with a fixed IP
//Specific domains can be set to return NXDOMAIN
class DNSServer {
    public:
        enum OpCode : uint8_t{
            QUERY = 0,
            IQUERY = 1, //Inverse
            STATUS = 2 //Server status
        };
        enum ResCode : uint8_t{
            NOERROR = 0,
            FORMERR = 1, //Format
            SERVFAIL = 2, //Server
            NXDOMAIN = 3, //Non-existant domain
            NOTIMP = 4, //Not implemented
            REFUSED = 5 //Query refused
        };
        enum Type : uint16_t{
            A = 1, //Address
            NS = 2, //Name server
            CNAME = 5, //Canonical name
            SOA = 6, //Start of authority
            PTR = 12, //Pointer
            MX = 15, //Mail exchange
            TXT = 16, //Text
            AAAA = 28, //Address (IPv6)
            ANY = 255
        };
        enum Class : uint16_t{
            IN = 1,
            CS = 2,
            CH = 3,
            HS = 4,
            NONE = 254,
            ANY = 255
        };
        struct Flags{
            uint16_t rd : 1; //Recursion desired
            uint16_t tc : 1; //Truncation
            uint16_t aa : 1; //Authoritative answer
            uint16_t op : 4; //Opcode
            uint16_t qr : 1; //Query
            
            uint16_t rCode : 4; //Response code
            uint16_t cd : 1; //Checking disabled
            uint16_t ad : 1; //Authentic data
            uint16_t z : 1; //Zero (reserved)
            uint16_t ra : 1; //Recursion available
        };
        struct __attribute__((packed)) Header{
            uint16_t id;
            Flags flags;
            uint16_t qCnt; //Question count
            uint16_t aCnt; //Answer count
            uint16_t authRRCnt; //Authoritative resource records
            uint16_t addRRCnt; //Additional resource records
        };
        struct Question{
            uint8_t *name;
            uint16_t type;
            uint16_t classCode;
        };
        struct Record{
            Question question; //Question we are replying to
            uint32_t ttl;
            uint16_t dataLength;
            uint8_t data[4]; //Answer
        };
        const uint8_t MAX_NX_DOMAINS = 4;
        static const uint8_t DNS_HEADER_SIZE = 12;
        static const uint8_t POINTER_BYTE = 0xc0;
        static const uint8_t ANSWER_SIZE = 16;
        DNSServer(IPAddress ip, uint16_t port = 53) : _port(port), _ip(ip) {
            for(uint8_t i = 0; i < 4; ++i){
                _defaultResponse.data[i] = _ip[i];
            }
        }
        void addNXDomain(const String& domain){
            if(_nxIdx >= MAX_NX_DOMAINS){ return; }
            _nxDomains[_nxIdx++] = _toName(domain);
        }
        bool start(){
            if(!_udp.listen(_port)){ return false; }
            _udp.onPacket([this](AsyncUDPPacket &packet){
                this->_handlePacket(packet);
            });
            return true;
        }
        void stop(){ _udp.close(); }
    private:
        AsyncUDP _udp;
        uint16_t _port;
        IPAddress _ip;
        String _nxDomains[MAX_NX_DOMAINS];
        Record _defaultResponse = {
            .question = {
                .name = nullptr,
                .type = htons(Type::A),
                .classCode = htons(Class::IN)
            },
            .ttl = htonl(7200),
            .dataLength = htons(4),
            .data = {0, 0, 0, 0}
        };
        uint8_t _nxIdx = 0;
        void _reply(AsyncUDPPacket &packet, const uint8_t nameLength, Header &header, ResCode resCode = ResCode::NOERROR){
            header.qCnt = htons(1);
            header.aCnt = resCode == ResCode::NOERROR ? htons(1) : 0;
            header.flags.qr = 1; //Response
            header.flags.aa = 1; //Authoritative answer
            header.flags.ra = 1; //Recursion available
            header.flags.rCode = resCode;
            uint8_t questionLength = nameLength + 4; //Name + Type + Class
            uint16_t resLength = DNS_HEADER_SIZE + questionLength + (resCode == ResCode::NOERROR ? ANSWER_SIZE : 0); //Header + Question + Answer(if any)
            uint8_t *response = new uint8_t[resLength]; 
            //Header
            memcpy(response, &header, DNS_HEADER_SIZE);
            uint8_t* idx = response + DNS_HEADER_SIZE;
            //Question
            memcpy(idx, packet.data()+DNS_HEADER_SIZE, questionLength);
            idx += questionLength;
            //Answer
            if(resCode == ResCode::NOERROR){
                //Name pointer
                *idx = POINTER_BYTE;
                ++idx;
                *idx = DNS_HEADER_SIZE; //Pointer offset
                ++idx;
                memcpy(idx, &_defaultResponse.question.type, 2);
                idx += 2;
                memcpy(idx, &_defaultResponse.question.classCode, 2);
                idx += 2;
                memcpy(idx, &_defaultResponse.ttl, 4);
                idx += 4;
                memcpy(idx, &_defaultResponse.dataLength, 2);
                idx += 2;
                memcpy(idx, _defaultResponse.data, 4);
            }
            packet.reply(response, resLength);
            delete[] response;
        }
        void _handlePacket(AsyncUDPPacket &packet){
            if(packet.length() < DNS_HEADER_SIZE){ return; } 

            Header header;
            memcpy(&header, packet.data(), DNS_HEADER_SIZE);
            header.qCnt = ntohs(header.qCnt);
            if(header.flags.qr == 1 || header.qCnt != 1 || header.flags.op != QUERY){ return; }

            bool nx = false; //Whether to reply with NXDOMAIN
            int nameLength = 0;
            uint8_t* nameEnd = nullptr;
            Question question;
            if(memchr(packet.data()+DNS_HEADER_SIZE, POINTER_BYTE, packet.length()-DNS_HEADER_SIZE) != nullptr){ //Compressed
                nx = true;
                nameEnd = packet.data()+DNS_HEADER_SIZE+2;
                nameLength = 2; //Pointer length
            }
            else{ //Not compressed
                nameEnd = (uint8_t*)memchr(packet.data()+DNS_HEADER_SIZE, '\0', packet.length()-DNS_HEADER_SIZE);
                if(nameEnd == nullptr){ //No null terminator
                    nx = true;
                    nameLength = packet.length() - DNS_HEADER_SIZE;
                }
            }
            //Ensure the rest of the question exists
            if((packet.data() + packet.length()) - nameEnd < 4){ 
                return; //Cant reply because we dont have full question
            }
            if(!nx){ //No errors so far
                question.name = packet.data()+DNS_HEADER_SIZE; //Start of null terminated string

                memcpy(&question.type, nameEnd+1, 2);
                question.type = ntohs(question.type);

                memcpy(&question.classCode, nameEnd+3, 2);
                question.classCode = ntohs(question.classCode);

                if((question.type != Type::A && question.type != Type::ANY) || (question.classCode != Class::IN && question.classCode != Class::ANY)){ nx = true; }
                else{
                    nameLength = nameEnd - (packet.data() + DNS_HEADER_SIZE) + 1; //+1 to include nameEnd in count
                    for(uint8_t i = 0; i < _nxIdx; ++i){
                        if(_nxDomains[i].length() != nameLength){ continue; }
                        if(memcmp(packet.data()+DNS_HEADER_SIZE, _nxDomains[i].c_str(), nameLength) == 0){
                            nx = true;
                            break;
                        }
                    }
                }
            }
            _reply(packet, nameLength, header, nx ? ResCode::NXDOMAIN : ResCode::NOERROR);
        }
        String _toName(const String& name){
            String out{};
            out.reserve(255); //max length for name
            uint8_t cnt = 0;
            int dotIdx = -1;
            int i = 0;
            for(; i < name.length(); ++i){
                if(name[i] == '.'){
                    if(cnt > 63){ return String(); }//Over length
                    out += (char)cnt;
                    out += name.substring(dotIdx+1, i);
                    dotIdx = i;
                    cnt = 0;
                }
                else{
                    ++cnt;
                }
            }
            out += (char)cnt;
            out += name.substring(dotIdx+1, i);
            out += '\0'; //Null terminator
            return out;
        }
};
#endif //DNS_SERVER_H