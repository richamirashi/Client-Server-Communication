#include <stdio.h>
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h> /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h> /* for atoi() */
#include <string.h> /* for memset() */
#include <unistd.h> /* for close() */
#include <stdbool.h>

#define MAXPENDING 5
#define RCVBUFFSIZE 1024

typedef struct USER_CREDENTIALS{
    char* username;
    char* password;
} UserCred;


void DieWithError(char *errorMessage);


UserCred credData[10];  // Max 10 users
int totalUsers = 0;

void setup(){
    char* line = NULL;
    size_t linelen = 0;
    ssize_t read; 
    char* token;

    FILE* file = fopen("creds.txt", "r");
    if(file == NULL)
	    DieWithError("Error opening file");

    while((read = getline(&line, &linelen, file)) != -1){
	    token = strtok(line, ":");
	    credData[totalUsers].username = malloc(strlen(token));
	    strcpy(credData[totalUsers].username, token);	
	    token = strtok(NULL, ":");
	    token = strtok(token, "\n");
	    credData[totalUsers].password = malloc(strlen(token));
	    strcpy(credData[totalUsers].password, token);	
	    totalUsers++;
    }

    fclose(file);
}


char* readFromSocket(int clntSock){
    char* msg;
    int msgSize;
    char rcvBuff[RCVBUFFSIZE];

    if((msgSize = recv(clntSock, rcvBuff, RCVBUFFSIZE - 1, 0)) < 0)
	    DieWithError("Reading msg from socket failed.\n");
    
    rcvBuff[msgSize] = '\0';
    msg = malloc(strlen(rcvBuff));
    strcpy(msg, rcvBuff);
    memset(rcvBuff, 0, sizeof(rcvBuff));
    return msg;
}


void sendMsgToClient(int clntSock, char* msgToSend){
    if(send(clntSock, msgToSend, strlen(msgToSend), 0) != strlen(msgToSend))
	    DieWithError("Msg sending to Client failed.\n");
}


bool isValidLogin(char* uname, char* pass){
    for(int i = 0; i < totalUsers; i++){
	    if( (strcmp(uname, credData[i].username)) == 0 && (strcmp(pass, credData[i].password)) == 0 ){
	        return true;
	    }
    }
    return false;
}


void sendMsgToClientwithACK(int clntSock, char* msg){
    sendMsgToClient(clntSock, msg);
    char* ack = readFromSocket(clntSock);
}


void returnNumOfUsersToClient(int clntSock){
    char buff[15];
    int numsOfCharsWritten = sprintf(buff, "%d", totalUsers);
    char* numOfUsersStr = malloc(numsOfCharsWritten);
    strcpy(numOfUsersStr, buff);
    sendMsgToClientwithACK(clntSock, numOfUsersStr);
}


void returnUserListToClient(int clntSock){
    char* username;
    for(int i = 0; i < totalUsers; i++){
	    username = credData[i].username;
	    sendMsgToClientwithACK(clntSock, username);
    }
}


char* receiveMsgWithACK(int clntSock){
    char* msg = readFromSocket(clntSock);
    char* ACK = "Received";
    sendMsgToClient(clntSock, ACK);
    return msg;
}


char* saveMessage(char* aMsgToUser, int clntSock){
    FILE* file;
    char* username = malloc(strlen(aMsgToUser));
    strcpy(username, aMsgToUser);
    file = fopen(strcat(username,".txt"), "a");
    char* msg = receiveMsgWithACK(clntSock);
    fputs(msg, file);
    fputs("\n", file);
    fclose(file);
    return msg;
}


char* strcatwrapper(char* str1, char* str2){
    char* concatenatedStr;
    concatenatedStr = malloc(strlen(str1) + strlen(str2));
    strcpy(concatenatedStr, str1);
    strcat(concatenatedStr, str2);
    return concatenatedStr;
}


