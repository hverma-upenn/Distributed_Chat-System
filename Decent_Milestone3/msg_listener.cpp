#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <queue>
#include "./chat_system.h"
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <cstring>
#include <algorithm>

int process_rec_msg(char * acBuffer);
void get_msg_from_bbm(int seqNum, char * name, char * data);
void check_hbm_and_display();
void display(msg_struct * msg);
void update_client_list(msg_struct * msg);
void display_client_list();
void check_ack_sb(int time_diff_sec);
void flush_dead_clients(std::list<int> deadclients);
void process_hbq(int timestamp, int message_id, int senderPort);
bool hbComp(const msg_struct* lhs, const msg_struct* rhs);

bool is_client_already_present(std::string name)
{
    for (std::list<msg_struct *>::iterator it = lpsClientInfo.begin();
            it != lpsClientInfo.end(); it++)
    {
        if(((*it)->name).compare(name) == 0)
            return true;
    }
    return false;
}

void msg_listener()
{
    int iRecLen = 0, iLenToBeSent = 0;
    char acBuffer[BUFF_SIZE] = "\0";

    while(true)
    {   
        memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
        iRecLen = recvfrom(iListeningSocketFd, acBuffer, BUFF_SIZE, 0,
                (struct sockaddr *) &sRecAddr, (socklen_t*) &iRecAddrLen);
        
        if (iRecLen < 0)
        {
            fprintf(stderr, "Error while receiving from the listening socket\n");
            exit(1);
        }

        iLenToBeSent = process_rec_msg(acBuffer);

        if(iLenToBeSent != 0)
        {
            sendto(iSendingSocketFd, acBuffer, iLenToBeSent, 0,
                    (struct sockaddr *) &sRecAddr, iRecAddrLen);
        }
    }
}

