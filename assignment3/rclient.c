#include <argp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sysexits.h>
#include <signal.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include "time.h"
#define MAXCOMMANDLEN 66000

int alarmFlag = 0;

/** TA:
 * Overall, looks good. Had a good breakdown of functions that made sense.
 * I would only suggest maybe some more utility functions for networking stuff
 * that looks like it repeats a lot. And less magic (hex)numbers and such, just
 * for readability and potentially to make things typo-proof.
 */

/** TA:
 * Good use of these functions
 */
int readBytes(int socket, size_t length, void* buffer) {
	size_t recieved = 0;
	while(recieved < length) {
		recieved += recv(socket, buffer + recieved, length - recieved, 0);
	}
    return recieved;
}

int sendBytes(int socket, size_t length, void* buffer) {
	size_t sent = 0;
	while(sent < length) {
		sent += send(socket, buffer + sent, length - sent, 0);
	}
    return sent;
}

int errorHandler(int socket, uint32_t flag, uint32_t recLen) {

    recLen = ntohl(recLen);
    uint8_t hasMsg = 0;
    readBytes(socket, 1, &hasMsg);


    /** TA:
     * 0x9a isn't strictly an 'error'. There is a pattern
     * to when you'll see 0x9a.
     */
    //printf("The flag in Msg Usr is %x\n", recflag);
    if(flag == 0x9a1704) {
        if(hasMsg != 0) {
            uint8_t recBuf[recLen];
            memset(recBuf, 0, recLen);
            readBytes(socket, recLen - 1, recBuf);
            recBuf[recLen] = '\0';
            printf("Command failed. (%s)\n", recBuf);
            return 0;
        }
    }
    return 1;
}

int hasSpace(char arr[]) {
    for(int i = 0; arr[i] != '\0'; i ++ ){
        if(isspace(arr[i]) != 0) {
            return 1;
        }
    }
    return 0;
}
//THIS WORKS FINE
int HandleConnection(char* command, char *clientName, int *connected) {

    /*FIND SOME OTHER WAY FOR THIS*/
    char ipaddr[16] = {0};
    uint32_t ip1, ip2, ip3, ip4, port;
    sscanf(command, "\\connect %d.%d.%d.%d:%d\n", &ip1, &ip2, &ip3, &ip4, &port);
    sprintf(ipaddr, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);


    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //fcntl(sock, F_SETFL, O_NONBLOCK);
    struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr)); //zero out structure
	servAddr.sin_family = AF_INET;
	if((servAddr.sin_addr.s_addr = inet_addr(ipaddr)) == (in_addr_t)(-1)) {
        printf("Could not connect to the chat server. (Invalid IP address)\n");
    }
	servAddr.sin_port = htons(port);

    if(connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
        //printf("Could not connect to the chat server. (Connection refused)\n");
        printf("Could not connect to the chat server. (Invalid argument)\n");
	}
    //printf("got here 2\n");
    if(sock < 0) {
        printf("Could not connect to the chat server. (Invalid argument)\n");
    }
    else {
        uint8_t sendBuffer[48];

        // TA: You don't need to write that in hex, just fyi
        uint32_t msgLen = htonl(0x00000029);
        uint32_t flag = 0x9b1704; //changing endians
        char msg[42] = "Greetings from the land of milk and honig";
        memcpy(sendBuffer, &msgLen, 4);
        memcpy(sendBuffer+4, &flag, 3);
        memcpy(sendBuffer+7, msg, 41);
        sendBytes(sock, 48, sendBuffer);

        uint32_t nameLen;
        readBytes(sock, 4, &nameLen);
        nameLen = ntohl(nameLen) - 1;
        uint32_t recvFlag;
        readBytes(sock, 4, &recvFlag);

        /** TA:
         * Is this the right size for name?
         */
        char name[nameLen];//DELETED + 1
        //char name[nameLen];
        readBytes(sock, nameLen, name);

        strcpy(clientName, name); 
        *connected = 1;
        printf("Connected to the chat server. Your name is '%s'.\n", name); 
    }
    return sock;
}

