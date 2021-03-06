#include <stdio.h>
#include <iostream>
#include <map>
#include <queue>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include "./chat_system.h"
#include <ifaddrs.h>
#include <thread>

struct sockaddr_in sListeningAddr;
struct sockaddr_in sRecAddr;
int iRecAddrLen;
int iListeningSocketFd, iSendingSocketFd;
int iListeningPortNum = 0;
std::string username;
bool is_server, is_server_alive, declare_leader, leader_already_declared;
std::queue<msg_struct *> qpsBroadcastq;
std::list<msg_struct *> lpsClientInfo;
std::list<sockaddr_in *> lpsClients;
std::map<int, msg_struct *> holdbackMap;
std::map<int, msg_struct *> sentBufferMap;
std::map<int, msg_struct *> broadcastBufferMap;
std::list<int> liCurrentClientPort; 
int iSeqNum = 0, iExpSeqNum = 0;
int iMsgId = 0, iResponseCount = 0;
std::mutex seqNumMutex;
std::mutex msgIdMutex;
std::mutex broadcastMutex;
std::mutex clientListMutex;
std::mutex broadcastbufferMutex;
std::mutex sentbufferMutex;
std::mutex heartbeatMutex;
std::mutex newLeaderElectedMutex;
msg_struct sServerInfo, sMyInfo;
sockaddr_in sServerAddr;
int P_value = 0;
int A_value = 0;
std::map<int, std::list<int >> MapmsgId_propTS;   
std::list<msg_struct *> hbQ; 

void get_ip_address(char * ip);
void user_listener();
void msg_listener();
void check_ack_sb(int time_diff_sec);
void broadcast_message();
void heartbeat();

