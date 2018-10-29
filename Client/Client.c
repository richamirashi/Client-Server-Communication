#include <stdio.h>
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h> /* for sockaddr_in and inet_addr() */
#include <stdlib.h> /* for atoi() */
#include <string.h> /* for memset() */
#include <unistd.h> /* for close() */
#include <stdbool.h>

#define RCVBUFSIZE 1024 /* Size of receive buffer */
#define BUFFERSIZE 1024
#define MAXPENDING 5
#define TRUE 1


void DieWithError(char *errorMessage);


char* strcatwrapper(char* str1, char* str2){
    char* concatenatedStr;
    concatenatedStr = malloc(strlen(str1) + strlen(str2));
    strcpy(concatenatedStr, str1);
    strcat(concatenatedStr, str2);
    return concatenatedStr;
}


int printAndSelectOption(){
    char buffer[5];
    printf("=====================================================\n");
    printf("Command: \n");
    printf("0. Connect to the Server\n");
    printf("1. Get the user list\n");
    printf("2. Send a message\n");
    printf("3. Get my messages\n");
    printf("4. Initiate a chat with my friend\n");
    printf("5. Chat with my friend\n");
    printf("Your option <please enter a number>: ");
    fgets(buffer, 5, stdin);
    return atoi(buffer);
}


char* readInput(char* commandMsg){
    char* stringToReturn;
    char buff[BUFFERSIZE];
    size_t buffLen;
    printf("%s", commandMsg);
    fgets(buff, BUFFERSIZE, stdin);
    buffLen = strlen(buff);
    if(buffLen > 0 && buff[buffLen-1] == '\n'){
        buff[--buffLen] = '\0';
    }
    stringToReturn = malloc(strlen(buff));
    strcpy(stringToReturn, buff);
    return stringToReturn;
}


void sendMsgToServer(int sock, char* msg){
    if(send(sock, msg, strlen(msg), 0) != strlen(msg))
        DieWithError("send() sent a different number of bytes than expected");
}


char* receiveMsgFromServer(int sock){
    char rcvBuffer[RCVBUFSIZE];
    int bytesRcvd;
    char* receivedMsg;

    if((bytesRcvd = recv(sock, rcvBuffer, RCVBUFSIZE - 1, 0)) <= 0)
        DieWithError("recv() failed or connection closed prematurely");
    rcvBuffer[bytesRcvd] = '\0'; // Terminate the string!
    receivedMsg = malloc(strlen(rcvBuffer));
    strcpy(receivedMsg, rcvBuffer);
    return receivedMsg;
}


char* receiveMsgAndSendACKtoServer(int clntSock){
    char* receivedMsg = receiveMsgFromServer(clntSock);
    char* ACK = "Received";
    sendMsgToServer(clntSock, ACK);
    return receivedMsg;
}


char* login(int sock){
    char* username;
    char* password;
    char* loginStatus;

    printf("Welcome! Please log in.\n");

    username = readInput("Username: ");
    sendMsgToServer(sock, username);
    
    password = readInput("Password: ");
    sendMsgToServer(sock, password);
    
    loginStatus = receiveMsgFromServer(sock);
    printf("%s Login\n", loginStatus);

    return username;
}


int connectWithServer(){
    int sock;
    struct sockaddr_in servAddr;
    unsigned short servPort;
    char* servIP;

    // Create a reliable, stream socket using TCP
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("\nsocket() failed");
    
    servIP = readInput("Please enter the IP address: ");
    char* servPort_str = readInput("Please enter the port number: ");
    servPort = atoi(servPort_str);

    // Construct the server address structure
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIP);
    servAddr.sin_port = htons(servPort);

    printf("Connecting.....\n");            
    
    // Establish the connection to the server
    if(connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
        DieWithError("\nconnect() failed");
    } 

    printf("Connected!\n");
    return sock;
}


void sendCaseOptionToServer(int selectedOption, int clntSock){
    char optionbuff[5];
    int numOfCharsWritten = sprintf(optionbuff, "%d", selectedOption);
    char* option1 = malloc(numOfCharsWritten);
    strcpy(option1, optionbuff);
    sendMsgToServer(clntSock, option1);
}


bool sendMsgToFriend(int clntSock, char* user){
    printf("Please enter <Bye> to end this conversation.\n");
    char* str = ": ";
    char* userStr = strcatwrapper(user, str);
    char* msg = readInput(userStr);
    char* msgToSend;

    if(strcmp(msg, "Bye") == 0){
        msgToSend = msg;
    } else {
         msgToSend = strcatwrapper(userStr, msg);
    }
    sendMsgToServer(clntSock, msgToSend);
    if(strcmp(msg, "Bye") == 0){
        return true;
    }
    return false;
}