//THIS ONE DOESN'T WORK AS EXPECTED.
int HandleJoin(char* command, int socket, char *roomName) {
    char pass[256] = {0}, room[256] = {0}, roomCheck[256] = {0}, passCheck[256] = {0};
    int val = sscanf(command, "\\join %s %[^\n]s", room, pass);
    memcpy(roomCheck, command+6, strlen(room));
    memcpy(passCheck, command+strlen(room)+ 6 + 1, strlen(pass));

    if(val <= 0 || val > 2 || hasSpace(roomCheck) == 1 || hasSpace(passCheck) == 1) {
        printf("Invalid Command.\n");
        return 0;
    }
    uint8_t roomLen = strlen(room);
    uint8_t passLen = strlen(pass); 
    uint32_t totalMsgLen = htonl(strlen(room) + 2 + strlen(pass)); //has been htonl'd
    uint32_t flag = 0x031704;

    //printf("totalMsgLen is %d, roomlen is %d passlen is %d, room is %s and pass is %s\n",ntohl(totalMsgLen),roomLen, passLen, room, pass);
    /** TA:
     * Might it be helpful to write a helper function that takes care of
     * some repeated things or the formats of messages?
     */

    sendBytes(socket, 4, &totalMsgLen);
    sendBytes(socket, 3, &flag);
    sendBytes(socket, 1, &roomLen);
    sendBytes(socket, roomLen, room); //sent over the room length and name. now need to do so for password
    sendBytes(socket, 1, &passLen); 
    sendBytes(socket, passLen, pass); //may have to do an if 0 check 

    uint32_t ret;
    readBytes(socket, 4, &ret);
    ret = ntohl(ret);
    if(ret == 1){
        uint32_t retFlag;
        readBytes(socket, 4, &retFlag);

        /** TA:
         * What's the point of roomName?
         */
        memset(roomName, 0, 256);
        strcpy(roomName, room);
        printf("Joined room '%s'.\n", room);
        return 1;
    }else{
        uint32_t retflag;
        readBytes(socket, 4, &retflag);

        uint8_t msg[ret];
        memset(msg, 0, ret);
        readBytes(socket, ret-1, msg);
        msg[ret] = '\0';
        printf("Command failed. (%s)\n", msg);
        if(strcmp((const char*) msg, "You attempt to bend space and time to reenter where you already are. You fail.") == 0) {
            return 1;
        }
        return 0;
    }
} 

//PRETTY SURE THIS ONE WORKS AS EXPECTED
void HandleLeave(int socket) {
    uint32_t flag = 0x061704;
    uint32_t msgLen = htonl(0);
    uint8_t sendBuf[7];
    memcpy(sendBuf, &msgLen, 4);
    memcpy(sendBuf + 4, &flag, 3);
    sendBytes(socket, 7, sendBuf);
}

//THIS WORKS FINE
void HandleListUsers(char* command, int socket, int inRoom, char* room) {
    if(strlen(command) != 12) {
        printf("Invalid Command.\n");
    }
    else {
        uint32_t sendLen = htonl(0);
        uint32_t flag = 0x0c1704;
        sendBytes(socket, 4, &sendLen);
        sendBytes(socket, 3, &flag);

        if(inRoom == 1) {
            printf("List of users in room '%s':", room);
        }
        else {
            printf("List of all users:");
        }
        uint32_t lstLen = 0;
        readBytes(socket, 4, &lstLen);
        lstLen = ntohl(lstLen);

        uint32_t rflag = 0;
        readBytes(socket, 4, &rflag);

        uint32_t count = 0;
        while(count < lstLen - 1) {
            uint32_t nameLen = 0;
            readBytes(socket, 1, &nameLen); //specifies strlen of person we want to read

            uint8_t name[nameLen + 1]; //was + 1
            memset(name, 0, nameLen + 1);
            readBytes(socket, nameLen, name);
            name[nameLen] = '\0'; //was '\0'
            printf(" %s", name);
            count += nameLen+1;
        }
        printf("\n"); 
    }
}