int process_rec_msg(char * acBuffer)
{
    msg_struct msg, * psMsg;
    //msg.addr = (sockaddr_in *) malloc(sizeof(sockaddr_in));
    msg.msgType = (messageType) atoi(acBuffer);
    msg.addr = &sRecAddr;
    msg.name = &acBuffer[NAME];
    msg.senderPort = atoi(&acBuffer[SENDER_LISTENING_PORT]);
    msg.data = &acBuffer[DATA];
    msg.seqNum = atoi(&acBuffer[SEQ_NUM]);
    msg.msgId = atoi(&acBuffer[MSG_ID]);
    sockaddr_in * psAddr;
    msg_struct * psClientInfo, * sMsg;
    int iTempIndex = 0;
    int iLenToBeSent = 0;
    char acTempStr[100] = "\0";
    switch(msg.msgType)
    {
        case REQ_CONNECTION:
            {
                    if(!is_client_already_present(msg.name))
                    {
                        /* Create the sockaddr_in struct and msg_struct struct
                         * to be inserted into the two clients list */
                        //psAddr = (sockaddr_in *) malloc(sizeof(sockaddr_in));
                        psAddr = new sockaddr_in;//();
                        if(psAddr == NULL)
                        {
                            fprintf(stderr, "Malloc failed. Please retry\n");
                            break;
                        }
                        psAddr->sin_family = AF_INET;
                        (psAddr->sin_addr).s_addr = sRecAddr.sin_addr.s_addr;
                        psAddr->sin_port = htons(atoi(&acBuffer[DATA]));

                        //psClientInfo = (msg_struct *) malloc(sizeof(msg_struct));
                        psClientInfo = new msg_struct;//();
                        if(psClientInfo == NULL)
                        {
                            fprintf(stderr, "Malloc failed. Please retry\n");
                            break;
                        }
                        psClientInfo->name = &acBuffer[NAME];
                        inet_ntop(AF_INET, &(sRecAddr.sin_addr), acTempStr, INET_ADDRSTRLEN);
                        psClientInfo->ipAddr = acTempStr;
                        psClientInfo->port = atoi(&acBuffer[DATA]);

                        /* Insert it into the two client lists */
                        clientListMutex.lock();
                        lpsClients.push_back(psAddr);
                        lpsClientInfo.push_back(psClientInfo);
                        clientListMutex.unlock();

                        /* Insert NEW_CLIENT_INFO msg to broadcast queue */
                        //psMsg = (msg_struct *) malloc(sizeof(msg_struct));
                        psMsg = new msg_struct;//();
                        if(psMsg == NULL)
                        {
                            fprintf(stderr, "Malloc failed. Please retry\n");
                            break;
                        }
                        psMsg->msgType = messageType::NEW_CLIENT_INFO;
                        sprintf(acTempStr, "%s joined the chat on %s:%d, listening on %s:%d\n",
                                (psClientInfo->name).c_str(), sMyInfo.ipAddr.c_str(),
                                sMyInfo.port, psClientInfo->ipAddr.c_str(), psClientInfo->port);
                        psMsg->data = acTempStr;
                        broadcastMutex.lock();
                        qpsBroadcastq.push(psMsg);
                        broadcastMutex.unlock();

                        /* Insert CLIENT_LIST msg to broadcast queue */
                        //psMsg = (msg_struct *) malloc(sizeof(msg_struct));
                        psMsg = new msg_struct;//();
                        if(psMsg == NULL)
                        {
                            fprintf(stderr, "Malloc failed. Please retry\n");
                            break;
                        }
                        psMsg->msgType = messageType::CLIENT_LIST;
                        broadcastMutex.lock();
                        qpsBroadcastq.push(psMsg);
                        broadcastMutex.unlock();

                        /* Send CONNECTION_ESTABLISHED to client */
                        sRecAddr.sin_port = htons(atoi(&acBuffer[DATA]));
                        memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                        sprintf(&acBuffer[MSG_TYPE], "%d", (int) messageType::CONNECTION_ESTABLISHED);
                        strcpy(&acBuffer[NAME], username.c_str());
                        seqNumMutex.lock();
                        sprintf(&acBuffer[SEQ_NUM], "%d", iSeqNum);
                        seqNumMutex.unlock();
                        iLenToBeSent = BUFF_SIZE;
                    }
                
               
                break;
            }

        case CONNECTION_ESTABLISHED:
            {
                
                    //sServerInfo.name = msg.name;
                    A_value = msg.seqNum;
                    iLenToBeSent = 0;
                break;
            }


        /*case CHAT:
            {
                if(is_server)
                {
          
                    psMsg = new msg_struct;//();
                    if(psMsg == NULL)
                    {
                        fprintf(stderr, "Malloc failed. Please retry\n");
                        break;
                    }
                    psMsg->msgType = messageType::MSG;
                    seqNumMutex.lock();
                    psMsg->seqNum = iSeqNum;
                    iSeqNum++;
                    seqNumMutex.unlock();
                    psMsg->name = msg.name;
                    psMsg->data = msg.data;

                     Push the msg to the broadcast queue 
                    broadcastMutex.lock();
                    qpsBroadcastq.push(psMsg);
                    broadcastMutex.unlock();

                    sRecAddr.sin_port = htons(atoi(&acBuffer[SENDER_LISTENING_PORT]));
                    memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                    sprintf(&acBuffer[MSG_TYPE], "%d", (int)messageType::ACK);
                    sprintf(&acBuffer[MSG_ID], "%d", msg.msgId);
                    iLenToBeSent = BUFF_SIZE;
                }
                break;
            }*/

        case MSG:
            {
                psMsg = new msg_struct;//();
                if(psMsg == NULL)
                {
                    fprintf(stderr, "Malloc failed. Please retry\n");
                    break;
                }
                psMsg->msgType = messageType::MSG;
                psMsg->seqNum = msg.seqNum;
                psMsg->name = msg.name;
                psMsg->data = msg.data;
                psMsg->senderPort = msg.senderPort;
                psMsg->msgId = msg.msgId;
                int ret_ts = std::max(P_value,A_value) + 1;
                P_value = ret_ts;
                hbQ.push_back(psMsg);
                

                memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                sprintf(&acBuffer[MSG_TYPE], "%d", (int) messageType::PROPOSE);
                sprintf(&acBuffer[MSG_ID], "%d", msg.msgId);
                sprintf(&acBuffer[SEQ_NUM], "%d", ret_ts);
                std::cout<<"Proposing a seq Number: " << ret_ts <<" for message " << msg.msgId << "\n";
                sRecAddr.sin_port = htons(msg.senderPort);
                iLenToBeSent = BUFF_SIZE; 
                break;
            }


        case NEW_CLIENT_INFO:
            {
                /* TODO: Display the msg */
                display(&msg);
                iLenToBeSent = 0;
                break;
            }

        case RETRIEVE_MSG:
            {
                if(is_server)
                {
                    /* Retrieve msg from the broadcast buffer and send that
                     * msg back to the client. Note that sRecAddr's port should be
                     * set to listening port of whoever sent the msg. Only
                     * then, the response will reach the client */
                    sRecAddr.sin_port = htons(atoi(&acBuffer[SENDER_LISTENING_PORT]));
                    memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                    sprintf(&acBuffer[SEQ_NUM], "%d",msg.seqNum);
                    get_msg_from_bbm(msg.seqNum, &acBuffer[NAME], &acBuffer[DATA]);

                    /*if the sequence number is not found in the buffer map 
                    *tell the client to increase the sequence number*/
                    if(strcmp(&acBuffer[NAME], "") == 0 && strcmp(&acBuffer[DATA], "") == 0)
                    {
                        sprintf(&acBuffer[MSG_TYPE], "%d", (int) messageType::MSG_NOT_FOUND);
                    }
                    else
                    {
                        //When the message is found in the buffer
                        sprintf(&acBuffer[MSG_TYPE], "%d", (int) messageType::MSG);
                    }
                    iLenToBeSent = BUFF_SIZE;
                }
                break;
            }

        case MSG_NOT_FOUND:
            {
                iExpSeqNum++;
                iLenToBeSent = 0;
                check_hbm_and_display();
                break;

            }

        case CLIENT_LIST:
            {
                if(!is_server)
                {
                    /* TODO: Implement the below two functions */
                    update_client_list(&msg);
                }
               
                display_client_list();
                iLenToBeSent = 0;
                break;
            }


        case CLIENT_HEARTBEAT:
            {

                sRecAddr.sin_port = htons(atoi(&acBuffer[SENDER_LISTENING_PORT]));
                memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                sprintf(&acBuffer[MSG_TYPE], "%d", (int)messageType::CLIENT_HEARTBEAT_RESPONSE);
                sprintf(&acBuffer[DATA],"%d", iListeningPortNum);
                iLenToBeSent = BUFF_SIZE;                
                break;
            }
        case CLIENT_HEARTBEAT_RESPONSE:
            {
                int iPortNo = atoi(&acBuffer[DATA]);
                heartbeatMutex.lock();
                liCurrentClientPort.remove(iPortNo);
                heartbeatMutex.unlock();
                iLenToBeSent = 0;
                break;
            }
        case CLIENT_EXITED:
            {
              
                int port = atoi(msg.data.c_str());
                std::list<int> liClientToBeRemoved(1, port);
                flush_dead_clients(liClientToBeRemoved);
                iLenToBeSent = 0;               
                break;
            }
        case TIMESTAMP:
            {
                if(msg.seqNum > A_value){
                    A_value = msg.seqNum;
                }
                std::cout<<"Displaying the Message with Seq_No " << msg.seqNum << " and msg_Id " << msg.msgId << " from " << msg.senderPort << "\n";
                process_hbq(msg.seqNum, msg.msgId, msg.senderPort);
                iLenToBeSent = 0;  
                break;
            }
        case PROPOSE:
            {                
                
                if ( MapmsgId_propTS.find(msg.msgId) == MapmsgId_propTS.end() ) {
                    std::list<int> list_ts;
                    list_ts.push_back(msg.seqNum);
                    MapmsgId_propTS[msg.msgId] = list_ts;
                } else {
                    MapmsgId_propTS[msg.msgId].push_back(msg.seqNum);
                    if(MapmsgId_propTS[msg.msgId].size() == lpsClients.size()){
                        std::list<int>::iterator max_it = std::max_element(MapmsgId_propTS[msg.msgId].begin(), MapmsgId_propTS[msg.msgId].end());
                        int max_ts = (*max_it);
                        MapmsgId_propTS.erase(msg.msgId);
                        psMsg = new msg_struct;
                        psMsg->msgType = messageType::TIMESTAMP;
                        psMsg->seqNum = max_ts;
                        psMsg->msgId = msg.msgId;
                        psMsg->senderPort = iListeningPortNum;
                        broadcastMutex.lock();
                        qpsBroadcastq.push(psMsg);
                        broadcastMutex.unlock();

                    }
                    break;
                }
                iLenToBeSent = 0;  
            }
    }
    return iLenToBeSent;
}

