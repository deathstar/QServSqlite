#include "QServ.h"
#include "../GeoIP/libGeoIP/GeoIPCity.h"
#include "../GeoIP/libGeoIP/GeoIP.h"
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <netdb.h>
#endif
#include "HTTPRequest.hpp"
#include <../sqlite/sqlite3.h>

//includes geoip handling, command system and a lot of useful tools
std::recursive_mutex server::QServ::qserv_mutex;
namespace server {
    clientinfo QServ::m_lastCI;
    bool m_olangcheck = false;
    QServ::QServ(bool olangcheck, int maxolangwarns,
                 char cmdprefix) {
        m_lastcommand = 0;
        m_olangcheck = olangcheck;
        m_maxolangwarns = maxolangwarns;
        m_cmdprefix = cmdprefix;
        m_db = NULL;
        }

        QServ::~QServ() { 
        if(m_db) sqlite3_close(m_db);
        }

        sqlite3 *QServ::getDB() {
        std::lock_guard<std::recursive_mutex> lock(qserv_mutex);
        if(!m_db) {
            if(sqlite3_open("playerinfo.db", &m_db) != SQLITE_OK) {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(m_db));
                sqlite3_close(m_db);
                m_db = NULL;
            }
            else {
            	//set timeout to prevent race condition on simultaneous connections
            	sqlite3_busy_timeout(m_db, 1000); 
        	}
        }
        return m_db;
        }

    
    bool QServ::initgeoip(const char *filename) {
        m_geoip = GeoIP_open(filename, GEOIP_STANDARD);
        if(m_geoip == NULL) return false;
        return true;
    }
    
    bool QServ::initcitygeoip(const char *filename) {
        city_geoip = GeoIP_open(filename, GEOIP_STANDARD);
        if(city_geoip == NULL) return false;
        return true;
    }
    
    char *QServ::congeoip(const char *ip) {
        return (char*)GeoIP_country_name_by_name(m_geoip, ip);
    }
    
    bool sendnearstatement = false;
    bool is_unknown_ip = false;
    bool geoip_record_copied = false;
    std::string QServ::cgip(const char *ip)  {
        std::stringstream gipi;
        const char delimiter[] = ", ";
        if (!city_geoip || !ip) {
            gipi << "unknown location";
            is_unknown_ip = true;
            return gipi.str();
        }
        GeoIPRecord *gipr = GeoIP_record_by_addr(city_geoip, ip);
        
        if(gipr) {
            if(gipr->city != NULL && gipr->region != NULL && isalpha(*gipr->region) && gipr->country_name != NULL) {
                gipi << gipr->city << delimiter << gipr->region << delimiter << gipr->country_name;
                sendnearstatement = true;
            }
            else if(gipr->city != NULL && gipr->country_name != NULL) {
                gipi << gipr->city << delimiter << gipr->country_name;
                sendnearstatement = true;
            }
            else if(gipr->city != NULL) {
                gipi << gipr->city;
                sendnearstatement = true;
            }
            else if(gipr->country_name != NULL) {
                gipi << gipr->country_name;
                sendnearstatement = false;
            }
            // Free it immediately while inside the block before we return
            GeoIPRecord_delete(gipr); 
        }
        else {
            gipi << "unknown location";
            is_unknown_ip = true;
        }
        return gipi.str();
    }
    
    void QServ::newcommand(const char *name, const char *desc, int priv, void (*callback)(int, char **args, int),
                           int args) {
        if(m_lastcommand >= 50) {
    		logoutf("[ERROR] Cannot register command '%s': Max limit of 50 reached.", name);
    		return;
		}
        snprintf(m_command[m_lastcommand].name, sizeof(m_command[m_lastcommand].name), "%c%s", m_cmdprefix, name);
		snprintf(m_command[m_lastcommand].desc, sizeof(m_command[m_lastcommand].desc), "%s", desc);
        
        m_command[m_lastcommand].priv = priv;
        m_command[m_lastcommand].id = m_lastcommand;
        m_command[m_lastcommand].func = callback;
        m_command[m_lastcommand].args = args+1;
        
        if(args > 0) {
            m_command[m_lastcommand].hasargs = true;
        } else {
            m_command[m_lastcommand].hasargs = false;
        }
        
        m_lastcommand += 1;
    }
    
    bool QServ::isCommand(char *text) {
        if(text[0] == m_cmdprefix) return true;
        return false;
    }
    
    int QServ::getCommand(char *text, char **args) {
        int CommandId = -1;
        
        for(int i = 0; i < m_lastcommand; i++) {
            if(strlen(m_command[i].name) > 1) {
                if(!strcmp(m_command[i].name, args[0])) {
                    CommandId = m_command[i].id;
                    break;
                }
            }
        }
        return CommandId;
    }
    
    void QServ::exeCommand(int command, char **args, int argc) {
        if(command > -1) {
            m_command[command].func(command, args, argc);
        }
    }
    
    char QServ::findWord(char *ctext, char *text, bool reg) {
        for(int i = 0; i < strlen(ctext); i++) {
            if(text[i+1] != ctext[i]) {
                return false;
            }
        }
        
        if(reg) {
            if(text[strlen(ctext)+1] != ' ' && text[strlen(ctext)+1] != '\0') {
                for(int j = 0; j < 3; j++) {
                    if(strcmp(owords[j], ctext)) {
                        return true;
                        break;
                    }
                }
                return false;
            }
        }
        return true;
    }
    
    char *QServ::getCommandName(int command) {
        return m_command[command].name;
    }
    
    char *QServ::getCommandDesc(int commandid) {
        return m_command[commandid].desc;
    }
    
    int QServ::getCommandPriv(int commandid) {
        return m_command[commandid].priv;
    }
    
    bool QServ::commandHasArgs(int command) {
        return m_command[command].hasargs;
    }
    
    int QServ::getCommandArgCount(int command) {
        return m_command[command].args;
    }
    
    int QServ::getlastCommand() {
        return m_lastcommand;
    }
    
    static int btimes = 0;
    void QServ::checkoLang(int cn, char *text) {
        if(m_olangcheck) {
            for(int i = 0; i < 50; i++) {
                if(strlen(owords[i]) > 0) {
                    for(int x = 0; x <= strlen(text); x++) {
                        if(!strcmp(owords[i], text+x-1)) {
                            btimes++;
                        }
                    }
                }
            }
            
            if(btimes > 0) {
                if(m_lastCI.connected) {
                    setoLangWarn(cn);
                    if(getoLangWarn(cn) == m_maxolangwarns) {
                        dcres(cn, "Offensive language");
                    } else {
                        if(getoLangWarn(cn) <= m_maxolangwarns) {
                            defformatstring(d)("\f7Watch your language \f0%s! \f3(Warning: %d)", m_lastCI.name, getoLangWarn(cn));
                            sendf(cn, 1, "ris", N_SERVMSG, d);
                        }
                    }
                }
                btimes = 0;
            }
        }
    }
    
    void QServ::setoLangWarn(int cn) {
        m_oLangWarn[cn] += 1;
    }
    
    void QServ::resetoLangWarn(int cn) {
        m_oLangWarn[cn] = 0;
    }
    
    int QServ::getoLangWarn(int cn) {
        return m_oLangWarn[cn];
    }
    
    void QServ::initCommands(void (*init)()) {
        init();
    }
    
    void QServ::setFullText(const char *text) {
        strcpy(m_fulltext, text);
    }
    
    char *QServ::getFullText() {
        return m_fulltext;
    }
    
    void QServ::setSender(int cn) {
        m_sender = cn;
    }
    
    int QServ::getSender() {
        return m_sender;
    }
    
    void QServ::setlastCI(clientinfo *ci) {
        m_lastCI = *ci;
    }
    
    clientinfo QServ::getlastCI() {
        return QServ::m_lastCI;
    }
    
    void QServ::setlastSA(bool lastsa) {
        m_lastSA = lastsa;
    }
    
    bool QServ::getlastSA() {
        return m_lastSA;
    }
    
    clientinfo *QServ::getClient(int cn) {
        return (clientinfo*)getclientinfo(cn);
    }
    
    char *QServ::cntoip(int cn) {
        static char ip[32];
        unsigned char by[4];
        
        for(int i = 0; i < 4; i++) {
            by[i] = (getclientip(cn) >> i*8) & 0xFF;
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d", by[0], by[1], by[2], by[3]);
        }
        return ip;
    }
    
   bool QServ::handleTextCommands(clientinfo *ci, char *text) {
        // 1. Initial defensive gates: Ensure pointers are valid before dereferencing
        if(!ci || !text || text[0] == '\0') return false;
    
        // 2. Thread-safety: Lock the gate to ensure state synchronization
        std::lock_guard<std::recursive_mutex> lock(qserv_mutex);
    
        setSender(ci->clientnum);
        setlastCI(ci);
        
        char ftb[1024] = {0};
        snprintf(ftb, sizeof(ftb), "%s", text);
        setFullText(ftb);
        
        if(isCommand(text)) {
            char *args[20];
            int argc = 0;
            char *token = NULL;
            
            // 3. Secure Tokenizer: Strictly bound argc to less than the size of args array (20)
			// 1. Create a copy strictly for tokenization, this prevents mem leaking of "text"
			char temp_copy[1024] = {0};
			copystring(temp_copy, text, sizeof(temp_copy)); 

			token = strtok(temp_copy, " "); 
			while(token != NULL && argc < 20) {
   				args[argc] = token;
    			argc++;
    			token = strtok(NULL, " ");
			}
            
            // 4. Validation Gate: If input was empty or contained only spaces, drop safely
            if(argc == 0 || !args[0]) {
                return false;
            }
            
            int command = getCommand(0, args);
            out(ECHO_CONSOLE, "%s issued: %s", colorname(ci), ftb);
            
            if(command >= 0) {
                char fulltext[1024] = {0};
                size_t prefix_len = strlen(args[0]) + 1;
                if (prefix_len < strlen(ftb)) {
                    copystring(fulltext, ftb + prefix_len, sizeof(fulltext));
                }
                setFullText(fulltext);
                
                bool fargs = false;
                if(ci->privilege >= getCommandPriv(command)) {
                    if(commandHasArgs(command)) {
                        if(getCommandArgCount(command) == argc) {
                            fargs = true;
                        } else {
                            fargs = false;
                        }
                    } else {
                        fargs = true;
                    }
                    
                    setlastSA(fargs);
                    exeCommand(command, args, argc);

                    return false;
                } else {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f3Insufficient permission");

                    return false;
                }
            } else {
                sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f3Error: Command not found. Use \f2\"#help\" \f3for a list of commands.");

                return false;
            }
        } else {
            // 5. Secure IRC string verification: Ensure client attributes exist before using them
            if(strlen(ftb) < 1024 && ci->name[0] != '\0' && irc.isConnected()) {
                irc.speak("%s(%d): %s\r\n", ci->name, ci->clientnum, ftb);
                printf("%s(%d): %s\r\n", ci->name, ci->clientnum, ftb);
            }
        }
        
        return false;
    }
    
    bool QServ::isLangWarnOn() {
        return m_olangcheck;
    }
    
    void QServ::setoLang(bool on) {
        m_olangcheck = on;
    }
    
    void QServ::setCmdPrefix(unsigned char cp) {
        m_cmdprefix = cp;
    }
    
    char QServ::getCmdPrefix() {
        return m_cmdprefix;
    }
    
      static int callback(void *data, int argc, char **argv, char **azColName){
        int i;
        //fprintf(stderr, "%s: ", (const char*)data); //prints callback success
        for(i=0; i<argc; i++){
            printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        }
        printf("\n");
        return 0;
    }
    
    bool isPartOf(const char* w1, const char* w2) {
        int j = 0;
        size_t len1 = strlen(w1);
        size_t len2 = strlen(w2);
        
        for(size_t i = 0; i < len1; i++) {
            if(w1[i] == w2[j]) {
                j++;
                if(j == len2) return true; // Found the full substring match sequentially
            }
        }
        return false;
    }
    
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#if !defined(_WIN32)
    #include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <net/if.h>
	#include <ifaddrs.h>