//THIS WORKS FINE
void HandleListRooms(int socket) {
    uint32_t len = htonl(0);
    uint32_t flag = 0x091704;
    sendBytes(socket, 4, &len);
    sendBytes(socket, 3, &flag);
    
    uint32_t lstLen = 0;
    readBytes(socket, 4, &lstLen);
    lstLen = ntohl(lstLen); //subtract 1

    uint32_t rflag = 0;
    readBytes(socket, 4, &rflag);


    uint32_t count = 0;
    printf("List of rooms:");
    while(count < lstLen - 1) {
        uint32_t nameLen = 0;
        readBytes(socket, 1, &nameLen); //specifies length of person we want to read
        uint8_t name[nameLen + 1];
        memset(name, 0, nameLen + 1);
        readBytes(socket, nameLen, name);
        name[nameLen] = '\0';

        printf(" %s", name);
        count += nameLen+1;
    }
    printf("\n");
}


//WORKS FINE
void HandleRecieveMsg(int socket) {
    uint32_t packetLen = 0;
    uint32_t flag = 0;
    readBytes(socket, 4, &packetLen);
    readBytes(socket, 3, &flag);
    packetLen = ntohl(packetLen);
    if(flag == 0x121704) {
        //sending msg to another user
        //reads in length of name
        uint8_t nameLen = 0;
        readBytes(socket, 1, &nameLen);
        
        //sets up name buffer with +1 bytes to account for null terminator
        uint8_t name[nameLen + 1];
        memset(name, 0, nameLen + 1);
        readBytes(socket, nameLen, name);
        name[nameLen] = '\0';

        //reads in len of msg 
        uint16_t msgLen = 0;
        readBytes(socket, 2, &msgLen);
        msgLen = ntohs(msgLen);

        //sets up msg buffer + 1 to account for null terminator
        uint8_t msg[msgLen + 1];
        memset(msg, 0, msgLen + 1);
        readBytes(socket, msgLen, msg);
        msg[msgLen] = '\0';

        printf("< %s: %s\n", name, msg);
    }
    if(flag == 0x151704) {
        //reads in len of rooms
        uint8_t roomLen = 0;
        readBytes(socket, 1, &roomLen);

        //sets up room buffer
        uint8_t room[roomLen + 1];
        memset(room, 0, roomLen + 1);
        readBytes(socket, roomLen, room);
        room[roomLen] = '\0';

        //reads in length of name
        uint8_t nameLen = 0;
        readBytes(socket, 1, &nameLen);
        
        //sets up name buffer with +1 bytes to account for null terminator
        uint8_t name[nameLen + 1];
        memset(name, 0, nameLen + 1);
        readBytes(socket, nameLen, name);
        name[nameLen] = '\0';

        //reads in len of msg 
        uint16_t msgLen = 0;
        readBytes(socket, 2, &msgLen);
        msgLen = ntohs(msgLen);

        //sets up msg buffer + 1 to account for null terminator
        uint8_t msg[msgLen + 1];
        memset(msg, 0, msgLen + 1);
        readBytes(socket, msgLen, msg);
        msg[msgLen] = '\0';

        printf("[%s] %s: %s\n", room, name, msg);
    } 
    if(flag == 0x9a1704) {
        uint8_t buf[packetLen];
        memset(buf, 0, packetLen);
        readBytes(socket, packetLen, buf);
    }

}


