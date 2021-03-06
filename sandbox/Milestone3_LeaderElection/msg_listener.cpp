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
#include "leader_election.cpp"

int process_rec_msg(char * acBuffer);
void get_msg_from_bbm(int seqNum, char * name, char * data);
void check_hbm_and_display();
void display(msg_struct * msg);
void update_client_list(msg_struct * msg);
void display_client_list();
void check_ack_sb();
void deleteClient(int);

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

    // printf("-----------------------is_server = %d\n", is_server);
    
    switch(msg.msgType)
    {
        case REQ_CONNECTION:
            {
                if(is_server)
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
                                (psClientInfo->name).c_str(), sServerInfo.ipAddr.c_str(),
                                sServerInfo.port, psClientInfo->ipAddr.c_str(), psClientInfo->port);
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
                }
                else
                {
                    /* Send SERVER_INFO to client. Note that sRecAddr's port should be
                     * set to listening port of whoever sent the msg. Only
                     * then, the response will reach the client */
                    sRecAddr.sin_port = htons(atoi(&acBuffer[DATA]));
                    memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                    sprintf(&acBuffer[MSG_TYPE], "%d", (int) messageType::SERVER_INFO);
                    strcpy(&acBuffer[NAME], sServerInfo.name.c_str());
                    strcpy(&acBuffer[DATA], sServerInfo.ipAddr.c_str());
                    iTempIndex = DATA + strlen(&acBuffer[DATA]) + 1;
                    sprintf(&acBuffer[iTempIndex], "%d", sServerInfo.port);
                    iLenToBeSent = BUFF_SIZE;
                }
                break;
            }

        case CONNECTION_ESTABLISHED:
            {
                if(!is_server)
                {
                    sServerInfo.name = msg.name;
                    iExpSeqNum = msg.seqNum;
                    iLenToBeSent = 0;
                }
                break;
            }

        case SERVER_INFO:
            {
                if(!is_server)
                {
                    /* Parse server's IP and port from the DATA part of buffer
                     * and store it in the temp msg struct */
                    msg.ipAddr = &acBuffer[DATA];
                    iTempIndex = strlen(&acBuffer[DATA]) + DATA + 1;
                    msg.port = atoi(&acBuffer[iTempIndex]);

                    /* Store server's information in the global sServerAddr struct */
                    sServerAddr.sin_family = AF_INET;
                    if(inet_pton(AF_INET, msg.ipAddr.c_str(), &sServerAddr.sin_addr) <= 0)
                    {
                        fprintf(stderr, "Error while storing server IP address. Please retry\n");
                        break;
                    }
                    sServerAddr.sin_port = htons(msg.port);

                    /* Store server's information in the global sServerInfo struct */
                    sServerInfo.name = msg.name;
                    sServerInfo.ipAddr = msg.ipAddr;
                    sServerInfo.port = msg.port;

                    /* Send REQ_CONNECTION to server now */
                    memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                    sprintf(&acBuffer[MSG_TYPE], "%d", (int)messageType::REQ_CONNECTION);
                    strcpy(&acBuffer[NAME], username.c_str());
                    sprintf(&acBuffer[DATA], "%d", iListeningPortNum);
                    sRecAddr = sServerAddr;
                    iLenToBeSent = BUFF_SIZE;
                }
                break;
            }

        case CHAT:
            {
                if(is_server)
                {
                    /* Fill msg struct with data to be sent */
                    //psMsg = (msg_struct *) malloc(sizeof(msg_struct));
                    std::cout<<"Got a message from: " << msg.name << "\n";
                    printf("seqnum :%d expected: %d\n", iSeqNum, iExpSeqNum);
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

                    /* Push the msg to the broadcast queue */
                    broadcastMutex.lock();
                    qpsBroadcastq.push(psMsg);
                    broadcastMutex.unlock();

                    /* Send ACK to client. Note that sRecAddr's port should be
                     * set to listening port of whoever sent the msg. Only
                     * then, the response will reach the client */
                    sRecAddr.sin_port = htons(atoi(&acBuffer[SENDER_LISTENING_PORT]));
                    memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                    sprintf(&acBuffer[MSG_TYPE], "%d", (int)messageType::ACK);
                    sprintf(&acBuffer[MSG_ID], "%d", msg.msgId);
                    iLenToBeSent = BUFF_SIZE;
                }
                break;
            }

        case MSG:
            {   //std::cout<<"Got a message from the server" << msg.data << "Sequence Number: " << msg.seqNum << "\n";
                if(msg.seqNum == iExpSeqNum)
                {
                    /* Display msg and then display all other msgs from the hbm
                     * that should be displayed */
                    /* TODO Implement the below function */
                    display(&msg);
                    iExpSeqNum++;
                    iLenToBeSent = 0;
                    /* TODO: Implement the below function */
                    check_hbm_and_display();
                }
                else if(msg.seqNum > iExpSeqNum)
                {
                    /* Create an entry in bbm for the msg */
                    //psMsg = (msg_struct *) malloc(sizeof(msg_struct));
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
                    holdbackMap.insert(std::pair<int, msg_struct *>(msg.seqNum, psMsg));

                    if(msg.seqNum >= iExpSeqNum + 2)
                    {   for(int iterator = iExpSeqNum; iterator < msg.seqNum; iterator++)
                        {
                            memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                            sprintf(&acBuffer[MSG_TYPE], "%d", (int)messageType::RETRIEVE_MSG);
                            sprintf(&acBuffer[SENDER_LISTENING_PORT], "%d", iListeningPortNum);
                            sprintf(&acBuffer[SEQ_NUM],"%d", iterator);
                            sRecAddr = sServerAddr;
                            iLenToBeSent = BUFF_SIZE;
                            sendto(iSendingSocketFd, acBuffer, iLenToBeSent, 0,(struct sockaddr *) &sRecAddr, iRecAddrLen);
                        }
                        iLenToBeSent = 0;

                    }
                    else
                    {
                        iLenToBeSent = 0;
                        
                    }
                }

                // XXX TESTING LEADER ELECTION XXX
                if(strcmp(msg.data.c_str(), "election") == 0)
                {
                    initiate_leader_election();
                }

                /*
                else
                {
             
                    fprintf(stderr, "Received unexpected msg\n");
                    break;
                }
                */
                
                break;
            }

        case ACK:
            {
                printf("=================== received ACK message ================== \n");
                if(!is_server)
                {
                    /* Remove entry from sent buffer */
                    sentbufferMutex.lock();

                    /* Logging */
                    /*
                    std::cout << "Current msg ID recd: " << msg.msgId << "\n\n";
                    for(auto it = sentBufferMap.cbegin(); it != sentBufferMap.cend(); ++it)
                    {
                        std::cout << it->first << " " << it->second << "\n";
                    }
                    */

                    if (sentBufferMap.find(msg.msgId) != sentBufferMap.end())
                    {
                        sentBufferMap.erase(msg.msgId);
                    }
                    else
                    {
                        fprintf(stderr, "Unexpected ack received\n");
                        break;
                    }
                    if(sentBufferMap.size() > 0){
                        check_ack_sb();
                    }
                    sentbufferMutex.unlock();
                }
                iLenToBeSent = 0;
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
                    if(&acBuffer[NAME] == NULL && &acBuffer[DATA] == NULL)
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

        case NEW_LEADER_ELECTED: 
            {
                if(!is_server)
                {

                    msg.ipAddr = &acBuffer[DATA];
                    iTempIndex = strlen(&acBuffer[DATA]) + DATA + 1;
                    msg.port = atoi(&acBuffer[iTempIndex]);
                    std::cout<<"got a new leader " << msg.port << "\n";

                    // Store server's information in the global sServerAddr struct 
                    sServerAddr.sin_family = AF_INET;
                    if(inet_pton(AF_INET, msg.ipAddr.c_str(), &sServerAddr.sin_addr) <= 0)
                    {
                        fprintf(stderr, "Error while storing server IP address. Please retry\n");
                        break;
                    }
                    sServerAddr.sin_port = htons(msg.port);

                    // Store server's information in the global sServerInfo struct
                    sServerInfo.name = msg.name;
                    sServerInfo.ipAddr = msg.ipAddr;
                    sServerInfo.port = msg.port;
                    //iExpSeqNum = 0;
                    deleteClient(msg.port);

                    // Send REQ_CONNECTION to server now
                    // memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                    // sprintf(&acBuffer[MSG_TYPE], "%d", (int)messageType::REQ_CONNECTION);
                    // strcpy(&acBuffer[NAME], msg.name.c_str());
                    // sprintf(&acBuffer[DATA], "%d", msg.port);
                    //sRecAddr = sServerAddr;
                    iLenToBeSent = 0;
                    
                    iSeqNum = 0;
                    iExpSeqNum = 0;
                }
                break;
            
            }
        case CLIENT_HEARTBEAT:
            {
                if(!is_server)
                {
                    
                    memset(acBuffer, 0x0, BUFF_SIZE * sizeof(char));
                    sprintf(&acBuffer[MSG_TYPE], "%d", (int)messageType::CLIENT_HEARTBEAT);
                    sprintf(&acBuffer[DATA],"%d", iListeningPortNum);
                    sRecAddr = sServerAddr;
                    iLenToBeSent = BUFF_SIZE;

                    
                }
                else
                {
                    int iPortNo = atoi(&acBuffer[DATA]);
                    liCurrentClientPort.remove(iPortNo);
                    iLenToBeSent = 0;
                }
                
                break;
            }
        case CLIENT_EXITED:
            {
                if(is_server)
                {
                    int port = atoi(msg.data.c_str());
                    for (std::list<msg_struct *>::iterator iter = lpsClientInfo.begin(); iter != lpsClientInfo.end(); ++iter)
                    {
                        msg_struct * psClientInfo = *iter; 
                        if(psClientInfo->port == port)
                        {
                            msg_struct * psMsg = new msg_struct;//();
                            if(psMsg == NULL)
                            {
                                fprintf(stderr, "Malloc failed. Please retry\n");
                                break;
                            }
                            char acTempStr[100] = "\0";
                            psMsg->msgType = messageType::NEW_CLIENT_INFO;
                            sprintf(acTempStr, "NOTICE %s left the chat or crashed",(psClientInfo->name).c_str());
                            psMsg->data = acTempStr;
                            broadcastMutex.lock();
                            qpsBroadcastq.push(psMsg);
                            broadcastMutex.unlock();               
                            iter = lpsClientInfo.erase(iter);
               
                        }


                    }
                    for (std::list<sockaddr_in *>::iterator iter1 = lpsClients.begin(); iter1 != lpsClients.end(); ++iter1)
                    {
            
                        if(ntohs((*iter1)->sin_port) == port)
                        {
                            iter1 = lpsClients.erase(iter1);
        
                        }            
                    }
                }
            break;
        }
        case STOP_LEADER_ELECTION:
        {
            printf("============== RECEIVED STOP_LEADER_ELECTION ===============\n");
            printf("============== INTERRUPT_LEADER_ELECTION = %d ===============\n", INTERRUPT_LEADER_ELECTION);
            INTERRUPT_LEADER_ELECTION = true;
            is_server = false;
            break;
        }
    }
    return iLenToBeSent;
}

