#ifndef WIN32
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#endif

#include <stdio.h>
#include <string.h>

#include "ircbot.h"
#include "../mod/QServ.h"

#include <pthread.h>

vector<IRCCommand> ircCommandQueue;

pthread_mutex_t ircCommandMutex = PTHREAD_MUTEX_INITIALIZER;

void queueIRCCommand(const char *cmd)
{
    if(!cmd || !cmd[0]) return;

    IRCCommand q;

    copystring(q.command, cmd);

    pthread_mutex_lock(&ircCommandMutex);

    ircCommandQueue.add(q);

    pthread_mutex_unlock(&ircCommandMutex);
}

void processIRCCommands()
{
    vector<IRCCommand> localQueue;
    // 2. Lock just long enough to copy the data out
    pthread_mutex_lock(&ircCommandMutex);
    
    loopv(ircCommandQueue)
    {
        localQueue.add(ircCommandQueue[i]);
    }
    ircCommandQueue.setsize(0); // Clear the shared queue immediately
    
    pthread_mutex_unlock(&ircCommandMutex); 

    // 3. Now execute the commands safely without holding the mutex lock
    loopv(localQueue)
    {
        IRCCommand &q = localQueue[i];
        if(strlen(q.command) > 0 && strlen(q.command) < 400)
        {
            conoutf("[IRC CMD] %s", q.command);
            execute(q.command); 
        }
    }
}

SVAR(irchost, "irc.gamesurge.net");
VAR(ircport, 0, 6667, 65535);
VAR(ircignore, 0, 0, 1);

SVAR(ircchan, "#QServ");
SVAR(ircbotname, "QServ");
SVAR(ircloginpass, "default");

ircBot irc;

bool isloggedin(bool echo)
{
    if(irc.IRCusers.access(irc.lastmsg()->host))
        return true;

    if(echo)
        irc.notice(irc.lastmsg()->nick, "Error: Insufficient Permission");

    return false;
}

ICOMMAND(login, "s", (char *s), {
    if(isloggedin(0))
    {
        irc.notice(irc.lastmsg()->nick, "You are already logged in!");
        return;
    }

    if(!strcmp(s, ircloginpass))
    {
        irc.IRCusers[irc.lastmsg()->host] = 1;

        irc.speak("%s has logged in", irc.lastmsg()->nick);

        out(
            ECHO_SERV,
            "\f0%s \f7has logged in thru IRC (%s \f3%s\f7)",
            irc.lastmsg()->nick,
            irchost,
            ircchan
        );
    }
    else
    {
        irc.notice(
            irc.lastmsg()->nick,
            "Error: Invalid Password"
        );
    }
});

ICOMMAND(clearbans, "", (), {
    if(isloggedin())
        server::clearbans();
});

ICOMMAND(forceintermission, "", (), {
    if(isloggedin())
        server::startintermission();
});

ICOMMAND(join, "s", (char *s), {
    if(isloggedin())
        irc.join(s);
});

ICOMMAND(part, "s", (char *s), {
    if(isloggedin())
        irc.part(s);
});

ICOMMAND(kick, "i", (int *i), {
    if(isloggedin())
        disconnect_client(*i, DISC_KICK);
});

IrcMsg *ircBot::lastmsg()
{
    return &msg;
}

int ircBot::getSock()
{
    return sock;
}

bool ircBot::isConnected()
{
    return connected;
}

bool ircBot::safeSend(const char *data)
{
    if(sock < 0 || !connected || !data)
        return false;

#ifdef __APPLE__
    int set = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
#endif

    int result = send(sock, data, strlen(data), 0);

    if(result <= 0)
    {
        connected = false;
        return false;
    }

    return true;
}

int ircBot::speak(const char *fmt, ...)
{
    char message[1000];
    char formatted[900];

    va_list list;

    va_start(list, fmt);

    vsnprintf(
        formatted,
        sizeof(formatted),
        fmt,
        list
    );

    va_end(list);

    snprintf(
        message,
        sizeof(message),
        "PRIVMSG %s :%s\r\n",
        ircchan,
        formatted
    );

    return safeSend(message);
}

void ircBot::join(char *channel)
{
    defformatstring(joinchan)(
        "JOIN %s\r\n",
        channel
    );

    safeSend(joinchan);
}