//THIS WORKS FINE
void HandleMsgUser(char *command, int socket) {
    char user[256], msg[65536+1];
    if(sscanf(command, "\\msg %s %[^\n]s", user, msg) != 2) {
        printf("Invalid Command.\n");
    }
    else {
        uint8_t userLen = strlen(user);
        uint16_t msgLen = strlen(msg);
        uint32_t totalMsgLen = htonl((1 + userLen) + (2 + msgLen));
        msgLen = htons(msgLen);

        uint32_t flag = 0x121704;
        sendBytes(socket, 4, &totalMsgLen);
        sendBytes(socket, 3, &flag);
        sendBytes(socket, 1, &userLen);
        sendBytes(socket, userLen, user);
        sendBytes(socket, 2, &msgLen);
        sendBytes(socket, ntohs(msgLen), msg);


        uint32_t recLen = 0;
        readBytes(socket, 4, &recLen);
        recLen = ntohl(recLen);
        uint32_t recflag = 0;
        readBytes(socket, 3, &recflag);
        uint8_t hasMsg = 0;
        readBytes(socket, 1, &hasMsg);

        //printf("The flag in Msg Usr is %x\n", recflag);
        if(recflag == 0x9a1704) {

            /** TA:
             * If you send a message (in general) how do you
             * know it even succeeded?
             */
            if(hasMsg == 0) {
                printf("> %s: %s\n", user, msg);
            }
            else {
                uint8_t recBuf[recLen];
                memset(recBuf, 0, recLen);
                readBytes(socket, recLen - 1, recBuf);
                recBuf[recLen] = '\0';
                printf("Command failed. (%s)\n", recBuf);
            }
        }
        if(recflag == 0x121704) {
            uint8_t nameLen = 0;
            nameLen += hasMsg;
            //readBytes(socket, 1, &nameLen);
            
            //sets up name buffer with +1 bytes to account for null terminator
            uint8_t name[nameLen + 1];
            memset(name, 0, nameLen + 1);
            readBytes(socket, nameLen, name);
            name[nameLen] = '\0';

            //reads in len of msg 
            uint16_t msgLen = 0;
            readBytes(socket, 2, &msgLen);
            msgLen = ntohs(msgLen);

            //sets up msg buffer + 1 to account for null terminator
            uint8_t msg[msgLen + 1];
            memset(msg, 0, msgLen + 1);
            readBytes(socket, msgLen, msg);
            msg[msgLen] = '\0';

            printf("> %s: %s\n", name, msg);
            printf("< %s: %s\n", name, msg);
        }

    }
}

//THIS WORKS FINE
void HandleNickName(char *command, int socket, char *name) {
    char nick[256];
    int val = sscanf(command, "\\nick %[^\n]s", nick);
    char nickCheck[256];
    memcpy(nickCheck, command+6, strlen(nick));

    if(val != 1 || hasSpace(nickCheck) == 1 || strlen(nick) == 0) {
        
        printf("Invalid Command.\n");
    }
    else {
        uint8_t nickLen = strlen(nick);
        uint32_t totalLen = nickLen + 1;
        uint32_t flag = 0x0f1704;

        totalLen = htonl(totalLen);
        sendBytes(socket, 4, &totalLen);
        sendBytes(socket, 3, &flag);
        sendBytes(socket, 1, &nickLen);
        sendBytes(socket, nickLen, nick);

        uint32_t recLen = 0;
        readBytes(socket, 4, &recLen);
        //recLen = ntohl(recLen);
        uint32_t recflag = 0;
        readBytes(socket, 3, &recflag);

        if(errorHandler(socket, recflag, recLen) == 1) {
            memset(name, 0, 256);
            strcpy(name, nick);
            printf("Changed nick to '%s'.\n", name);
        }
    }
}

//say we had a sendPacket function that sent
//WORKS FINE
void HandleRoomMsg(char* command, char * roomName, int socket, char* clientName) {
    uint32_t packetLen = (strlen(roomName) + 1) + strlen(command) + 1;
    packetLen = htonl(packetLen);

    uint32_t flag = 0x151704;
    uint8_t roomLen = strlen(roomName);

    uint16_t msgLen = strlen(command) - 1;//for the \n
    msgLen = htons(msgLen);
    sendBytes(socket, 4, &packetLen);
    sendBytes(socket, 3, &flag);
    sendBytes(socket, 1, &roomLen);
    sendBytes(socket, roomLen, roomName);
    sendBytes(socket, 2, &msgLen);
    sendBytes(socket, ntohs(msgLen), command);

    printf("[%s] %s: %s", roomName, clientName, command);
}