void get_msg_from_bbm( int seq_number, char * name, char * data)
{
    /*Finding the message from the buffer storing it in the
    *name and data arrays of acbuffer*/
    std::map<int, msg_struct *>::iterator buffer_map_iterator;

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
    /*Clearing the Clients list*/
    lpsClientInfo.clear();

    std::string message;
    //populating message from the structure data part
    message = psMessageStruct->data;

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
        if(psClientInfo == NULL)
        {
            fprintf(stderr, "Malloc failed. Please retry\n");
            break;
        }

        //populating the message struct from the tokens from strtok
        psClientInfo->name = tokens[0];
        psClientInfo->ipAddr = tokens[1];
        psClientInfo->port = atoi(tokens[2]);
        
        /* Insert it into the client info list */
        clientListMutex.lock();
        lpsClientInfo.push_back(psClientInfo);
        clientListMutex.unlock();
  
    }

}


void display_client_list()
{
    //Getting the server info from the first element of the list
    //msg_struct * psServerInfo = lpsClientInfo.front();
    std::cout<< sServerInfo.name + " " + sServerInfo.ipAddr + ":" + std::to_string(sServerInfo.port) + " (Leader)" << "\n";

    //Displaying the clients information in a list
    //starting from the second element in the list
    for (std::list<msg_struct *>::iterator i = lpsClientInfo.begin(); i != lpsClientInfo.end(); ++i)
    {
        msg_struct * psClientInfo = *i; 
        std::cout<< psClientInfo->name + " " + psClientInfo->ipAddr + ":" + std::to_string(psClientInfo->port)  << "\n";
    }
}

void deleteClient(int port)
{       clientListMutex.lock();
        for (std::list<msg_struct *>::iterator iter = lpsClientInfo.begin(); iter != lpsClientInfo.end(); ++iter)
        {
            msg_struct * psClientInfo = *iter; 
            if(psClientInfo->port == port)
            {             
                iter = lpsClientInfo.erase(iter);
                break;
               
            }


        }
        for (std::list<sockaddr_in *>::iterator iter1 = lpsClients.begin(); iter1 != lpsClients.end(); ++iter1)
        {
            
            if(ntohs((*iter1)->sin_port) == port)
            {
                iter1 = lpsClients.erase(iter1);
                break;
        
            }
            

        }
        clientListMutex.unlock();
}