void ircBot::part(char *channel)
{
    defformatstring(partchan)(
        "PART %s\r\n",
        channel
    );

    safeSend(partchan);
}

void ircBot::notice(char *user, const char *message)
{
    defformatstring(noticeuser)(
        "NOTICE %s :%s\r\n",
        user,
        message
    );

    safeSend(noticeuser);
}

void ircBot::ParseMessage(char *buff)
{
    if(!buff || strlen(buff) >= 1000)
    {
        msg.is_ready = 0;
        return;
    }

    if(sscanf(
        buff,
        ":%[^!]!%[^@]@%[^ ] %*[^ ] %[^ :] :%[^\r\n]",
        msg.nick,
        msg.user,
        msg.host,
        msg.chan,
        msg.message
    ) == 5)
    {
        msg.is_ready = 1;

        if(msg.chan[0] != '#')
            strcpy(msg.chan, msg.nick);
    }
    else
    {
        msg.is_ready = 0;
    }
}

bool ircBot::HandlePing(char *buff)
{
    if(!strncmp(buff, "PING :", 6))
    {
        char pong[512];

        snprintf(
            pong,
            sizeof(pong),
            "PONG :%s\r\n",
            buff + 6
        );

        printf(">> %s", pong);

        return safeSend(pong);
    }

    return false;
}

bool ircBot::IsCommand(char *buff)
{
    if(!buff || !buff[0])
        return false;

    if(HandlePing(buff))
        return true;

    ParseMessage(buff);

    if(!msg.is_ready)
        return false;

    if(msg.message[0] == '#' || msg.message[0] == '@')
    {
        char *c = msg.message + 1;

        if(strlen(c) > 0 && strlen(c) < 400)
        {
            conoutf("%s", c);

            queueIRCCommand(c);;
        }

        return true;
    }

    return false;
}

void ircBot::init()
{
    if(getvar("ircignore")) return;
#ifndef WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    time_t last_attempt = 0;

    while(true) 
    {
        // Non-blocking timer: enforce 5-second wait between attempts
        if (time(NULL) - last_attempt < 5) 
        {
            // Small sleep here (100ms) prevents 100% CPU usage while waiting
#ifdef WIN32
            Sleep(100); 
#else
            usleep(100000);
#endif
            continue; 
        }
        last_attempt = time(NULL);

        connected = false;
        printf("[IRC] Connecting to %s:%d...\n", irchost, ircport);

        struct sockaddr_in sa;
        struct hostent *he = gethostbyname(irchost);
        if(!he) { printf("[IRC] Failed resolving host\n"); continue; }

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock < 0) { printf("[IRC] Socket creation failed\n"); continue; }

        // Set timeout
        timeval timeout = {300, 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(ircport);
#ifdef _WIN32
        memcpy((char *)&sa.sin_addr.s_addr, *he->h_addr_list, sizeof(sa.sin_addr.s_addr));
#else
        bcopy(*he->h_addr_list, (char *)&sa.sin_addr.s_addr, sizeof(sa.sin_addr.s_addr));
#endif

        if(connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
        {
            printf("[IRC] Connect failed\n");
            if(sock >= 0) { close(sock); sock = -1; }
            continue;
        }

        connected = true;
        printf("[IRC] Connected\n");

        // Authenticate
        defformatstring(user)("USER %s 0 * :%s\r\n", ircbotname, ircbotname);
        safeSend(user);
        defformatstring(nick)("NICK %s\r\n", ircbotname);
        safeSend(nick);

		char recvbuf[2048];
        char linebuf[8192];
        int linepos = 0;
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(linebuf, 0, sizeof(linebuf));
        
        while(connected)
        {
            int len = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);
            if(len <= 0) break;
            recvbuf[len] = '\0';
        
            for(int i = 0; i < len; i++)
            {
                if(linepos < (int)sizeof(linebuf) - 1)
                    linebuf[linepos++] = recvbuf[i];
        
                if(recvbuf[i] == '\n')
                {
                    linebuf[linepos] = '\0';
                    
                    printf("%s", linebuf); 
        
                    if(strstr(linebuf, " 001 ")) { /* ... JOIN ... */ }
                    if(!IsCommand(linebuf) && msg.is_ready) { /* ... */ }
        
                    linepos = 0; 
                }
            }
        }
        if(sock >= 0) { close(sock); sock = -1; }
    }
}