//WORKS OK I THINK
void HandleStayingAlive(int socket) {
    //printf("in here\n");
    uint32_t packLen = htonl(0x00000020);
    uint32_t sendFlag = 0x131704;
    uint8_t msgLen = 0x1f;
    char msg[] = "staying alive, staying alive...";

    sendBytes(socket, 4, &packLen);
    sendBytes(socket, 3, &sendFlag);
    sendBytes(socket, 1, &msgLen);
    sendBytes(socket, 31, msg);
}

//WORKS FINE
void HandleDisconnect(int socket, char *room , char* name, struct pollfd sock[]) {
    alarm(0);
    alarmFlag = 0;
    close(socket);
    sock[1].fd = -1;
    memset(room, 0, 256);
    memset(name, 0, 256);
}

void sigHandler() {
    alarmFlag = 1;
    signal(SIGALRM, sigHandler);
    alarm(1);
}

int main() {
    struct pollfd sock[2];
    int connected = 0, inRoom = 0, socket = -1;
    char clientName[256] = {0}; char roomName[256] = {0};
    sock[0].fd = STDIN_FILENO;
    sock[0].events = POLLIN;

    signal(SIGALRM, sigHandler); //every 6 seconds sighandler is gonna make alarmFlag = 1;
    //sigHandler();
    alarmFlag = 0;
    alarm(6);

    for(;;) {
        char command[MAXCOMMANDLEN];
        //memset(command, 0, MAXCOMMANDLEN);

        if(poll(sock, 2, 0) > 0) {

            if(sock[0].revents && POLLIN) {
                memset(command, 0, MAXCOMMANDLEN);
                fgets(command, MAXCOMMANDLEN, stdin);
                //HandleStayingAlive(socket);
                //printf("In a room is %d\n", inRoom);
                if(strstr(command, "\\") == command) {
                    
                    if(strstr(command, "\\connect ") == command) {
                        //if(connect != 1) connect
                        //else printf("Already connected to server\n");
                        if(connected != 1) {
                            socket = HandleConnection(command, clientName, &connected);
                            alarmFlag = 0;
                            signal(SIGALRM, sigHandler);
                            alarm(1);
                            sock[1].fd = socket;
                            sock[1].events = POLLIN;
                            //HandleStayingAlive(socket);
                        }
                        else {
                            printf("Already connected to a chat server.\n");
                        }
                    }

                    else if(strstr(command, "\\join ") == command) {
                        if(connected == 1) {
                            inRoom = HandleJoin(command, socket, roomName);
                            alarmFlag = 0;
                            signal(SIGALRM, sigHandler);
                            //alarmFlag = 0;
                            alarm(6);
                            //signal(SIGALRM, sigHandler);
                            //sendJoin(command, roomName, &inRoom, socket);
                            //inRoom = HandleJoin(command, socket, roomName);
                            memset(command, 0, MAXCOMMANDLEN);
                        }
                        else {
                            printf("Not connected to a chat server.\n");
                        } //do this for all
                    }

                    else if(strcmp(command, "\\leave\n") == 0) {
                        if(connected == 1) {
                            alarmFlag = 0;
                            HandleLeave(socket);
                            if(inRoom == 1) {
                                //alarm(6);
                                signal(SIGALRM, sigHandler);
                                alarm(6);
                                printf("Left room '%s'.\n", roomName);
                                memset(roomName, 0, 256);
                            }
                            else {
                                //alarm(0);
                                HandleDisconnect(socket, roomName, clientName, sock);
                                connected = 0;
                                printf("Left the chat server.\n");
                                printf("Disconnected from the chat server (Reason: Server closed the connection)\n");
                            } 
                            inRoom = 0;
                        }
                        else {
                            printf("Not connected to a chat server.\n");
                        }
                    }

                    else if(strstr(command, "\\list ") == command) {
                        if(strcmp(command, "\\list users\n") == 0 ) {
                            if(connected == 1) {
                                HandleListUsers(command, socket, inRoom, roomName);
                                alarmFlag = 0;
                                signal(SIGALRM, sigHandler);
                                //alarmFlag = 0;
                                //siginterrupt(SIGALRM, 1);
                                alarm(6);
                                //signal(SIGALRM, sigHandler);
                                //HandleListUsers(command, socket, inRoom, roomName);
                            }
                            else {
                                printf("Not connected to a chat server.\n");
                            }
                        }
                        else if(strcmp(command, "\\list rooms\n") == 0) {
                            if(connected == 1) {
                                HandleListRooms(socket);
                                alarmFlag = 0;
                                signal(SIGALRM, sigHandler);
                                //alarmFlag = 0;
                                //siginterrupt(SIGALRM, 1);
                                alarm(6);
                                //signal(SIGALRM, sigHandler);
                                //printf("got here\n");
                                //HandleListRooms(socket);
                            }
                            else {
                                printf("Not connected to a chat server.\n");
                            }
                        }
                        else {
                            printf("Invalid list type. Please choose from \"rooms\" or \"users\".\n");
                        }
                        //list users
                    }
                    else if(strstr(command, "\\msg ") == command) {
                        //msg user
                        if(connected == 1) {
                            HandleMsgUser(command, socket);
                            alarmFlag = 0;
                            signal(SIGALRM, sigHandler);
                            //alarmFlag = 0;
                            alarm(6);
                            //signal(SIGALRM, sigHandler);
                            //HandleMsgUser(command, socket);
                        }
                        else {
                            printf("Not connected to a chat server.\n");
                        }
                    }

                    else if(strstr(command, "\\nick ") == command) {
                        //nickname
                        if(connected == 1) {
                            HandleNickName(command, socket, clientName);
                            alarmFlag = 0;
                            signal(SIGALRM, sigHandler);
                            alarm(6);
                            //signal(SIGALRM, sigHandler);
                            //HandleNickName(command, socket, clientName);
                        }
                        else {
                            printf("Not connected to a chat server.\n");
                        }
                    }

                    else if(strcmp(command, "\\disconnect\n") == 0) {
                        if(connected == 1) {
                            HandleDisconnect(socket, roomName, clientName, sock);
                            connected = 0;
                        }
                        else {
                            printf("Not connected to a chat server.\n");
                        }
                    }

                    else if(strcmp(command, "\\quit\n") == 0) {
                        close(socket);
                        connected = 0;
                        printf("Quiting...\n");
                        break;
                    }

                    else { //not any of the commands listed above
                        printf("Invalid Command.\n");
                    }
                }

                else {
                    if(connected == 1) {
                        if(inRoom == 1) {
                            //handleRoomMsg
                            alarmFlag = 0;
                            signal(SIGALRM, sigHandler);
                            //alarmFlag = 0;
                            alarm(6);
                            //signal(SIGALRM, sigHandler);
                            HandleRoomMsg(command, roomName, socket, clientName);
                        }
                        else {
                            printf("Command failed. (You shout into the void and hear nothing.)\n");
                        }
                    }
                    else {
                        printf("Not connected to a chat server.\n");
                    }
                }
            }

            /** TA:
             * Is this correct?
             */
            if(sock[1].revents && POLLIN && connected == 1) {
                HandleRecieveMsg(sock[1].fd);
            }

        }


        if(connected == 1 && alarmFlag == 1) {
            alarmFlag = 0;
            HandleStayingAlive(socket);
            //alarm(6);
            //signal(SIGALRM, sigHandler);;
        }
    }

    close(socket);

}