void process_hbq(int timestamp, int message_id, int senderPort)
{
     for (std::list<msg_struct *>::iterator i = hbQ.begin(); i != hbQ.end(); ++i) 
     {
        if((*i)->msgId == message_id && (*i)->senderPort == senderPort){
            (*i)->seqNum = timestamp;
            break;
        }
     }

     std::list<msg_struct *>::iterator min_it = std::min_element(hbQ.begin(), hbQ.end(), hbComp);
     std::list<msg_struct *>::iterator begin_it = hbQ.begin();
     if((*begin_it)->seqNum != 32760){
        display(*min_it);
        hbQ.erase(min_it);

     }
}

bool hbComp(const msg_struct* lhs, const msg_struct* rhs) {
  return lhs->seqNum < rhs->seqNum;
}

void get_msg_from_bbm( int seq_number, char * name, char * data)
{
    /*Finding the message from the buffer storing it in the
    *name and data arrays of acbuffer*/
    std::map<int, msg_struct *>::iterator buffer_map_iterator;

    broadcastbufferMutex.lock();
    buffer_map_iterator = broadcastBufferMap.find(seq_number);
    
    /*If found the corresponding sequence number as a
    *key in the broadcast buffer map*/
    if (buffer_map_iterator != broadcastBufferMap.end())
    {
        /*Storing the message structure in a
        temp variable the structure*/
        msg_struct * temp = buffer_map_iterator -> second;
        strcpy(name, temp->name.c_str());
        strcpy(data, temp->data.c_str());
    }
    broadcastbufferMutex.unlock();
}

