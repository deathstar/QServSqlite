#ifndef __IRCBOT_INCLUDED
#define __IRCBOT_INCLUDED

#include <cstring> 
#include "game.h"

struct IrcMsg
{
    char nick[64];
    char user[64];
    char host[64];
    char chan[64];
    char message[1000];

    int is_ready;
};

extern volatile bool irc_running;

class ircBot
{
    public:
        void init(void);

        int getSock();

        int speak(const char *fmt, ...);
        
        bool safeSend(const char *data);

        bool HandlePing(char *buff);

        bool IsCommand(char *buff);

        void join(char *channel);

        void part(char *channel);

        void notice(char *user, const char *message);

        IrcMsg *lastmsg();

        bool isConnected();

        hashtable<char *, int> IRCusers;

    private:
        void ParseMessage(char *buff);

        int sock = -1;

        IrcMsg msg;

        bool connected = false;
};

extern ircBot irc;

struct IRCCommand
{
    char command[512];
};

extern vector<IRCCommand> ircCommandQueue;

void queueIRCCommand(const char *cmd);

void processIRCCommands();

extern bool isloggedin(bool echo = 1);

#endif ///__IRCBOT_INCLUDED