#endif
#include <errno.h>

     bool IsAlphabetical(char c) { //also allows spaces
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == ' ');
     }

    //replace string
    void RString(std::string& subject, const std::string& search, const std::string& replace) {
        size_t pos = 0;
        while ((pos = subject.find(search, pos)) != std::string::npos) {
            subject.replace(pos, search.length(), replace);
            pos += replace.length();
        }
    }

    void UTFEncode(std::string& s) {
        RString(s, "À", "A"); RString(s, "È", "E"); RString(s, "Ì", "I");
        RString(s, "Ò", "O"); RString(s, "Ù", "U"); RString(s, "à", "a");
        RString(s, "è", "e"); RString(s, "ì", "i"); RString(s, "ò", "o");
        RString(s, "ù", "u"); RString(s, "É", "E"); RString(s, "Í", "I");
        RString(s, "Ó", "O"); RString(s, "Ú", "U"); RString(s, "Ý", "Y");
        RString(s, "á", "a"); RString(s, "é", "e"); RString(s, "í", "i");
        RString(s, "ó", "o"); RString(s, "ú", "u"); RString(s, "ý", "y");
        RString(s, "Â", "A"); RString(s, "Ê", "E"); RString(s, "Î", "I");
        RString(s, "Ô", "O"); RString(s, "Û", "U"); RString(s, "â", "a");
        RString(s, "ê", "e"); RString(s, "î", "i"); RString(s, "ô", "o");
        RString(s, "û", "u"); RString(s, "Ñ", "N"); RString(s, "Õ", "O");
        RString(s, "ã", "a"); RString(s, "ñ", "n"); RString(s, "õ", "o");
        RString(s, "Ä", "A"); RString(s, "Ë", "E"); RString(s, "Ï", "I");
        RString(s, "Ö", "O"); RString(s, "Ü", "U"); RString(s, "Ÿ", "Y");
        RString(s, "ä", "a"); RString(s, "ë", "e"); RString(s, "ï", "i");
        RString(s, "ö", "o"); RString(s, "ü", "u"); RString(s, "ÿ", "y");
    }
    
	void QServ::getLocation(clientinfo *ci) {
	    if (!ci) return;
	
	    // 1. SCOPE: Lock briefly to copy volatile data out of the shared clientinfo object
	    std::string ip;
	    std::string client_name;
	    {
	        std::lock_guard<std::recursive_mutex> lock(qserv_mutex);
	        char *ip_raw = toip(ci->clientnum);
	        if (!ip_raw) return;
	        ip = ip_raw;
	        client_name = (ci->name[0] != '\0') ? ci->name : "Unknown";
	    } // Lock automatically releases here
	
	    // 2. Perform Network/Heavy operations OUTSIDE the lock
	    char buf[64] = ""; 
	#ifndef _WIN32
	    struct ifaddrs *myaddrs = NULL, *ifa = NULL;
	    void *in_addr = NULL;
	    if (getifaddrs(&myaddrs) == 0) {
	        for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next) {
	            if (ifa->ifa_addr != NULL && (ifa->ifa_flags & IFF_UP) && ifa->ifa_addr->sa_family == AF_INET) {
	                struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
	                if (inet_ntop(AF_INET, &s4->sin_addr, buf, sizeof(buf))) break;
	            }
	        }
	        freeifaddrs(myaddrs);
	    }
	#endif
	
	    std::string geo_string_holder;
	    const char *location = NULL;
	    int type = 0, typeconsole = 0;
	    const char *types[] = { " connected from \f3unknown", " \f7connected from \f3unknown", sendnearstatement ? " \f7connected near\f0" : " \f7connected from\f0" };
	    const char *typesconsole[] = { " connected from unknown", " connected from unknown/localhost", sendnearstatement ? " connected near " : " connected from " };
	    char lmsg[512] = {0}, pmsg[255] = {0};
	
	    if (!enable_HTTP_geo && ip.length() > 2) {
	        if (!strcmp(ip.c_str(), "127.0.0.1") || !strcmp(buf, ip.c_str()) || isPartOf(ip.c_str(), "172.16") || isPartOf(ip.c_str(), "192.168")) {
	            location = "localhost";
	        } else {
	            geo_string_holder = cgip(ip.c_str()); 
	            location = geo_string_holder.c_str(); 
	        }
	
	        if (!location || !strcmp("(null)", location) || is_unknown_ip) {
	            type = 0; typeconsole = 0;
	        } else if (!strcmp(ip.c_str(), "127.0.0.1") || !strcmp(buf, ip.c_str()) || isPartOf(ip.c_str(), "172.16") || isPartOf(ip.c_str(), "192.168")) {
	            type = 1; typeconsole = 1;
	        } else {
	            type = 2; typeconsole = 2;
	            snprintf(lmsg, sizeof(lmsg), "%s %s", types[type], location);
	            snprintf(pmsg, sizeof(pmsg), "%s%s", typesconsole[typeconsole], location);
	        }
	    }
	
	    std::string s;
	    bool http_completed = false;
	    if (enable_HTTP_geo && ip.length() > 2) {
	        try {
	            if (ip == "127.0.0.1" || ip == "::1") {
	                s = "Localhost";
	                http_completed = true;
	            } else {
	                defformatstring(r_str)("http://ip-api.com/line/%s?fields=city,regionName,country", ip.c_str());
	                http::Request req(r_str);
	                const http::Response res = req.send("GET"); 
	                s.assign(res.body.begin(), res.body.end());
	                RString(s, "\n", " > ");
	                UTFEncode(s);
	                if (s.length() >= 2) s.erase(s.length() - 2, 2);
	                http_completed = true;
	            }
	        } catch (const std::exception& e) {
	            std::cerr << "no geo information for IP: " << ip << '\n';
	        }
	    }
	
	    // 3. SCOPE: Lock again ONLY for printing/updating shared states
	    {
	        std::lock_guard<std::recursive_mutex> lock(qserv_mutex);
	        bool is_still_connected = false;
	        loopv(clients) { if (clients[i] == ci) { is_still_connected = true; break; } }
	
	        if (ip.length() > 2 && is_still_connected) {
	            if (!enable_HTTP_geo) {
	                defformatstring(msg)("\f0%s\f7%s", client_name.c_str(), (type < 2) ? types[type] : lmsg);
	                defformatstring(nocolormsg)("%s%s", client_name.c_str(), (typeconsole < 2) ? typesconsole[typeconsole] : pmsg);
	                out(ECHO_SERV, "%s", msg);
	                out(ECHO_NOCOLOR, "%s", nocolormsg);
	                geoip_record_copied = true;
	                is_unknown_ip = false; 
	            } else if (enable_HTTP_geo && http_completed) {
	                defformatstring(msg)("\f0%s \f7connected from \f4%s", colorname(ci), s.c_str());
	                out(ECHO_SERV, "%s", msg);
	                defformatstring(cmsg)("%s connected from %s", colorname(ci), s.c_str());
	                out(ECHO_CONSOLE, "%s", cmsg);
	            }
	        }
	    }
	}
    
    void QServ::checkMsg(int cn) {
        ms[cn].count += 1;
    }
    
    int QServ::getMsgC(int cn) {
        return ms[cn].count;
    }
    
    void QServ::resetMsg(int cn) {
        ms[cn].count = 0;
    }
}