void display(msg_struct * message)
{
    /*If the message type is a simple message*/
    if(message->msgType == messageType::MSG)
    {
        std::cout << message->name + ": " + message->data << "\n";
    }
    else if(message->msgType == messageType::NEW_CLIENT_INFO) //if the message is a new client notification
    {
        std::cout << message->data << "\n";
    }
}


void check_hbm_and_display()
{
    std::map<int, msg_struct *>::iterator hb_map_iterator;

    while(true)
    {
        hb_map_iterator = holdbackMap.find(iExpSeqNum);
        
        /*If found the corresponding sequence number as a
        *key in the Holdback map*/
        if (hb_map_iterator != holdbackMap.end())
        {
            /*display the message*/
            display(hb_map_iterator->second);
            if(hb_map_iterator->second != NULL)
            {
                delete hb_map_iterator->second;
                holdbackMap.erase(hb_map_iterator);
            }            
            iExpSeqNum++;
        }
        else        //if the next sequence number is not in the holdback map
        {
            break;
        }
    }

}

void update_client_list(msg_struct * psMessageStruct)
{
    std::list<msg_struct *> lpsTempClientList;
    std::string message = psMessageStruct->data;

    //Reading the message as a string stream 
    std::stringstream stringStream(message);
    std::string line;


    //Reading upto the new line
    while(std::getline(stringStream, line)) 
    {
        //Copying the string into a new char pointer
        //to change it from string to char pointer
        //and to make it char * from const char *
        char * temp = new char [line.length()+1];
        std::strcpy (temp, line.c_str());

        //char pointer array to hold name, ip , port
        char *tokens[3];
        char * token = std::strtok(temp, ":");
        int i = 0;
        while(token != NULL)
        {
            tokens[i++] = token;
            token = std::strtok(NULL, ":");
        }
        //Creating a new message structure pointer to hold one client
        msg_struct * psClientInfo = new msg_struct;//(); //(msg_struct *) malloc(sizeof(msg_struct));

        //populating the message struct from the tokens from strtok
        psClientInfo->name = tokens[0];
        psClientInfo->ipAddr = tokens[1];
        psClientInfo->port = atoi(tokens[2]);
        
        /* Insert it into the client info list */
        lpsTempClientList.push_back(psClientInfo);
    }

    clientListMutex.lock();
    for (std::list<msg_struct *>::iterator i = lpsClientInfo.begin(); i != lpsClientInfo.end(); ++i)
        delete (*i);

    lpsClientInfo.clear();

    for (std::list<msg_struct *>::iterator iter = lpsTempClientList.begin(); iter != lpsTempClientList.end(); ++iter)
        lpsClientInfo.push_back(*iter);
    lpsClients.clear();
    struct sockaddr_in * psAddr;
    for(std::list<msg_struct *>::iterator i = lpsClientInfo.begin(); i != lpsClientInfo.end(); ++i)
    {
       
        /* Create sockaddr struct and add it to lpsClients */
        psAddr = new sockaddr_in;
        psAddr->sin_family = AF_INET;
        if(inet_pton(AF_INET, ((*i)->ipAddr).c_str(), &(psAddr->sin_addr)) <= 0)
        {
            fprintf(stderr, "Error while storing the IP address. Please retry\n");
            exit(1);
        }
        psAddr->sin_port = htons((*i)->port);

        lpsClients.push_back(psAddr);
    }
    clientListMutex.unlock();
}

void display_client_list()
{

    /* Displaying clients information in a list */
    clientListMutex.lock();
    for (std::list<msg_struct *>::iterator i = lpsClientInfo.begin(); i != lpsClientInfo.end(); ++i)
    {
        msg_struct * psClientInfo = *i; 
        std::cout<< psClientInfo->name + " " + psClientInfo->ipAddr + ":" + std::to_string(psClientInfo->port)  << "\n";
    }
    clientListMutex.unlock();
}
