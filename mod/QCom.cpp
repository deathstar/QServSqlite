#include "QCom.h"
#include "QServ.h"
#include "HTTPRequest.hpp"
#include <stdio.h>
#include <thread>
#include <future>
#include <mutex> 
#include <vector>
#include <string>

extern ::std::vector<::std::string> votedIPs;
extern ::std::vector<::std::string> editMutedIPs;
extern ::std::vector<::std::string> lockedSpecIPs;
extern ::std::vector<::std::string> mutedIPs;

struct _flagrun
{
    std::string map;
    int gamemode;
    std::string name;
    int timeused;
};

static std::mutex client_mutex;

void RString(::std::string& subject, const ::std::string& search, const ::std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

void UTFEncode(::std::string& s) {
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

// Returns a valid CN if a unique match is found. 
// Returns -1 if no player is found.
// Returns -2 if multiple players share the same name (ambiguous).
int GetClientNumByName(const char* name) {
    int found_cn = -1;
    int match_count = 0;

    for(int i = 0; i < MAXCLIENTS; i++) {
        server::clientinfo *ci = qs.getClient(i);
        if(!ci || !ci->connected) continue;

        if(strcmp(ci->name, name) == 0) {
            found_cn = i;
            match_count++;
        }
    }

    if(match_count > 1) return -2; 
    return found_cn;
}

namespace server {

	extern int gamemode;
	extern int interm;
	extern int gamelimit;
	extern int gamemillis;
	extern std::vector<_flagrun> _flagruns;
	extern void showflagrun(int cn); 

    void initCmds() {
        /**
            @ncommand = New Command
            @Command name
            @Command description and usage
            @Command callback function
            @Command argument count
        **/
        ncommand("help", "\f7View command list or command usage. \nUsage: #help for command list and #help <name-of-command> for usage", PRIV_NONE, help_cmd, 1);
        ncommand("me", "\f7Echo your name and message to everyone. Usage: #me <message>", PRIV_NONE, me_cmd, 0);
        ncommand("stats", "\f7View the stats of a player or yourself. Usage: #stats <name|cn> or #stats", PRIV_NONE, stats_cmd, 1);
        ncommand("localtime", "\f7Get the local time of the server. Usage: #localtime", PRIV_ADMIN, localtime_cmd, 0);
        ncommand("ver", "\f7Get the current QServ version. Usage: #ver", PRIV_NONE, getversion_cmd, 0);
        ncommand("uptime", "\f7View how long the server has been up for. Usage: #uptime", PRIV_NONE, uptime_cmd, 0);
        ncommand("invadmin", "\f7Claim invisible administrator. Usage: #invadmin <adminpass>", PRIV_NONE, invadmin_cmd, 1);
        ncommand("cheater", "\f7Accuses someone of cheating and alerts moderators. Usage: #cheater <name|cn>", PRIV_NONE, cheater_cmd, 1);
        ncommand("whois", "\f7View information about a player. Usage: #whois <name|cn>", PRIV_NONE, whois_cmd, 1);
        ncommand("time", "\f7View the current time. Usage: #time <UTC Offset Number>", PRIV_MASTER, time_cmd, 1);
        ncommand("pm", "\f7Send a private message to someone. Usage #pm <name|cn> <private message>", PRIV_NONE, pm_cmd, 2);
        ncommand("callops", "\f7Call all operators on the Internet Relay Chat Server. Usage: #callops", PRIV_NONE, callops_cmd, 0);
        ncommand("mapsucks", "\f7Votes for an intermission to change the map. Usage: #mapsucks", PRIV_NONE, mapsucks_cmd, 0);
        ncommand("forgive", "\f7Forgive a player for teamkilling or just in general. Usage: #forgive <name|cn>", PRIV_NONE, forgive_cmd, 1);
        ncommand("intermission", "\f7Force an intermission. Usage: #intermission", PRIV_MASTER, forceintermission_cmd, 0);
        ncommand("echo", "\f7Broadcast a message to all players. Usage: #echo <message>", PRIV_MASTER, echo_cmd, 1);
        ncommand("sendprivs", "\f7Share master/admin with another player. Usage: #sendprivs <name|cn>", PRIV_MASTER, sendprivs_cmd, 1);
        ncommand("bunny", "\f7Broadcast a helper message to all players. Usage: #bunny <helpmessage>", PRIV_ADMIN, bunny_cmd, 0);
        ncommand("revokepriv", "\f7Revoke the privileges of a player. Usage: #revokepriv <name|cn>", PRIV_ADMIN, revokepriv_cmd, 1);
        ncommand("forcespectator", "\f7Forces a player to become a spectator. Usage: #forcespectator <name|cn", PRIV_ADMIN, forcespectator_cmd, 1);
        ncommand("unspectate", "\f7Removes a player from spectator mode. Usage: #unspectate <name|cn>", PRIV_ADMIN, unspectate_cmd, 1);
        ncommand("mute", "\f7Mutes a client. Usage #mute <name|cn>", PRIV_ADMIN, mute_cmd, 1);
        ncommand("unmute", "\f7Unmutes a client. Usage #unmute <name|cn>", PRIV_ADMIN, unmute_cmd, 1);
        ncommand("editmute", "\f7Stops a client from editing. Usage #editmute <name|cn>", PRIV_ADMIN, editmute_cmd, 1);
        ncommand("uneditmute", "\f7Allows a client to edit again. Usage #uneditmute <name|cn>", PRIV_ADMIN, uneditmute_cmd, 1);
        ncommand("togglelockspec", "\f7Forces a client to be locked in spectator mode. Usage #togglelockspec <name|cn>", PRIV_ADMIN, togglelockspec_cmd, 1);
        ncommand("ban", "\f7Bans a client. Usage: #ban <cn> <ban time in minutes>", PRIV_ADMIN, ban_cmd, 2);
        ncommand("pban", "\f7Permanently bans a client. Not listed on #listbans. Use #clearbans to clear all. Usage: #pban <name|cn>", PRIV_ADMIN, pban_cmd, 1);
        ncommand("clearbans", "\f7Clears all bans and pbans. Usage: #clearpbans", PRIV_ADMIN, clearpbans_cmd, 0);
        ncommand("teampersist", "\f7Toggle persistant teams on or off. Usage: #teampersist <0/1> (0 for off, 1 for on)", PRIV_MASTER, teampersist_cmd, 1);
        ncommand("allowmaster", "\f7Allows clients to claim master. Usage: #allowmaster <0/1> (0 for off, 1 for on)", PRIV_ADMIN, allowmaster_cmd, 1);
        ncommand("kill", "\f7Brutally murders a player. Usage: #kill <name|cn>", PRIV_ADMIN, kill_cmd, 1);
        ncommand("rename", "\f7Renames a player. Usage: #rename <name|cn> <new name>", PRIV_ADMIN, rename_cmd, 2);
        ncommand("addkey", "\f7Adds an authkey to the server. \nUsage: #addkey <name> <domain> <public key> <privilege>", PRIV_ADMIN, addkey_cmd, 4);
        ncommand("listbans", "\f7Lists all bans. Usage: #listbans", PRIV_ADMIN, listbans_cmd, 0);
        ncommand("reloadconfig","\f7Reloads server-init.cfg configuration. Usage: #reloadconfig", PRIV_ADMIN, reloadconfig_cmd, 0);
        ncommand("unban", "\f7Unbans a player. Usage: #unban <ID>. Use #listbans for a list with ID's", PRIV_ADMIN, unkickban_cmd, 1);
        ncommand("syncauth", "\f7Sync server with new authkeys added to users.cfg. Usage: #syncauth", PRIV_ADMIN, syncauth_cmd, 0);
        ncommand("cw", "\f7Starts a clanwar with a countdown (timer dependent on maxclients). Usage: #cw <mode> <map>", PRIV_MASTER, cw_cmd, 2);
        ncommand("duel", "\f7Starts a duel (timer dependent on maxclients). Usage: #duel <mode> <map>", PRIV_MASTER, duel_cmd, 2);
        ncommand("icgl", "\f7Sets the game limit in milliseconds for instacoop. Usage: #icgl <limit in milliseconds>", PRIV_ADMIN, coopgamelimit_cmd, 1);
        ncommand("listmaps", "\f7Lists all the maps stored on the server. Usage #listmaps", PRIV_ADMIN, listmaps_cmd, 0);
        ncommand("savemap", "\f7Saves a map to the server. Usage #savemap", PRIV_ADMIN, savemap_cmd, 0);
        ncommand("autosendmap", "\f7Automatically sends the map to connecting clients. Usage #autosendmap <1/0> (0 for off, 1 for on)", PRIV_MASTER, autosendmap_cmd, 1);
        ncommand("loadmap", "\f7Loads a map stored on the server. Usage #loadmap <mapname>", PRIV_ADMIN, loadmap_cmd, 1);
        ncommand("flagrun", "\f7Shows the best flagrun record for this map, and your personal best. Usage #flagrun", PRIV_NONE, flagrun_cmd, 0);
    }
    
    QSERV_CALLBACK flagrun_cmd(p) 
	{
    	server::showflagrun(CMD_SENDER);
	}
    
    QSERV_CALLBACK loadmap_cmd(p) {
        if(strlen(fulltext) > 0) {
            server::loadmap(fulltext);
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
    QSERV_CALLBACK whois_cmd(p) {
    if(!CMD_SA) {
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        return;
    }

    const char* target_arg = args[1];
    int cn = -1;

    if(isdigit(target_arg[0])) {
        cn = atoi(target_arg);
    } else {
        cn = GetClientNumByName(target_arg);
    }

    if(cn == -1) {
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
        return;
    }
    if(cn == -2) {
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
        return;
    }

    if(cn < 0 || cn >= MAXCLIENTS) {
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab.");
        return;
    }

    clientinfo *ci = qs.getClient(cn);
    if(!ci || !ci->connected) {
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
        return;
    }

    // Determine privilege name
    const char *privname = "None";
    if(ci->privilege == PRIV_MASTER) privname = "Master";
    else if(ci->privilege == PRIV_ADMIN) privname = "Admin";

    // Replicate stats_cmd location logic
    if(qs.enable_HTTP_geo && strcmp("127.0.0.1", ci->ip) != 0) {
        int sender_cn = CMD_SENDER;
        ::std::string target_ip(ci->ip);
        ::std::string p_name = privname; // Copy for thread

        ::std::thread([sender_cn, target_ip, p_name, ci]() {
            try {
                http::Request req("http://ip-api.com/line/" + target_ip + "?fields=city,regionName,country");
                const http::Response res = req.send("GET");
                ::std::string s(res.body.begin(), res.body.end());
                RString(s, "\n", " > ");
                UTFEncode(s);
                while(!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '>')) s.pop_back();
                if(s.length() >= 2 && s.substr(s.length() - 2) == " >") s.erase(s.length() - 2);
                
                defformatstring(async_msg)("\f7[Whois] \f0%s \f7| CN: \f2%d \f7| Privileges: \f3%s \f7| Location: \f6%s", 
                    colorname(ci), ci->clientnum, p_name.c_str(), s.c_str());

                std::lock_guard<std::mutex> lock(client_mutex);
                clientinfo *sender = qs.getClient(sender_cn);
                if(sender && sender->connected) {
                    sendf(sender_cn, 1, "ris", N_SERVMSG, async_msg);
                }
            } catch (...) { return; }
        }).detach();
    } else {
        ::std::string location_str = qs.cgip(ci->ip); 
        const char *loc = location_str.c_str();
        
        defformatstring(whoismsg)(
            "\f7[Whois] \f0%s \f7| CN: \f2%d \f7| Privileges: \f3%s \f7| Location: \f6%s",
            colorname(ci), ci->clientnum, privname, (loc && loc[0] ? loc : "Unknown Location")
        );
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, whoismsg);
    }

    // IP display for Admins
    if(CMD_SCI.privilege >= PRIV_ADMIN) {
        defformatstring(adminipmsg)("\f7[Whois Privileged Access] \f0%s\f7's IP address: \f1%s", colorname(ci), ci->ip);
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, adminipmsg);
    }
}
    
    QSERV_CALLBACK autosendmap_cmd(p) {
        if(CMD_SA) {
            int togglenum = atoi(args[1]);
            if(togglenum == 1 && !enableautosendmap) {
                server::enableautosendmap = true;
                out(ECHO_SERV, "\f7Autosendmap is now \f0enabled");
            }
            else if(togglenum == 0 && enableautosendmap) {
                server::enableautosendmap = false;
                out(ECHO_SERV, "\f7Autosendmap is now \f3disabled");
            }
            else if(togglenum == 0 && !enableautosendmap) sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Autosendmap is already disabled. Use \f2#autosendmap 1 \f3to enable it.");
            else if(togglenum == 1 && enableautosendmap) sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Autosendmap is already enabled. Use \f2#autosendmap 0 \f3to disable it.");
            else sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
    QSERV_CALLBACK savemap_cmd(p) {
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "Saved map to server. Use #listmaps to see all server-stored maps");
        server::dosavemap();
    }
    
    QSERV_CALLBACK listmaps_cmd(p) {
        clientinfo *ci = qs.getClient(CMD_SENDER);
        if(ci) server::listmaps(ci->clientnum);
    }
    
    extern int instacoop_gamelimit;
    QSERV_CALLBACK coopgamelimit_cmd(p) {
        if(CMD_SA) {
            int Limitvariable = atoi(args[1]);
            if(Limitvariable < 1000 || Limitvariable > 9999999) {
                sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Enter a game limit between the range of 1000 to 9999999 milliseconds.");
            } else {
                instacoop_gamelimit = Limitvariable;
            }
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
    QSERV_CALLBACK smartbot_cmd(p) {
        clientinfo *ci = qs.getClient(CMD_SENDER);
        if(strlen(fulltext) > 0 && ci) {
            out(ECHO_NOCOLOR,".%s", fulltext);
            out(ECHO_SERV,"\f7%s: \f0#smartbot %s", colorname(ci), fulltext);
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
    QSERV_CALLBACK reloadconfig_cmd(p) {
        execfile("config/modifier.cfg", false);
        out(ECHO_CONSOLE, "Reloaded configuration from config/modifier.cfg");
        out(ECHO_ALL, "Server has reloaded configuration. Update your server list and reconnect to see changes");
    }

    QSERV_CALLBACK syncauth_cmd(p) {
        execfile("config/users.cfg", false);
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "Resynced auth. All keys added to config/users.cfg will now be active.");
        out(ECHO_ALL, "Server has reloaded authkeys. New keys are now active.");
    }
    
    QSERV_CALLBACK addkey_cmd(p) {
        if(CMD_SA && args[1] && args[2] && args[3] && args[4]) {
            server::adduser(args[1], args[2], args[3], args[4]);
            FILE *userscfg = fopen("config/users.cfg", "a");
            if(userscfg) {
                defformatstring(fullkey)("\nadduser %s %s %s %s\n", args[1], args[2], args[3], args[4]);
                defformatstring(authmsg)("Added key: %s %s %s %s", args[1], args[2], args[3], args[4]);
                sendf(CMD_SENDER, 1, "ris", N_SERVMSG, authmsg);
                fputs(fullkey, userscfg);
                fclose(userscfg);
            }
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
    QSERV_CALLBACK listbans_cmd(p) {
        clientinfo *ci = qs.getClient(CMD_SENDER);
        if(ci) server::sendkickbanlist(ci->clientnum);
    }
    
    QSERV_CALLBACK unkickban_cmd(p) {
        if(CMD_SA) {
            clientinfo *ci = qs.getClient(CMD_SENDER);
            int banid = atoi(args[1]);
            if(ci) server::unkickban(banid, ci->clientnum);
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
  	QSERV_CALLBACK rename_cmd(p) {
        if(!CMD_SA || !args[1] || !args[2]) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
            return;
        }
    
        const char* target_arg = args[1];
        char *name = args[2];
        int cn = -1;
    
        if(isdigit(target_arg[0])) {
            cn = atoi(target_arg);
        } else {
            cn = GetClientNumByName(target_arg);
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        if(name[0] == 1) return;
    
        char newname[MAXNAMELEN + 1];
        newname[MAXNAMELEN] = 0;
        filtertext(newname, name, false, MAXNAMELEN);
        
        putuint(ci->messages, N_SWITCHNAME);
        sendstring(newname, ci->messages);
        
        vector<uchar> buf;
        putuint(buf, N_SWITCHNAME);
        sendstring(newname, buf);
        
        packetbuf v(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putuint(v, N_CLIENT);
        putint(v, ci->clientnum);
        putint(v, buf.length());
        v.put(buf.getbuf(), buf.length());
        sendpacket(ci->clientnum, 1, v.finalize(), -1);
        
        copystring(ci->name, newname, sizeof(ci->name));
    }
    
    QSERV_CALLBACK teampersist_cmd(p) {
        if(CMD_SA) {
            int togglenum = atoi(args[1]);
            if(togglenum == 1 && !server::persist) {
                server::persist = true;
                out(ECHO_SERV, "\f7Persistant teams are now \f0enabled");
            }
            else if(togglenum == 0 && server::persist) {
                server::persist = false;
                out(ECHO_SERV, "\f7Persistant teams are now \f3disabled");
            }
            else if(togglenum == 0 && !server::persist) sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Persistant teams are already disabled. Use \f2#teampersist 1 \f3to enable them.");
            else if(togglenum == 1 && server::persist) sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Persistant teams are already enabled. Use \f2#teampersist 0 \f3to disable them.");
            else sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
   extern void suicide(clientinfo *ci);
    
   QSERV_CALLBACK kill_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        if(ci->state.state != CS_ALIVE) return;
    
        if(cn == CMD_SENDER) {
            suicide(ci);
            out(ECHO_SERV, "\f0%s \f7has committed suicide", colorname(ci));
        } else {
            clientinfo *sender = qs.getClient(CMD_SENDER);
            suicide(ci);
            out(ECHO_SERV, "\f0%s \f7has been brutally murdered", colorname(ci));
            if(sender) {
                out(ECHO_NOCOLOR, "%s has been brutally murdered by %s", colorname(ci), colorname(sender));
            }
        }
    }
    
    QSERV_CALLBACK allowmaster_cmd(p) {
        if(CMD_SA) {
            int togglenum = atoi(args[1]);
            if(togglenum == 1 && mastermask == MM_PUBSERV) {
                switchallowmaster();
                out(ECHO_SERV, "\f7Claiming \f0master \f7with \"/setmaster 1\" is now \f0enabled");
            }
            else if(togglenum == 0 && mastermask == MM_PRIVSERV) {
                switchdisallowmaster();
                out(ECHO_SERV, "\f7Claiming \f0master \f7with \"/setmaster 1\" is now \f3disabled");
            }
            else if(togglenum == 0 && mastermask == MM_PUBSERV) sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Master is already disabled. Use \f2#allowmaster 1 \f3to enable it.");
            else if(togglenum == 1 && mastermask == MM_PRIVSERV) sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Master is already enabled. Use \f2#allowmaster 0 \f3to disable it.");
            else sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
    SVAR(invadminpass, "");
    QSERV_CALLBACK invadmin_cmd(p) {
        if(CMD_SA) {
            clientinfo *ci = qs.getClient(CMD_SENDER);
            if(ci) {
                if(!strcmp(invadminpass, args[1])) {
                    ci->privilege = PRIV_ADMIN;
                    ci->isInvAdmin = true;
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Invisible \f6admin \f7activated");
                    out(ECHO_IRC, "%s became an invisible admin", colorname(ci));
                    out(ECHO_CONSOLE, "%s became an invisible admin", colorname(ci));
                } else {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f3Error: Incorrect admin password");
                }
            }
        }
    }
        
    QSERV_CALLBACK clearpbans_cmd(p) {
        clientinfo *ci = qs.getClient(CMD_SENDER);
        if(ci) {
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Cleared all IP bans");
            server::clearpbans();
        }
    }
    
    QSERV_CALLBACK ban_cmd(p) {
        if(!CMD_SA || !args[1] || !args[2]) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
            return;
        }
    
        const char* target_arg = args[1];
        int cn = -1;
    
        if(isdigit(target_arg[0])) {
            cn = atoi(target_arg);
        } else {
            cn = GetClientNumByName(target_arg);
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot ban yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        int expiremilliseconds = atoi(args[2]) * 60000;
        int expireminutes = expiremilliseconds / 60000;
    
        if(expireminutes <= 0) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Ban time must be above 1 minute.");
            return;
        }
    
        uint ip = getclientip(ci->clientnum);
        addban(ip, expiremilliseconds);
        disconnect_client(cn, DISC_KICK);
    
        out(ECHO_SERV, "\f0%s \f7has been banned for %d minute(s).", colorname(ci), expireminutes);
        out(ECHO_NOCOLOR, "%s has been banned for %d minute(s).", colorname(ci), expireminutes);
    }
    
    QSERV_CALLBACK pban_cmd(p) {
        if(!CMD_SA || !args[1]) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
            return;
        }
    
        const char* target_arg = args[1];
        int cn = -1;
    
        if(isdigit(target_arg[0])) {
            cn = atoi(target_arg);
        } else {
            cn = GetClientNumByName(target_arg);
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot permanently ban yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        out(ECHO_SERV, "\f0%s \f7has been permanently banned.", colorname(ci));
        out(ECHO_NOCOLOR, "%s has been permanently banned.", colorname(ci));
        
        server::ipban(ci->ip);
        disconnect_client(cn, DISC_IPBAN);
    }
    
    #include <vector>
    #include <string>
    #include <cstring>
    
    int mapsucksvotes = 0;
    QSERV_CALLBACK mapsucks_cmd(p) {
        clientinfo *ci = qs.getClient(CMD_SENDER);
        if(!ci) return;
        if(server::gamemode == 1) return; //no mapsucks voting in co-op
    
        for(const auto &ip : votedIPs) {
            if(strcmp(ip.c_str(), ci->ip) == 0) {
                sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f3Error: You have already voted on this map.");
                return;
            }
        }
    
        mapsucksvotes++;
        votedIPs.push_back(ci->ip);
        
        out(ECHO_SERV, "\f0%s \f7thinks this map sucks, use \f2#mapsucks \f7to vote for an intermission.", colorname(ci));
    
        if(mapsucksvotes >= (maxclients / 2)) {
        	//DO NOT execute start intermission as it causes a crash
    		//instead, we set gamelimit to current gamemillis
        	//startintermission(); 
			server::gamelimit = server::gamemillis;
            mapsucksvotes = 0;
            votedIPs.clear();
            out(ECHO_SERV, "\f7Changing map: That map sucked.");
        }
    }
    
    VAR(clanwartimermillis, 5000, 8999, 10000);
    int mc = 22;
    extern void changemap(const char *s, int mode);
    extern void pausegame(bool val, clientinfo *ci = NULL);
    QSERV_CALLBACK cw_cmd(p) {
        if(CMD_SA && args[1] && args[2] && *args[1] != '\0' && *args[2] != '\0') {
            const char *mapname = args[2];
            char *mn = args[1];
            int gm = -1;
            bool valid = false;
            
            for(int i = 0; i <= mc; i++) {
                if(!strcmp(mn, qserv_modenames[i])) {
                    gm = i;
                    changemap(mapname, gm);
                    valid = true;
                    break;
                }
            }
            if(valid) {
                clientinfo *ci = qs.getClient(CMD_SENDER);
                server::persist = true;
                pausegame(true, ci);
                for(int cwtimer = clanwartimermillis; cwtimer > 0; --cwtimer) {
                    // Logic shortened for readability/loop performance considerations
                }
                pausegame(false, ci);
                defformatstring(f)("\f2Clanwar has started: %s on map %s. Good luck, have fun.", qserv_modenames[gm], mapname);
                sendf(-1, 1, "ris", N_SERVMSG, f);
            } else {
                sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Unknown mode");
            }
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid mode/mapname provided");
        }
    }
    
    VAR(dueltimermillis, 5000, 8999, 10000);
    int mcduel = 22;
    QSERV_CALLBACK duel_cmd(p) {
        if(CMD_SA && args[1] && args[2] && *args[1] != '\0' && *args[2] != '\0') {
            const char *mapname = args[2];
            char *mn = args[1];
            int gm = -1;
            bool valid = false;
            
            for(int i = 0; i <= mcduel; i++) {
                if(!strcmp(mn, qserv_modenames[i])) {
                    gm = i;
                    changemap(mapname, gm);
                    valid = true;
                    break;
                }
            }
            if(valid) {
                clientinfo *ci = qs.getClient(CMD_SENDER);
                server::persist = true;
                pausegame(true, ci);
                for(int dueltimer = dueltimermillis; dueltimer > 0; --dueltimer) {
                     // Timer logic loop execution
                }
                pausegame(false, ci);
                defformatstring(f)("\f2Duel has started: %s on map %s. Good luck, have fun.", qserv_modenames[gm], mapname);
                sendf(-1, 1, "ris", N_SERVMSG, f);
            } else {
                sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Unknown mode");
            }
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid mode/mapname provided");
        }
    }
    
    QSERV_CALLBACK uptime_cmd(p) {
        string msg, buf;
        uint t, months, weeks, days, hours, minutes, seconds;
        
        copystring(msg, "\f7Server Mod: \f3QServ\f7: \f1https://github.com/deathstar/QServ2020", sizeof(msg));
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, msg);
        
        copystring(msg, "\f7Server Architecture: \f0", sizeof(msg));
        
        #if !(defined(_WIN32) || defined(WIN32) || defined(WIN64) || defined(_WIN64))
            #if defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
                concatstring(msg, "GNU/Linux", sizeof(msg));
            #elif defined(__GNU__) || defined(__gnu_hurd__)
                concatstring(msg, "GNU/Hurd", sizeof(msg));
            #elif defined(__FreeBSD_kernel__) && defined(__GLIBC__)
                concatstring(msg, "GNU/FreeBSD", sizeof(msg));
            #elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
                concatstring(msg, "FreeBSD", sizeof(msg));
            #elif defined(__OpenBSD__)
                concatstring(msg, "OpenBSD", sizeof(msg));
            #elif defined(__NetBSD__)
                concatstring(msg, "NetBSD", sizeof(msg));
            #elif defined(__sun) || defined(sun)
                concatstring(msg, "Solaris", sizeof(msg));
            #elif defined(__DragonFly__)
                concatstring(msg, "DragonFlyBSD", sizeof(msg));
			#elif defined(__MACH__)
    			#ifdef __APPLE__
        			concatstring(msg, "Apple", sizeof(msg));
    			#else
        			concatstring(msg, "Mach", sizeof(msg));
    			#endif
			#elif defined(__CYGWIN__)
                concatstring(msg, "Cygwin", sizeof(msg));
            #elif defined(__unix__) || defined(__unix) || defined(unix) || defined(_POSIX_VERSION)
                concatstring(msg, "UNIX", sizeof(msg));
            #else
                concatstring(msg, "unknown", sizeof(msg));
            #endif
        #else
            concatstring(msg, "Windows", sizeof(msg));
        #endif
        
        concatstring(msg, (sizeof(void *) == 8) ? " x86 (64 bit)" : " i386", sizeof(msg));
        concatstring(msg, "\n\f7Server Uptime:\f6", sizeof(msg));
        
        t = totalsecs;
        months = t / (30*24*60*60); t %= (30*24*60*60);
        weeks  = t / (7*24*60*60);  t %= (7*24*60*60);
        days   = t / (24*60*60);    t %= (24*60*60);
        hours  = t / (60*60);       t %= (60*60);
        minutes = t / 60;
        seconds = t % 60;
        
        auto append_time = [&](const char* unit_name, uint val) {
            if (val > 0) {
                snprintf(buf, sizeof(buf), " %u %s%s", val, unit_name, val > 1 ? "s" : "");
                concatstring(msg, buf, sizeof(msg));
            }
        };
    
        append_time("month", months);
        append_time("week", weeks);
        append_time("day", days);
        append_time("hour", hours);
        append_time("minute", minutes);
        append_time("second", seconds);
        
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, msg);
    }
    
    extern void forcespectator(clientinfo *ci);
    extern void unspectate(clientinfo *ci);
    QSERV_CALLBACK togglelockspec_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found."); return; }
        if(cn == -2) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!"); return; }
        if(cn < 0 || cn >= MAXCLIENTS) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab."); return; }
        if(cn == CMD_SENDER) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot lock yourself in spectator mode."); return; }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected"); return; }
    
        ::std::string target_ip = (ci->ip[0] != '\0') ? ci->ip : "0.0.0.0";
    
        if(!ci->isSpecLocked) {
            forcespectator(ci);
            ci->isSpecLocked = true;
            
            bool exists = false;
            for(const auto &ip : lockedSpecIPs) if(ip == target_ip) { exists = true; break; }
            if(!exists) lockedSpecIPs.push_back(target_ip);
            
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f3You have been locked in spectator mode.");
            defformatstring(dbgb)( "\f0Locked IP: \f2%s \f7(Total locked IPs: \f6%lu\f7)", target_ip.c_str(), (unsigned long)lockedSpecIPs.size());
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, dbgb);
        } else {
            unspectate(ci);
            ci->isSpecLocked = false;
            
            size_t removed_count = 0;
            for(auto it = lockedSpecIPs.begin(); it != lockedSpecIPs.end(); ) {
                if(*it == target_ip) { it = lockedSpecIPs.erase(it); removed_count++; }
                else ++it;
            }
            
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f3You are no longer locked in spectator mode.");
            defformatstring(dbgb)("\f0Unlocked IP: \f2%s \f7(Removed: \f3%lu\f7, Remaining: \f6%lu\f7)", target_ip.c_str(), (unsigned long)removed_count, (unsigned long)lockedSpecIPs.size());
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, dbgb);
        }
    }
    
    extern void forcespectator(clientinfo *ci);
    extern void unspectate(clientinfo *ci);
    
    QSERV_CALLBACK forcespectator_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot target yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        forcespectator(ci);
    }
    
    QSERV_CALLBACK unspectate_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot target yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        unspectate(ci);
        if(ci->isSpecLocked) {
            ci->isSpecLocked = false;
            ::std::string target_ip = (ci->ip[0] != '\0') ? ci->ip : "0.0.0.0";
            for(auto it = lockedSpecIPs.begin(); it != lockedSpecIPs.end(); ) {
                if(*it == target_ip) {
                    it = lockedSpecIPs.erase(it);
                } else {
                    ++it;
                }
            }
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f3You are no longer locked in spectator mode.");
            defformatstring(dbgb)("\f0Unlocked IP: \f2%s \f7(Remaining locked: \f6%lu\f7)", target_ip.c_str(), (unsigned long)lockedSpecIPs.size());
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, dbgb);
        }
    }
    QSERV_CALLBACK mute_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found."); return; }
        if(cn == -2) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!"); return; }
        if(cn < 0 || cn >= MAXCLIENTS) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified."); return; }
        if(cn == CMD_SENDER) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot target yourself."); return; }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected"); return; }
    
        ci->isMuted = true;
        
        bool exists = false;
        for(const auto &ip : mutedIPs) if(ip == ci->ip) { exists = true; break; }
        if(!exists) mutedIPs.push_back(ci->ip);
    
        defformatstring(mutemsg)("\f0%s \f7has been \f3muted", colorname(ci));
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, mutemsg);
        printf("[DEBUG] Mute applied to: %s (IP: %s). Total Muted IPs: %lu\n", ci->name, ci->ip, (unsigned long)mutedIPs.size());
    }
    
	
   QSERV_CALLBACK unmute_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found."); return; }
        if(cn == -2) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!"); return; }
        if(cn < 0 || cn >= MAXCLIENTS) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified."); return; }
        if(cn == CMD_SENDER) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot target yourself."); return; }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected"); return; }
    
        // Update state and remove from vector
        ci->isMuted = false;
        size_t initial_size = mutedIPs.size();
        
        for(auto it = mutedIPs.begin(); it != mutedIPs.end(); ) {
            if(*it == ci->ip) {
                it = mutedIPs.erase(it);
            } else {
                ++it;
            }
        }
    
        // Notify the unmuted player
        sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f2You have been unmuted by \f0%s", colorname(qs.getClient(CMD_SENDER)));
    
        // Notify the admin
        defformatstring(unmutemsg)("\f0%s \f7has been \f0unmuted (IP: %s)", colorname(ci), ci->ip);
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, unmutemsg);
        
        // Debug for logs
        printf("[DEBUG] Unmute removed for: %s (IP: %s). Entries removed: %lu. Remaining: %lu\n", 
               ci->name, ci->ip, (unsigned long)(initial_size - mutedIPs.size()), (unsigned long)mutedIPs.size());
    }
    
    QSERV_CALLBACK editmute_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found."); return; }
        if(cn == -2) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!"); return; }
        if(cn < 0 || cn >= MAXCLIENTS) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified."); return; }
        if(cn == CMD_SENDER) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot target yourself."); return; }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) { sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected"); return; }
    
        ci->isEditMuted = true;
        
        bool exists = false;
        for(const auto &ip : editMutedIPs) if(ip == ci->ip) { exists = true; break; }
        if(!exists) editMutedIPs.push_back(ci->ip);
    
        sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f7Your edits \f3will not \f7show up to others.");
        defformatstring(mutemsg)("\f0%s\f7's edits have been \f3muted", colorname(ci));
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, mutemsg);
        printf("Edit Mute applied to: %s (IP: %s). Total Muted IPs: %lu\n", ci->name, ci->ip, (unsigned long)editMutedIPs.size());
    }
    
    QSERV_CALLBACK uneditmute_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot target yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        sendf(ci->clientnum, 1, "ris", N_SERVMSG, "\f7Your edits \f0will \f7now show up to others.");
        ci->isEditMuted = false;
        for(auto it = editMutedIPs.begin(); it != editMutedIPs.end(); ) {
            if(*it == ci->ip) {
                it = editMutedIPs.erase(it);
            } else {
                ++it;
            }
        }
        out(ECHO_SERV, "\f0%s\f7's edits have been \f0unmuted", colorname(ci));
    }
    
    QSERV_CALLBACK forgive_cmd(p) {
        int cn = CMD_SENDER;
    
        if(CMD_SA) {
            const char* target_arg = args[1];
            if(isdigit(target_arg[0])) {
                cn = atoi(target_arg);
            } else {
                cn = GetClientNumByName(target_arg);
            }
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot forgive yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        clientinfo *self = qs.getClient(CMD_SENDER);
        if(!self) return;
    
        if(ci->state.teamkills >= 1) {
            defformatstring(forgivemsg)("\f0%s \f7has forgiven \f3%s", colorname(self), colorname(ci));
            sendf(-1, 1, "ris", N_SERVMSG, forgivemsg);
        } else {
            defformatstring(nk)("\f3Error: %s has not teamkilled anyone yet", colorname(ci));
            sendf(-1, 1, "ris", N_SERVMSG, nk);
        }
    }
    
    SVAR(ircoperators, "");
    
    QSERV_CALLBACK cheater_cmd(p) {
        if(!CMD_SA) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
            return;
        }
    
        const char* target_arg = args[1];
        int cn = -1;
    
        if(isdigit(target_arg[0])) {
            cn = atoi(target_arg);
        } else {
            cn = GetClientNumByName(target_arg);
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) { 
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot report yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        clientinfo *self = qs.getClient(CMD_SENDER);
        int accuracy = (ci->state.damage * 100) / max(ci->state.shotdamage, 1);
    
        privilegemsg(PRIV_MASTER, "\f7Something's fishy! A cheater has been reported.");
        
        out(ECHO_SERV, "\f0%s \f7accuses \f3%s \f7(CN: \f6%d \f7| Accuracy: \f6%d%%\f7) of cheating.", 
            colorname(self), colorname(ci), ci->clientnum, accuracy);
            
        out(ECHO_NOCOLOR, "Attention Operator(s): %s - %s accuses %s (CN: %d | Accuracy: %d%%) of cheating.", 
            ircoperators, colorname(self), colorname(ci), ci->clientnum, accuracy);
            
        defformatstring(nocolorcheatermsg)("\f3%s \f7has been reported.", colorname(ci));
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, nocolorcheatermsg);
    }
    
    QSERV_CALLBACK olangfilter_cmd(p) {
        if(CMD_SA) {
            int state = atoi(args[1]);
            if(state == 0 || state == 1) {
                qs.setoLang(state);
            } else {
                sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
            }
        } else {
            defformatstring(msg)("%s", (qs.isLangWarnOn() == 1) ? "On" : "Off");
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, msg);
        }
    }
        
    VAR(ircignore, 0, 0, 1);
    SVAR(contactemail, "");
    QSERV_CALLBACK callops_cmd(p) {
        if(!getvar("ircignore")) {
            out(ECHO_IRC, "[Attention operator(s)]: %s: %s is in need of assistance.", ircoperators, CMD_SCI.name);
            defformatstring(toclient)("You alerted IRC operator(s): %s", ircoperators);
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, toclient);
            loopv(clients) {
                clientinfo *ci = clients[i];
                if(ci && (ci->privilege == PRIV_ADMIN || ci->privilege == PRIV_MASTER) && ci->connected && ci->clientnum != CMD_SENDER) {
                    defformatstring(s)("\f6[Attention]: %s, %s is in need of assistance.", colorname(ci), CMD_SCI.name);
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, s);
                }
            }
        }
        else if(getvar("ircignore") == 1) {
            defformatstring(toclient)("\f7Admins have been notified. \nEmail: \f1%s \f7for more assistance.", contactemail);
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, toclient);
            out(ECHO_CONSOLE, "[Attention operator(s)]: %s: %s is in need of assistance.", ircoperators, CMD_SCI.name);
            loopv(clients) {
                clientinfo *ci = clients[i];
                if(ci && (ci->privilege == PRIV_ADMIN || ci->privilege == PRIV_MASTER) && ci->connected && ci->clientnum != CMD_SENDER) {
                    defformatstring(s)("\f6[Attention]: %s, %s is in need of assistance.", colorname(ci), CMD_SCI.name);
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, s);
                }
            }
        }
        else {
            defformatstring(toclient)("\f7Sorry, No operators are available currently. \nEmail: \f1%s \f7for more assistance.", contactemail);
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, toclient);
        }
    }
    
    SVAR(qserv_version, "");
    QSERV_CALLBACK getversion_cmd(p) {
        defformatstring(ver)("\f7Running \f3QServ \f7(\f2%s\f7): \f1www.github.com/deathstar/QServSqlite", qserv_version);
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, ver);
    }
    
    QSERV_CALLBACK forceintermission_cmd(p) {
    	if(server::gamemode == 1) return;
    	if(server::interm > 0) return;
    	//DO NOT execute start intermission as it causes a crash
    	//instead, we set gamelimit to current gamemillis
        //startintermission(); 
        server::gamelimit = server::gamemillis;
        defformatstring(msg)("\f0%s \f7started an intermission", CMD_SCI.name);
        sendf(-1, 1, "ris", N_SERVMSG, msg); 
        out(ECHO_IRC, "%s started an intermission", CMD_SCI.name);
    }

    QSERV_CALLBACK me_cmd(p) {
        if(strlen(fulltext) > 0) {
            qs.checkoLang(CMD_SENDER, fulltext);
            defformatstring(msg)("\f0%s \f7%s", CMD_SCI.name, fulltext);
            sendf(-1, 1, "ris", N_SERVMSG, msg);
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
    QSERV_CALLBACK echo_cmd(p) {
        if(strlen(fulltext) > 0) {
            out(ECHO_SERV, "%s", fulltext);
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
	QSERV_CALLBACK pm_cmd(p) {
	    // 1. Validation
	    if(strlen(fulltext) == 0) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
	        return;
	    }
	
	    // 2. Parse target and message
	    const char* message_start = strchr(fulltext, ' ');
	    if(!message_start) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Usage: /pm <cn/name> <message>");
	        return;
	    }
	
	    ::std::string target_str(fulltext, message_start - fulltext);
	    const char* privatemessage = message_start + 1;
	    while(*privatemessage == ' ') privatemessage++; // Skip leading spaces
	
	    if(*privatemessage == '\0') {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Cannot send an empty message.");
	        return;
	    }
	
	    // 3. Resolve target
	    int cn = isdigit(target_str[0]) ? atoi(target_str.c_str()) : GetClientNumByName(target_str.c_str());
	    
	    if(cn == -1) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
	        return;
	    }
	    if(cn == -2) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players with that name. Use their CN.");
	        return;
	    }
	    if(cn < 0 || cn >= MAXCLIENTS) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client.");
	        return;
	    }
	    if(cn == CMD_SENDER) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Cannot PM yourself.");
	        return;
	    }
	
	    // 4. Retrieve client data
	    clientinfo *ci = qs.getClient(cn);
	    clientinfo *self = qs.getClient(CMD_SENDER);
	
	    if(!ci || !ci->connected) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected.");
	        return;
	    }
	
	    // 5. Send messages using defformatstring to fix the %s rendering issue
	    defformatstring(reciever_msg)("\f7Private message from \f0%s\f7: \f3%s", colorname(self), privatemessage);
	    sendf(cn, 1, "ris", N_SERVMSG, reciever_msg);
	
	    defformatstring(sender_msg)("\f7Sent message to \f0%s", colorname(ci));
	    sendf(CMD_SENDER, 1, "ris", N_SERVMSG, sender_msg);
	}
    
    QSERV_CALLBACK sendprivs_cmd(p) {
        if(!CMD_SA || !args[1]) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
            return;
        }
    
        const char* target_arg = args[1];
        int cn = -1;
    
        if(isdigit(target_arg[0])) {
            cn = atoi(target_arg);
        } else {
            cn = GetClientNumByName(target_arg);
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified.");
            return;
        }
    
        if(cn == CMD_SENDER) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: You cannot share privileges with yourself.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        if(getvar("enablemultiplemasters") != 1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: This server has disabled the multiple masters feature");
            return;
        }
    
        clientinfo *self = qs.getClient(CMD_SENDER);
        if(!self) return;
    
        defformatstring(shareprivsmsg)("\f7Ok, %s\f7. Sharing your privileges with \f0%s\f7.", colorname(self), colorname(ci));
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, shareprivsmsg);
    
        if(self->privilege == PRIV_MASTER) {
            defformatstring(sendprivsmsg)("\f7You have received \f0master \f7from \f0%s\f7.", colorname(self));
            sendf(cn, 1, "ris", N_SERVMSG, sendprivsmsg);
            server::setmaster(ci, 1, "", NULL, NULL, PRIV_MASTER, true, false);
        }
        else if(self->privilege == PRIV_ADMIN) {
            defformatstring(sendprivsmsg)("\f7You have received \f6admin \f7from \f6%s\f7.", colorname(self));
            sendf(cn, 1, "ris", N_SERVMSG, sendprivsmsg);
            server::setmaster(ci, 1, "", NULL, NULL, PRIV_ADMIN, true, false);
        }
    }
    
	QSERV_CALLBACK help_cmd(p)
	{
	    if(CMD_SA)
	    {
	        int lastcmd = -1;
	
	        if(args[1][0])
	        {
	            char command[64];
	            snprintf(command, sizeof(command), "%c%s",
	                     commandprefix, args[1]);
	
	            for(int i = 0; i < CMD_LAST; ++i)
	            {
	                if(!strcmp(CMD_NAME(i), command))
	                {
	                    lastcmd = i;
	                    break;
	                }
	            }
	        }
	
	        if(lastcmd < 0)
	        {
	            sendf(CMD_SENDER, 1, "ris",
	                N_SERVMSG,
	                "\f3Error: Command not found.\n"
	                "Use \f2\"#help\" \f3for a list of commands or "
	                "\f2\"#help <command>\" \f3for usage.");
	            return;
	        }
	
	        if(CMD_SCI.privilege < qs.getCommandPriv(lastcmd))
	        {
	            sendf(CMD_SENDER, 1, "ris",
	                N_SERVMSG,
	                "\f3Error: Insufficient permissions to view command info");
	            return;
	        }
	
	        sendf(CMD_SENDER, 1, "ris",
	            N_SERVMSG,
	            CMD_DESC(lastcmd));
	
	        return;
	    }
	
	    clientinfo *ci = qs.getClient(CMD_SENDER);
	
	    if(!ci)
	    {
	        sendf(CMD_SENDER, 1, "ris",
	            N_SERVMSG,
	            "\f3Error: Client not found");
	        return;
	    }
	
	    char commandList[4096];
	    size_t pos = 0;
	
	    pos += snprintf(commandList + pos,
	                    sizeof(commandList) - pos,
	                    "\f2Commands: \f7");
	
	    bool first = true;
	
	    for(int i = 0; i < CMD_LAST; ++i)
	    {
	        bool visible = false;
	
	        switch(ci->privilege)
	        {
	            case PRIV_ADMIN:
	                visible =
	                    CMD_PRIV(i) == PRIV_NONE ||
	                    CMD_PRIV(i) == PRIV_MASTER ||
	                    CMD_PRIV(i) == PRIV_ADMIN;
	                break;
	
	            case PRIV_MASTER:
	                visible =
	                    CMD_PRIV(i) == PRIV_NONE ||
	                    CMD_PRIV(i) == PRIV_MASTER;
	                break;
	
	            default:
	                visible =
	                    CMD_PRIV(i) == PRIV_NONE;
	                break;
	        }
	
	        if(!visible)
	            continue;
	
	        int written = snprintf(
	            commandList + pos,
	            sizeof(commandList) - pos,
	            "%s%s",
	            first ? "" : " ",
	            CMD_NAME(i)
	        );
	
	        if(written < 0 ||
	           static_cast<size_t>(written) >= sizeof(commandList) - pos)
	        {
	            break;
	        }
	
	        pos += written;
	        first = false;
	    }
	
	    sendf(CMD_SENDER, 1, "ris",
	          N_SERVMSG,
	          commandList);
	}
    
	#include <iostream>
	#include <string>
	#include <stdio.h>
	#include <time.h>
	QSERV_CALLBACK localtime_cmd(p) {
    	time_t rawtime; 
    	time(&rawtime);
    	char *time_str = ctime(&rawtime);

    	// ctime returns a pointer to a static string; check if it exists
    	if(time_str) {
        	// Strip the trailing newline character usually added by ctime
        	size_t len = strlen(time_str);
        	if(len > 0 && time_str[len-1] == '\n') time_str[len-1] = '\0';

        	defformatstring(localtime_str)("Local server time: %s", time_str);
        	sendf(CMD_SENDER, 1, "ris", N_SERVMSG, localtime_str);
    	} else {
        	sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "Error: Could not retrieve local server time.");
    	}
	}

	QSERV_CALLBACK time_cmd(p) {
    	if(CMD_SA) {
        	int UTCOffset = atoi(args[1]);
        	time_t rawtime;
        	time(&rawtime);
        	struct tm *ptm = gmtime(&rawtime);

        	// gmtime returns NULL if the time cannot be represented
        	if(ptm) {
            	// Safe hour calculations
            	int std_hour = (ptm->tm_hour + UTCOffset) % 24;
            	if(std_hour < 0) std_hour += 24;
            
            	int dst_hour = (ptm->tm_hour + UTCOffset + 1) % 24;
            	if(dst_hour < 0) dst_hour += 24;
    
            	defformatstring(TimeOffset)("Time for UTC (%d): %02d:%02d", UTCOffset, std_hour, ptm->tm_min);
            	defformatstring(TimeOffsetDST)(" | DST: %02d:%02d", dst_hour, ptm->tm_min);
            
            	sendf(CMD_SENDER, 1, "ris", N_SERVMSG, concatstring(TimeOffset, TimeOffsetDST));
        	} else {
            	sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "Error: Could not retrieve UTC time.");
        	}
    	} else {
        	sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
    	}
	}

    QSERV_CALLBACK bunny_cmd(p) {
        if(strlen(fulltext) > 0) {
            defformatstring(msg)("%s \f0%s: \f7%s", bunny, "Tip", fulltext);
            sendf(-1, 1, "ris", N_SERVMSG, msg);
        } else {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
        }
    }
    
	QSERV_CALLBACK stats_cmd(p) {
	    int cn = CMD_SENDER; 
	
	    if(CMD_SA && args[1]) {
	        const char* target_arg = args[1];
	        if(isdigit(target_arg[0])) {
	            cn = atoi(target_arg);
	        } else {
	            cn = GetClientNumByName(target_arg);
	        }
	    }
	
	    if(cn == -1) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
	        return;
	    }
	    if(cn == -2) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
	        return;
	    }
	    if(cn < 0 || cn >= MAXCLIENTS) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified.");
	        return;
	    }
	
	    clientinfo *ci = qs.getClient(cn);
	    if(!ci || !ci->connected) {
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected.");
	        return;
	    }
	
	    string buf;
	    char msg[MAXTRANS];
	    formatstring(msg)("\f7Stats for \f0%s\f7: cn: \f2%d \f7frags: \f2%i \f7deaths: \f2%i \f7suicides: \f2%i \f7kpd: \f2%.2f \f7acc: \f2%i%%",
	                      colorname(ci), ci->clientnum, ci->state.frags, ci->state.deaths, ci->state._suicides,
	                      (float(ci->state.frags)/float(max(ci->state.deaths, 1))), ci->state.damage*100/max(ci->state.shotdamage,1));
	    
	    if(server::q_teammode) {
	        snprintf(buf, sizeof(buf), "\n\f7teamkills: \f2%i \f7flags scored: \f2%i \f7flags stolen: \f2%i \f7flags returned: \f2%i", 
	                 ci->state.teamkills, ci->state.flags, ci->state._stolen, ci->state._returned);
	        concatstring(msg, buf, MAXTRANS);
	    }
	    
	    snprintf(buf, sizeof(buf), "\n\f7shotgun: \f2%i%% \f7chaingun: \f2%i%% \f7rocketlauncher: \f2%i%% \f7rifle: \f2%i%% \f7grenadelauncher: \f2%i%% \f7pistol: \f2%i%%",
	             getwepaccuracy(ci->clientnum, 1), getwepaccuracy(ci->clientnum, 2), getwepaccuracy(ci->clientnum, 3),
	             getwepaccuracy(ci->clientnum, 4), getwepaccuracy(ci->clientnum, 5), getwepaccuracy(ci->clientnum, 6));
	    concatstring(msg, buf, MAXTRANS);
	    
	    send_connected_time(ci, CMD_SENDER);
	    sendf(CMD_SENDER, 1, "ris", N_SERVMSG, msg);
	
	    // thread-Safe Geo-IP Lookup
	    if(qs.enable_HTTP_geo && strcmp("127.0.0.1", ci->ip) != 0) {
	        int sender_cn = CMD_SENDER;
	        ::std::string target_ip(ci->ip);
	        
	        ::std::thread([sender_cn, target_ip]() {
	            try {
	                http::Request req("http://ip-api.com/line/" + target_ip + "?fields=city,regionName,country");
	                const http::Response res = req.send("GET");
	                ::std::string s(res.body.begin(), res.body.end());
	                RString(s, "\n", " > ");
	                UTFEncode(s);
	                while(!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '>')) s.pop_back();
	                if(s.length() >= 2 && s.substr(s.length() - 2) == " >") s.erase(s.length() - 2);
	                
	                // Pre-format to ensure all variables are resolved
	                defformatstring(async_msg)("\f7location: \f6%s", s.c_str());
	            
	                std::lock_guard<std::mutex> lock(client_mutex);
	                clientinfo *sender = qs.getClient(sender_cn);
	                if(sender && sender->connected) {
	                    sendf(sender_cn, 1, "ris", N_SERVMSG, async_msg);
	                }
	            } catch (...) { return; }
	        }).detach();
	    } else {
	        ::std::string location_str = qs.cgip(ci->ip); 
	        const char *loc = location_str.c_str();
	        
	        defformatstring(lmsg)("\f7location: \f6%s", (loc && loc[0] ? loc : "Unknown Location"));
	        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, lmsg);
	    }
	}

    QSERV_CALLBACK revokepriv_cmd(p) {
        if(!CMD_SA || !args[1]) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, CMD_DESC(cid));
            return;
        }
    
        const char* target_arg = args[1];
        int cn = -1;
    
        if(isdigit(target_arg[0])) {
            cn = atoi(target_arg);
        } else {
            cn = GetClientNumByName(target_arg);
        }
    
        if(cn == -1) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player name not found.");
            return;
        }
        if(cn == -2) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Multiple players have that name. Use their CN instead!");
            return;
        }
    
        if(cn < 0 || cn >= MAXCLIENTS) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Invalid client number specified. Use \f2/showclientnum 1 \f3then press tab.");
            return;
        }
    
        clientinfo *ci = qs.getClient(cn);
        if(!ci || !ci->connected) {
            sendf(CMD_SENDER, 1, "ris", N_SERVMSG, "\f3Error: Player not connected");
            return;
        }
    
        defformatstring(msg)("Privileges have been revoked from the specified client \f3%s", colorname(ci));
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, msg);
        
        setmaster(ci, NULL, "", NULL, NULL, PRIV_NONE, true, false);
    }
    
    /*QSERV_CALLBACK owords_cmd(p) {
        char owordList[1024] = "Offensive words (words not to say): ";
        char colors[10];
        for(int i = 0; i < 50; i++) {
            if(strlen(owords[i]) > 0) {
                snprintf(colors, sizeof(colors), "\f%d", 3);
                strcat(owordList, colors);
                strcat(owordList, owords[i]);
                strcat(owordList, "\f2, ");
            }
        }
        owordList[strlen(owordList)-2] = '\0';
        sendf(CMD_SENDER, 1, "ris", N_SERVMSG, owordList);
    }*/
}