int main(int argc, char ** argv)
{
    char acBufferLocal[BUFF_SIZE];
    std::string token;
    struct sockaddr_in sConnectingAddr;
    msg_struct * psMsgStruct;
    sockaddr_in * psSockAddr;
    char acTemp[50];
    struct sockaddr_in temp;
    int addrlen = sizeof(temp);

    if(argc != 2 && argc != 3)
    {
        fprintf(stderr,"Incorrect number of arguments supplied. Please retry\n");
        exit(1);
    }


    iRecAddrLen = sizeof(sRecAddr);

    /* Establishing listener socket */

    sListeningAddr.sin_family = AF_INET;
    sListeningAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sListeningAddr.sin_port = htons(iListeningPortNum);

    iListeningSocketFd = socket(AF_INET, SOCK_DGRAM, 0);

    if (iListeningSocketFd < 0)
    {
        fprintf(stderr, "Error while opening listening socket\n");
        exit(1);
    }

    while(!iListeningPortNum)
    {
        if (bind(iListeningSocketFd, (struct sockaddr *) &sListeningAddr, sizeof(sListeningAddr)) < 0)
        {
            fprintf(stderr, "Error while binding listening socket\n");
            exit(1);
        }

        if(getsockname(iListeningSocketFd, (struct sockaddr *) &temp, (socklen_t *) &addrlen) == 0 &&
           temp.sin_family == AF_INET && addrlen == sizeof(temp))
        {
            iListeningPortNum = ntohs(temp.sin_port);
        }
    }

    printf("\nThis user listens to port number '%d'\n", iListeningPortNum);

    //get_ip_address(acTemp);
    //if(NULL == acTemp)
        strcpy(acTemp, "127.0.0.1");

    /* Storing my info in sMyInfo struct */
    sMyInfo.name = username;
    sMyInfo.ipAddr = acTemp;
    sMyInfo.port = iListeningPortNum;
    sMyInfo.addr = &sListeningAddr;

    /* Establishing sender socket */
    
    iSendingSocketFd = socket(AF_INET, SOCK_DGRAM, 0);

    if (iSendingSocketFd < 0)
    {
        fprintf(stderr, "Error while opening sender socket\n");
        exit(1);
    }

    /* If initiating a new chat */
    if(2 == argc)
    {
        

        /* Set username to what's being passed as an arg */
        sockaddr_in * psAddr = new sockaddr_in;//();
        if(psAddr == NULL)
        {
            fprintf(stderr, "Malloc failed. Please retry\n");
            exit(1);
        }
        psAddr->sin_family = AF_INET;
        if(inet_pton(AF_INET, acTemp, &(psAddr->sin_addr)) <= 0)
        {
            fprintf(stderr, "Error while storing the IP address. Please retry\n");
            exit(1);
        }
        psAddr->sin_port = htons(iListeningPortNum);

        //psClientInfo = (msg_struct *) malloc(sizeof(msg_struct));
        msg_struct * psClientInfo = new msg_struct;//();
        if(psClientInfo == NULL)
        {
            fprintf(stderr, "Malloc failed. Please retry\n");
            exit(1);
        }
        psClientInfo->name = argv[1];
        psClientInfo->ipAddr = acTemp;
        psClientInfo->port = iListeningPortNum;

        /* Insert it into the two client lists */
        clientListMutex.lock();
        lpsClients.push_back(psAddr);
        lpsClientInfo.push_back(psClientInfo);
        clientListMutex.unlock();
        username = argv[1];

        /* Start all the threads */
        std::thread user_listener_thread(user_listener);
        std::thread msg_listener_thread(msg_listener);
        std::thread broadcast_message_thread(broadcast_message);
        std::thread heartbeat_thread(heartbeat);

        /* Wait for threads to join */
        user_listener_thread.join();
        msg_listener_thread.join();
        broadcast_message_thread.join();
        heartbeat_thread.join();

    }

    /* If connecting to an already present chat system */
    else
    {
       

        username = argv[1];

        sConnectingAddr.sin_family = AF_INET;

        token = strtok(argv[2]," :");
        if(NULL == token.c_str())
        {
            fprintf(stderr, "The IP address specified is not valid. Please retry\n");
            exit(1);
        }

        if(inet_pton(AF_INET, token.c_str(), &sConnectingAddr.sin_addr) <= 0)
        {
            fprintf(stderr, "Error while storing the IP address. Please retry\n");
            exit(1);
        }


        token = strtok(NULL, " :");
        if(NULL == token.c_str())
        {
            fprintf(stderr, "The port specified is not valid. Please retry\n");
            exit(1);
        }

        if(-1 == (int) strtol(token.c_str(), NULL, 10))
        {
            fprintf(stderr, "The port specified is not valid. Please retry\n");
            exit(1);
        }

        sConnectingAddr.sin_port = htons( (int) strtol(token.c_str(), NULL, 10) );


        sprintf(&acBufferLocal[MSG_TYPE], "%d", (int) messageType::REQ_CONNECTION);
        strcpy(&acBufferLocal[NAME], username.c_str());
        sprintf(&acBufferLocal[DATA], "%d", iListeningPortNum);
        sendto(iSendingSocketFd, acBufferLocal, BUFF_SIZE, 0,
            (struct sockaddr *) &sConnectingAddr, sizeof(sockaddr_in));

       

        /* Start all the threads */
        std::thread user_listener_thread(user_listener);
        std::thread msg_listener_thread(msg_listener);
        std::thread broadcast_message_thread(broadcast_message);
        std::thread heartbeat_thread(heartbeat);
        
        user_listener_thread.join();
        msg_listener_thread.join();
        broadcast_message_thread.join();
        heartbeat_thread.join();
    }
    return 0;
}

void get_ip_address(char * ip)
{
    struct ifaddrs * sIfAddr = NULL;
    struct ifaddrs * sIterator = NULL;
    void * pvTemp = NULL;

    strcpy(ip, "");
    getifaddrs(&sIfAddr);

    for(sIterator = sIfAddr; sIterator != NULL; sIterator = sIterator->ifa_next)
    {
        if(!sIterator->ifa_addr)
        {
            continue;
        }

        if( (sIterator->ifa_addr->sa_family == AF_INET) &&
                !(strcmp(sIterator->ifa_name, "em1")))
        {
            pvTemp = &((struct sockaddr_in *) sIterator->ifa_addr)->sin_addr;
            char acAddrBuff[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, pvTemp, acAddrBuff, INET_ADDRSTRLEN);
            strcpy(ip, acAddrBuff);
        }
    }
    if(sIfAddr != NULL)
        freeifaddrs(sIfAddr);

    return;
}