void sendBackMsgsToClient(int clntSock, char* username){
    char* line = NULL;
    size_t linelen = 0;
    ssize_t read; 
    char* msg;
    char buff[10];
    char* msgsList[10];
    int totalLines = 0;

    char* fileName = strcatwrapper(username, ".txt");
    
    FILE* file = fopen(fileName, "r");
    if(file == NULL){
        msg = malloc(1);
        strcpy(msg, "0");
        sendMsgToClientwithACK(clntSock, msg);
    } else {
    
        while((read = getline(&line, &linelen, file)) != -1){
    	    msg = malloc((int)(linelen));
    	    strcpy(msg, line);
    	    msgsList[totalLines] = msg;	
    	    totalLines++;
        }
    
        int numsOfCharsWritten = sprintf(buff, "%d", totalLines);
        char* totalLinesStr = malloc(numsOfCharsWritten);
        strcpy(totalLinesStr, buff);
        sendMsgToClientwithACK(clntSock, totalLinesStr);
    
        for(int i = 0; i < totalLines; i++){
        	sendMsgToClientwithACK(clntSock, msgsList[i]);
        }
      
        fclose(file);
    }
}


int main(int argc, char *argv[]){
    int servSock;
    int clntSock;
    struct sockaddr_in servAddr;
    struct sockaddr_in clntAddr;
    unsigned short servPort;
    unsigned int clntLen; /* Length of client address data structure */
    int clntCounter = 0;

    char* username;
    char* password;
    char* loginStatus;

    char* optionChar;
    int option;

    char* aMsgToUser;

    /* Test for correct number of arguments */
    if (argc != 1){
        fprintf(stderr, "Usage: %s <Server Port>\n", argv[0]) ;
        exit(1);
    }

    servPort = 8000;

    /* Create socket for incoming connections */
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr)); /* Zero out structure */
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr =inet_addr("127.0.0.1");
    servAddr.sin_port = htons(servPort);

    /* Bind to the local address */
    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        DieWithError("bind() failed");

    /* Mark the socket so it will listen for incoming connections */
    if (listen(servSock, MAXPENDING) < 0)
        DieWithError("listen() failed");

    printf("Server started!\n");
    printf("Listening on %s:%d\n", inet_ntoa(servAddr.sin_addr), servPort);

    setup();

    for(;;){
        // Set the size of the in-out parameter
        clntLen = sizeof(clntAddr);

	    // Wait for a client to connect
        if ((clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
            DieWithError("accept() failed");
	
	    clntCounter++;

	    // clntSock is connected to a client!
        printf("Client %d connected on %s:%d\n", clntCounter, inet_ntoa(clntAddr.sin_addr), servPort);

	    username = readFromSocket(clntSock);
	    printf("Login Username: %s\n", username);
	    password = readFromSocket(clntSock);
	    printf("Login Password: %s\n", password);

	    if(!isValidLogin(username, password)){
	        loginStatus = "Invalid";
	        sendMsgToClient(clntSock, loginStatus);
	        continue;
	    }

	    loginStatus = "Successful";
	    sendMsgToClient(clntSock, loginStatus);

	    while(1){

	        optionChar = readFromSocket(clntSock);
	        option = atoi(optionChar);

	        switch(option){

	    	    case 1:
	  	            returnNumOfUsersToClient(clntSock);
	    	        returnUserListToClient(clntSock);
	    	        printf("Returned user list!\n");
		            break;

	    	    case 2:
	    	        aMsgToUser = readFromSocket(clntSock);
	    	        printf("A message to: %s\n", aMsgToUser);
		            char* msg = saveMessage(aMsgToUser, clntSock);
		            printf("Message is: %s\n", msg);
		            break;

		        case 3:
		            printf("Send back %s's msgs!\n", username);
		            sendBackMsgsToClient(clntSock, username);
		            break;

	    	    case 4:
		            printf("Client %d disconnected!\n", clntCounter);
		            close(clntSock);
		            break;

	    	    case 5:
		            printf("Client %d disconnected!\n", clntCounter);
		            close(clntSock);
		            break;

		        default:
		            printf("Client typed invalid option.\n");
		            break;

	        } // end of switch
                
 	        if(option <= 0 || option > 3) {
	    	    break;
	        }

	    } // end of while
    } // end of for loop

    return 0;

}