void runClientAsServer(int newPort, char* username, char* friendName){
    int servSock;
    int clntSock;
    struct sockaddr_in servAddr;
    struct sockaddr_in clntAddr;
    unsigned int clntLen; /* Length of client address data structure */
    bool isBye;
    char* receivedMsg;

    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    memset(&servAddr, 0, sizeof(servAddr)); /* Zero out structure */
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr =inet_addr("127.0.0.1");
    servAddr.sin_port = htons(newPort);
    
    if(bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        DieWithError("bind() failed");

    if(listen(servSock, MAXPENDING) < 0)
        DieWithError("listen() failed");

    printf("I am listening on %s:%d\n", inet_ntoa(servAddr.sin_addr), newPort);
    
    for(;;){
        clntLen = sizeof(clntAddr);

        if ((clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
            DieWithError("accept() failed");

        printf("%s connected on %s:%d\n", friendName, inet_ntoa(clntAddr.sin_addr), newPort);
    
        receivedMsg = receiveMsgFromServer(clntSock);
        printf("%s\n", receivedMsg);
    
        while(TRUE){
            isBye = sendMsgToFriend(clntSock, username);
            if(isBye){
                break;
            } else{
                receivedMsg = receiveMsgFromServer(clntSock);
                if(strcmp(receivedMsg, "Bye") == 0){
                    break;
                } else{
                    printf("%s\n", receivedMsg);
                }
            }
        }    
        close(clntSock);
        printf("Chat session ended.\n");
        break;
    }

}


int main(int argc, char *argv[]){

    int selectedOption;
    int clntSock;
    
    char* user;
    char* loggedinUser;
    
    while(TRUE){
        selectedOption = printAndSelectOption();

        switch(selectedOption){

            case 0:
                clntSock = connectWithServer();
                loggedinUser = login(clntSock);
                break;

            case 1:
                sendCaseOptionToServer(selectedOption, clntSock);

                char* numOfUSers = receiveMsgAndSendACKtoServer(clntSock);
                printf("There are %s users:\n", numOfUSers);

                for(int i = 0; i < atoi(numOfUSers); i++){
                    user = receiveMsgAndSendACKtoServer(clntSock);
                    printf("%s\n", user);
                }
                break;

            case 2:
                sendCaseOptionToServer(selectedOption, clntSock);
        
                user = readInput("Please enter username to chat with: ");
                if(strcmp(user, loggedinUser) == 0){
                    printf("%s cannot send a message to %s\n", user, loggedinUser);
                    continue;
                }
                sendMsgToServer(clntSock, user);
        
                char* msgToSend = readInput("Please type a message: ");
                sendMsgToServer(clntSock, msgToSend);
                if(strcmp(receiveMsgFromServer(clntSock), "Received") != 0)
                    DieWithError("Server did not receive the client's message.");
                printf("\nMessage sent successfully!\n");
                break;

            case 3:
                sendCaseOptionToServer(selectedOption, clntSock);

                char* totalMsgs = receiveMsgAndSendACKtoServer(clntSock);
                printf("You have %s message(s)!\n", totalMsgs);

                for(int i = 0; i < atoi(totalMsgs); i++){
                    char* msg = receiveMsgAndSendACKtoServer(clntSock);
                    printf("%s\n", msg);
                }
        
                break;

            case 4:
                sendCaseOptionToServer(selectedOption, clntSock);
                close(clntSock);    
                printf("--------------------Disconnected with the Server--------------------\n");
                char* newPortStr = readInput("Please enter the port you want to listen on: ");
                int newPort = atoi(newPortStr);
                runClientAsServer(newPort, loggedinUser, user);
                break;

            case 5:
                sendCaseOptionToServer(selectedOption, clntSock);
                close(clntSock);    
                printf("--------------------Disconnected with the Server--------------------\n");
        
                int newSock = connectWithServer();
                char* receivedMsg;
                bool isBye;
                while(TRUE){
                    isBye = sendMsgToFriend(newSock, loggedinUser);
                    if(isBye){
                        break;
                    } else{
                        receivedMsg = receiveMsgFromServer(newSock);
                        if(strcmp(receivedMsg, "Bye") == 0){
                            break;
                        } else{
                            printf("%s\n", receivedMsg);
                        }
                    }
                }
                close(newSock);
                printf("Chat session ended.\n");
                break;

            default:
                printf("Invalid option! Please select the correct option.\n");
                break;
        } // end of switch case

    } // end of while loop

    exit(0);

}

