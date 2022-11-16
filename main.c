/*===========================================================*/
/*Program: Ring                                              */
/*                                                           */
/*Description: A program that creates a data base in a ring  */
/*             form, using tcp and udp connecting. This data */
/*             base is abble to execute various operations.  */
/*                                                           */
/*Files:                                                     */
/*  (1) main.c                                               */
/*  (2) Makefile                                             */
/*                                                           */
/*Bibliography: Support papers given in the class            */ 
/*                                                           */
/* UC: RCI - IST-LEEC - 3rd year - 3rd period                */
/*                                                           */
/*Autorship: Eduardo Filipe Braz Barata 99930, IST-LEEC      */
/*                                                           */
/*                                          Last mod:14/04/22*/
/*===========================================================*/

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

/*********************************************************************************
* Macro name: PERROR()
*
* Arguments: s - Error Message 
*
* Side-effects: Closes the running program emitting an error message.
*
* Description: Sends an error message to stderr and closes the program
*
**********************************************************************************/
#define PERROR(s) {\
        printf("Something went wrong: %s\n",s);\
        exit(EXIT_FAILURE); }

/*********************************************************************************
* Macro name: distance()
*
* Arguments: a - 1st key
*            b - 2nd key 
*
* Side-effects: none to report.
*
* Description: Calculates the distance from a to b in the ring.
*
**********************************************************************************/
#define distance(a,b) ((b-a)%32 >=0 ? (b-a)%32 : 32+(b-a)%32 )

//Confirm free addr info

struct nodecontents{
    int nodenumber;
    char ipadress[64];
    char port[16];
};
struct nodeinformation{
    struct nodecontents self;
    struct nodecontents pred;
    struct nodecontents succ;
    struct nodecontents chord;
}THISNODE;

struct findstruct{
    short type;
    int seqnumber;
    int key;
    struct sockaddr_in addr;
    int udpserver;
};
struct findstruct findvec[100];

void help();
void HUB();
void Create_Servers(int *TCP, int *UDP);
int create_tcp_client(char desip[],char desport[]);
int user_interface(char input[],int *client, int *tcpserver, int *udpserver, int *successor,int *chordsocket,struct addrinfo **chord);
int pentry();
int messageanalyzer(char message[], int *oldsuccessor,int successor, int *pred, int chord, struct addrinfo *chordstruct);
void leave(int *client, int *tcpserver, int *udpserver, int *successor, int *chordsocket, struct addrinfo **chord);
int find(int successor, int key, int chord, struct addrinfo *chordinfo, int flag, int udpserver, struct sockaddr_in *addr);
int nodeverify(int nodenumber, char ipadress[], char port[]);
int receivetcpmessage(int *client, char buffer[]);

/*********************************************************************************
* Function name: main()
*
* Arguments: int argc - number of strings pointed to by argv[]
*            char *argv[] - argument vector
*
* Return: int - program exit code
*
* Side-effects: none to report.
*
* Description: Command line arguments collection.
*
**********************************************************************************/
int main(int argc, char * argv[]) {
    //argv[1] node Key, argv[2] ip adress, argv[3] port
    int node=0, i=0;
    srand((unsigned) time(NULL)); //assign random seed
    if(argc!=4){//There should be exactly 4 arguments
        printf("Too few arguments... Please use the following syntax: ./ring i i.IP i.port\n");
        exit(EXIT_SUCCESS);
    }
    if(sscanf(argv[1],"%d",&node)!=1){//Unable to read an integer from the 1st argument
        printf("Invalid node number\n");
        exit(EXIT_SUCCESS);
    }
    for(i=0;i<100;i++)//Inicializes the type of find in the findvec as -1
        findvec[i].type=-1;
    //Assign the colected data to the "self" struct, set the others as -1
    THISNODE.self.nodenumber=node;
    THISNODE.succ.nodenumber=-1;
    THISNODE.pred.nodenumber=-1;
    THISNODE.chord.nodenumber=-1;
    strcpy(THISNODE.self.ipadress,argv[2]);
    strcpy(THISNODE.self.port,argv[3]);
    //Verifies if the given node is valid
    if(nodeverify(THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port))
        exit(0);
    HUB();
}

/*********************************************************************************
* Function name: HUB()
*
* Arguments: 
*
* Return: void
*
* Side-effects: none to report.
*
* Description: This function is the core of the program. This is where
*              almost every file descriptor interruption will be handled 
*              and foward to its handling function.
*
**********************************************************************************/
void HUB(){
    socklen_t addrlen;
    struct sockaddr_in addr;
    struct addrinfo *res;
    int opt, n, udpserver=-1, tcpserverawnser=-1, tcpclient=-1, tcpserver=-1, oldclient=-1, chord=-1;
    fd_set inputs;
    char buffer[256];

    printf("Welcome! Please type a command or type \"h\" for help.\n");
    while(1){
        //Setting up flags for Select
        memset(buffer,0,sizeof buffer);
        FD_ZERO(&inputs); //Initializes "inputs" to have 0 bits for all file descriptors
        FD_SET(0,&inputs); //Set stdin channel on
        if(tcpserver!=-1 && udpserver!=-1){
            FD_SET(tcpserver,&inputs); //Set TCP server socket channel on
            FD_SET(udpserver,&inputs); //Set UDP server socket channel on
        }
        if(tcpclient!=-1)
            FD_SET(tcpclient,&inputs); //Set the tcp server we connected to socket channel on
        if(tcpserverawnser!=-1)
            FD_SET(tcpserverawnser,&inputs); //Set the tcp connected client socket channel on
        if(oldclient!=-1)
            FD_SET(oldclient,&inputs); //Set the tcp previously connected client socket channel on (only to read a 0)
        opt=select(FD_SETSIZE, &inputs, NULL, NULL, NULL);
        if(opt==-1){//Select error
            PERROR("Select");
        }
        else{
            if(FD_ISSET(0,&inputs)){//Interuption caused by stdin, load user interface
                if((n=read(0,buffer,256))!=0){//Reads string from stdin
                    if(n==-1)//Unable to read
                        PERROR("Read error");
                    buffer[n]=0;
                    if(user_interface(buffer,&tcpclient,&tcpserver,&udpserver,&tcpserverawnser,&chord,&res)==-1)
                        break;
                }
                printf("Please type a command or type \"h\" for help.\n");
            }
            if(FD_ISSET(tcpserver,&inputs)){//Interuption caused by TCP server socket
                oldclient=tcpserverawnser;
                tcpserverawnser=-1;
                addrlen=sizeof(addr);
                //Accepts a connection with client
                if((tcpserverawnser=accept(tcpserver,(struct sockaddr*)&addr,&addrlen))==-1)
                    PERROR("Accept error");
            }
            if(FD_ISSET(tcpserverawnser,&inputs)){//Interuption caused by our sucessor
                //Receive message from the client
                if(receivetcpmessage(&tcpserverawnser,buffer)){
                    if(messageanalyzer(buffer,&oldclient,tcpserverawnser,&tcpclient,chord,res)){
                        leave(&tcpclient,&tcpserver,&udpserver,&tcpserverawnser,&chord,&res);
                        return;
                    }
                }
            }
            if(FD_ISSET(oldclient,&inputs)){ //Interuption caused by the (still) connect old tcp client
                //Receive message from the client (we will only read a 0)
                receivetcpmessage(&oldclient,buffer);             
            }
            if(FD_ISSET(tcpclient,&inputs)){ //Interuption caused by our predecessor
                //Receive message from the server we connect to
                if(receivetcpmessage(&tcpclient,buffer)){
                    if(messageanalyzer(buffer,&oldclient,tcpserverawnser,&tcpclient,chord,res)){
                        leave(&tcpclient,&tcpserver,&udpserver,&tcpserverawnser,&chord,&res);
                        return;
                    }
                }
            }
            if(FD_ISSET(udpserver,&inputs)){//Interuption caused by UDP server socket (chord or node using bentry)
                addrlen=sizeof(addr);
                //Receive the message from the UDP client and send back an "ACK"
                if(recvfrom(udpserver,buffer,256,0,(struct sockaddr*)&addr,&addrlen)==-1)
                    PERROR("Receive error")
                if(sendto(udpserver,"ACK",3,0, (struct sockaddr*)&addr, addrlen)==-1)
                    PERROR("Sendto error");
                if(strstr(buffer,"EFND")!=NULL){//If we received an EFND we go straight to starting the find operation
                    if(sscanf(buffer,"%*s %d",&n)!=1){//Get the key we have to find in the ring
                        printf("Sscanf error. Bad message!\n");
                        leave(&tcpclient,&tcpserver,&udpserver,&tcpserverawnser,&chord,&res);
                        return;
                    }      
                    if(find(tcpserverawnser,n,chord,res,1,udpserver,&addr)){
                        leave(&tcpclient,&tcpserver,&udpserver,&tcpserverawnser,&chord,&res);
                        return;
                    }
                }
                else
                    messageanalyzer(buffer,&oldclient,tcpserverawnser,&tcpclient,chord,res);
            }
        }
    }
}

/*********************************************************************************
* Function name: Create_Servers()
*
* Arguments: int *TCP - TCP server socket
*            int *UDP - UDP server socket
*
* Return: void
*
* Side-effects: Modification of the value of the passed arguments.
*
* Description: Creation of a TCP and UDP socket.
*
**********************************************************************************/
void Create_Servers(int *TCP, int *UDP){
    int fd=0, n/*, flag=0*/;
    struct addrinfo hints, *res;
    //Create TCP socket and verify it
    if((fd = socket(AF_INET, SOCK_STREAM,0))==-1)
        PERROR("Failed to create socket");
  
    memset(&hints,0,sizeof(hints));
    //Assigning IP and PORT
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_STREAM; //TCP socket
    hints.ai_flags=AI_PASSIVE;
    n=getaddrinfo(NULL,THISNODE.self.port,&hints,&res);//Get addr info from the choosen port
    if(n!=0){
        fprintf(stderr,"getaadrinfo: %s\n", gai_strerror(n));
        exit(EXIT_FAILURE);
    }

    //Binding newly created socket to given IP and verification
    if(bind(fd,res->ai_addr,res->ai_addrlen)==-1)
        PERROR("Bind error");
    //The server is now ready to listen
    if(listen(fd,5)==-1)
        PERROR("Listen error");
    *TCP=fd;//assign the TCP variable with the new socket
    freeaddrinfo(res);
    fd=-1;
    memset(&hints,0,sizeof hints);
    memset(&res,0,sizeof res);
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_DGRAM; //Datagram socket (UDP)
    hints.ai_flags=AI_PASSIVE;
    //Create UDP socket and verify it
    if((fd = socket(AF_INET, SOCK_DGRAM,0))==-1)
        PERROR("Failed to create socket");
    n=getaddrinfo(NULL,THISNODE.self.port,&hints,&res);//Get addr info from the choosen port
    if(n!=0){
        fprintf(stderr,"getaadrinfo: %s\n", gai_strerror(n));
        exit(EXIT_FAILURE);
    }
    //Binding newly created socket to given IP and verification
    if(bind(fd,res->ai_addr, res->ai_addrlen)==-1)
        PERROR("Bind error");
    *UDP=fd;//assign the UDP variable with the new socket
    
    freeaddrinfo(res);
}

/*********************************************************************************
* Function name: create_tcp_client()
*
* Arguments: char desip[] - The ip we want to communicate with
*            char desport[] - The port we want to communicate with
*
* Return: int - socket trough which we can send messages
*
* Side-effects: none to report.
*
* Dependencies: There should be a server on the given Ip and Port or the program will abort.
*
* Description: Creation of a TCP client that connects to a server that
*              is located in the "desip" ip with the "desport" port.
*
**********************************************************************************/
int create_tcp_client(char desip[],char desport[]){
    int fd,n;
    struct addrinfo hints,*res;
    
    if((fd=socket(AF_INET,SOCK_STREAM,0))==-1) //Create a socket for tcp connection
        PERROR("Failed to create socket");

    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; //IPV4
    hints.ai_socktype=SOCK_STREAM; //TCP Socket
    //Gets the information from the server in order to connect to it
    n=getaddrinfo(desip,desport,&hints,&res);
    if(n!=0){
        fprintf(stderr,"getaadrinfo: %s\n", gai_strerror(n));
        return -1;
    }
    if(connect(fd,res->ai_addr,res->ai_addrlen)==-1){//Connects to the server
        printf("I wasn't able to connect with the server. Please check the ip and port.\n");
        close(fd);
        fd=-1;
    }
    freeaddrinfo(res);
    return fd;
}

/*********************************************************************************
* Function name: receivetcpmessage()
*
* Arguments: int *client - Socket that has information
*            char buffer[] - Vector to store the received message
*
* Return: int - 0 if the client was closed, 1 if we read a message
*
* Side-effects: The buffer variable will be changed
*
* Dependencies: The client must be an active socket
*
* Description: Reads the message that is waiting to be read in the "client" socket
*
**********************************************************************************/
int receivetcpmessage(int *client, char buffer[]){
    int n=0;
    n=read(*client,buffer,256);//Read message in the socket
    if(n==-1){//Error reading
        PERROR("Unable to read from socket");
    }
    else if(n==0){//The connection was closed by the other side
        close(*client);//Closse client and clear it
        *client=-1;
        return 0;
    }
    return 1;
}

/*********************************************************************************
* Function name: create_udp_client()
*
* Arguments: char desip[] - The ip we want to communicate with
*            char desport[] - The port we want to communicate with
*            struct addrinfo **res - Variable to store the addrinfo of the server
*                                    we are going to connect to
*
* Return: int - socket trough which we can send messages
*
* Side-effects: none to report.
*
* Dependencies: There should be a server on the given Ip and Port or the program will abort.
*
* Description: Creation of a UDP client that connects to a server that
*              is located in the "desip" ip with the "desport" port.
*
**********************************************************************************/
int create_udp_client(char desip[], char desport[],struct addrinfo **res){
    int fd,n;
    struct addrinfo hints;

    if((fd=socket(AF_INET,SOCK_DGRAM,0))==-1) //Create the UDP socket
        PERROR("Failed to create socket");
    memset(&hints,0,sizeof hints);
    hints.ai_family=AF_INET; //IPv4
    hints.ai_socktype=SOCK_DGRAM; //UDP socket
    n=getaddrinfo(desip,desport,&hints,res);
    if(n!=0){
        fprintf(stderr,"getaadrinfo: %s\n", gai_strerror(n));
        exit(EXIT_FAILURE);
    }

    return fd;
}

/*********************************************************************************
* Function name: sendudpmessage()
*
* Arguments: char message[] - The message to be sent
*            int udpclient - The socket through which we can communicate
*            struct addrinfo *client - Variable with the addrinfo of the server
*                                      we are going to send a message to
*            struct sockaddr_in *client2 - Variable with the addrinfo of the client
*                                      we are going to send a message to
*
* Return: int - 1 in case we can't send the message, 0 if we do
*
* Side-effects: none to report.
*
* Dependencies: There should be a udp socket created and the client variable should
*               have the information of the client we want to send the message to.
*
* Description: Sends a message using the UDP protocol through the given socket
*              by the variable "udpclient". This function will block the program
*              for 10 seconds if it doesn't receive an ACK from the receiver.
*              After those 10 seconds pass it will return 1. Otherwise, if everything
*              goes acording to plan it will return 0.
*
**********************************************************************************/
int sendudpmessage(char message[], int udpclient, struct addrinfo *client, struct sockaddr_in *client2){
    char buffer[128];
    int opt,n=0;
    fd_set inputs;
    struct timeval timeout;
    socklen_t addrlen;
    struct sockaddr_in addr;

    addrlen=sizeof(addr);
    if(client!=NULL)//We are sending a message to an UDP server
        if(sendto(udpclient,message,strlen(message),0,client->ai_addr,client->ai_addrlen)==-1)
            PERROR("Sendto error");
    if(client2!=NULL)//We are sendind a message to an UDP client
        if(sendto(udpclient,message,strlen(message),0,(struct sockaddr*)&(*client2), addrlen)==-1)
                PERROR("Sendto error");
    timeout.tv_sec=10;
    timeout.tv_usec=0;

    while(1){
        //Setting up flags for Select        
        FD_ZERO(&inputs); //Clears the inputs we are checking
        FD_SET(0,&inputs); //Set stdin channel on
        FD_SET(udpclient,&inputs); //Set the udpclient 
        opt=select(FD_SETSIZE, &inputs, NULL, NULL, &(timeout));
        if(opt==-1){//Select error
            PERROR("Select failure");
        }
        else if(opt==0)//Timed out after 10 seconds
            return 1;
        else{
            if(FD_ISSET(0,&inputs)){//Stdin produced the change, tell the user we are busy
                if((n=read(0,buffer,256))!=0)//Reads string from stdin
                    if(n==-1)//Unable to read
                        PERROR("Read error");
                printf("I'm trying to reach my udp client, please wait before using another command\n");                    
            }
            if(FD_ISSET(udpclient,&inputs)){//Received an answer to our sent message
                addrlen=sizeof(addr);
                memset(buffer,0,128);
                if(recvfrom(udpclient,buffer,128,0,(struct sockaddr*)&addr,&addrlen)==-1)
                    PERROR("Receive error");
                if(strcmp(buffer,"ACK")==0)//We were waiting for an ACK, if we get something else we ignore it
                    return 0;
            }                 
        }
    }
    return 0;
}

/*********************************************************************************
* Function name: messageanalyzer()
*
* Arguments: char message[] - The message that was sent
*            int *oldsuccessor - The previous sucessor socket (before we got another connection)
*            int successor - The current sucessor socket
*            int *pred - The predecessor socket
*            int chord - The shortcut socket
*            struct addrinfo *chordstruct - The shortcut addr information
*
* Return: int 1 - unsuccessful or badly formated message, 0 - successful
*
* Side-effects: If there is an "oldsucessor", the connection will be closed if we
*               receive a "SELF" message.
*
* Dependencies: The sockets that will be used should be already assigned to their
*               servers/ clients in order for the messages to be sent.
*
* Description: Analyzes which type of message was sent from the available types:
*              "SELF", "PRED", "FND", "RSP", "EFND" or "EPRED" and calls the function
*              responsible to handle them.
*
**********************************************************************************/
int messageanalyzer(char message[], int *oldsuccessor,int successor, int *pred, int chord, struct addrinfo *chordstruct){
    struct nodecontents aux;
    int findkey=0, sequencenumber=0, flag=0;
    char buffer[128], udpbuffer[128];

    aux.nodenumber=0;
    memset(buffer,0,128);
    memset(udpbuffer,0,128);
    if(message==NULL)
        return 1;
    if(strstr(message,"EPRED")!=NULL)//We will only receive a "EPRED" message here if its duplicate so we ignore it 
        return 0;
    if((strstr(message,"SELF")!=NULL || strstr(message,"PRED")!=NULL)){//Gets the information from the message
        if(sscanf(message,"%*s %d %s %s",&aux.nodenumber,aux.ipadress,aux.port)!=3){
            printf("Sscanf error. Bad message!\n");
            return 1;
        }
        if(nodeverify(aux.nodenumber,aux.ipadress,aux.port))
            return 1;
    }
    if((strstr(message,"FND")!=NULL || strstr(message,"RSP")!=NULL)){//Gets the information from the message
        if(sscanf(message,"%*s %d %d %d %s %s",&findkey,&sequencenumber,&aux.nodenumber,aux.ipadress,aux.port)!=5){
            printf("Sscanf error. Bad message!\n");
            return 1;
        }
        if(nodeverify(aux.nodenumber,aux.ipadress,aux.port))
            return 1;
    }
    if(strstr(message,"SELF")!=NULL){//Received a self message
        //This will only happen when the node that is already in a ring receives a SELF message from a node that used pentry or bentry
        //If the successor number is set at -1 then the node is not apart of a ring yet so its trying to join it
        if(THISNODE.succ.nodenumber!=-1 && *oldsuccessor!=-1){
            //Prepare a PRED message to be sent to the old sucessor node
            sprintf(buffer,"PRED %d %s %s\n",aux.nodenumber,aux.ipadress,aux.port);
            if(write(*oldsuccessor,buffer,strlen(buffer))==-1){//Send message
                printf("Write error. Wans't abble to send TCP message!\n");
                close(*oldsuccessor);
                *oldsuccessor=-1;
                return 1;
            }
        }
        if(distance(THISNODE.self.nodenumber,THISNODE.succ.nodenumber)>distance(THISNODE.self.nodenumber,aux.nodenumber))
            flag++;//We received a Self from a leave message so we will only want to store the data and leave
        THISNODE.succ.nodenumber=aux.nodenumber;//Store the data
        strcpy(THISNODE.succ.ipadress,aux.ipadress);
        strcpy(THISNODE.succ.port,aux.port);
        if((flag) || THISNODE.self.nodenumber==aux.nodenumber)
            return 0;//If this self came from a leave message or we received a message from our selfs 
    }
    //We received a PRED message or the node is alone in the ring
    if(strstr(message,"PRED")!=NULL || (aux.nodenumber==THISNODE.succ.nodenumber && THISNODE.self.nodenumber==THISNODE.pred.nodenumber)){
        //Update our predecessor
        THISNODE.pred.nodenumber=aux.nodenumber;
        strcpy(THISNODE.pred.ipadress,aux.ipadress);
        strcpy(THISNODE.pred.port,aux.port);
        //Prepare a message to send with our information
        sprintf(buffer,"SELF %d %s %s\n",THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);
        //Create a new client with our new predecessor
        if(((*pred)=create_tcp_client(aux.ipadress,aux.port))==-1)
            return 1;
        if(write((*pred),buffer,strlen(buffer))==-1){//Send a "SELF" message introducing our selfs
                printf("Write error. Wans't abble to send TCP message!\n");
                return 1;
            }
        return 0;
    }
    //We received either a FND or a RSP message and, because it can come through UDP
    //We need to make sure to add a \n to the end of the message in case we send it through TCP
    //Or remove the \n in case we want to send the message through UDP
    memset(buffer,0,sizeof buffer);
    if(strstr(message,"\n")==NULL)//Does it have a \n? if not we will add it to the end of the string
        strcat(message,"\n");
    strcpy(buffer,message);//Get the string to the buffer that will be sent through TCP
    strcpy(udpbuffer,message);//Get the string to the buffer that will be sent through UDP and remove the \n
    udpbuffer[strlen(message)-1]='\0';

    if(strstr(message,"FND")!=NULL){
        //Have we found the node with the awnser to this find? if so we will print a message with the RSP to the buffers
        if(distance(THISNODE.self.nodenumber,findkey)<distance(THISNODE.succ.nodenumber,findkey)){
            sprintf(buffer,"RSP %d %d %d %s %s\n",aux.nodenumber,sequencenumber,THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);
            sprintf(udpbuffer,"RSP %d %d %d %s %s",aux.nodenumber,sequencenumber,THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);
            findkey=aux.nodenumber;//Now that we have the awnser we are searching for the node that sent the message 1st
        }
        //Check if we have a chord and, if we do, where its better to send the message to, the chord or the sucessor
        if(THISNODE.chord.nodenumber== -1 || (distance(THISNODE.succ.nodenumber,findkey)<distance(THISNODE.chord.nodenumber,findkey))){
                //The sucessor has the lower distance, send message to it
                if(write(successor,buffer,strlen(buffer))==-1){
                    printf("Write error. Wans't abble to send TCP message!\n");
                    return 1;
                }
        }
        else
            if(sendudpmessage(udpbuffer,chord,chordstruct,NULL)==1){//Send message through the chord (lower distance)
                //There was something wrong when trying to send a message through the shortcut, we will then send the message through the sucessor
                printf("I wasn't abble to establish connection with my shortcut. I will be sending my message trough my sucessor\n");
                if(write(successor,buffer,strlen(buffer))==-1){//Sends message to the sucessor
                    printf("Write error. Wans't abble to send TCP message!\n");
                    return 1;
                }
            }
        return 0;
    }
    if(strstr(message,"RSP")!=NULL){//We received a RSP message
        //Check if the Awnser is directed to us or not
        if(findkey==THISNODE.self.nodenumber && findvec[sequencenumber-1].seqnumber==sequencenumber){//If it is
            if(findvec[sequencenumber-1].type==0){//The type of find is 0 - find was typed (more description on the find function)
                //Printf information to the terminal
                printf("The key %d was found in the ring! Here are the informations of the node that is storing it:\n \
                Node Number: %d \n \
                Ip Adress: %s \n \
                Port: %s \n",findvec[sequencenumber-1].key,aux.nodenumber,aux.ipadress,aux.port);
                printf("Please type a command or type \"h\" for help.\n");
                findvec[sequencenumber-1].type=-1;//Retire this find operation in case we get a duplicate message
                return 0;
            }
            else if(findvec[sequencenumber-1].type==1){//The type of find is 1 - bentry find (more description on the find function)
                //Send a EPRED message to the node that started the bentry operation
                sprintf(buffer,"EPRED %d %s %s",aux.nodenumber,aux.ipadress,aux.port);
                sendudpmessage(buffer,findvec[sequencenumber-1].udpserver,NULL,&findvec[sequencenumber-1].addr); //Ignore if we dont receive the ACK
                findvec[sequencenumber-1].type=-1; //Retire this find operation in case we get a duplicate message
            }
        }
        else{//The awnser is not directed to us. Continue sending the message
            //Check if we have a chord and, if we do, where its better to send the message to, the chord or the sucessor
            if(THISNODE.chord.nodenumber== -1 || (distance(THISNODE.succ.nodenumber,findkey)<distance(THISNODE.chord.nodenumber,findkey))){
                //The sucessor has the lower distance, send message to it
                if(write(successor,buffer,strlen(message))==-1){
                    printf("Write error. Wans't abble to send TCP message!\n");
                    return 1;
                }
            }
            else{
                if(sendudpmessage(udpbuffer,chord,chordstruct,NULL)==1){//Send message through the chord (lower distance)
                     //There was something wrong when trying to send a message through the shortcut, we will then send the message through the sucessor
                    printf("I wasn't abble to establish connection with my shortcut. I will be sending my message trough my sucessor\n");
                    if(write(successor,buffer,strlen(buffer))==-1){//Sends message to the sucessor
                        printf("Write error. Wans't abble to send TCP message!\n");
                        return 1;
                    }
                }
            }
        }
    }
    if(strstr(message,"SELF")!=NULL || strstr(message,"PRED")!=NULL || strstr(message,"FND")!=NULL || strstr(message,"RSP")!=NULL)
        return 0;
    //We received a message that isn't either of the above
    printf("Error: Bad message\n");
    return 1;
}

/*********************************************************************************
* Function name: newring()
*
* Arguments: none
*
* Return: void
*
* Side-effects: Modification of the values in the THISNODE structure.
*
* Description: Assigns the only existing node as its predecessor and sucessor.
*              Places the chord as unexisting.
*
**********************************************************************************/
void newring(){
    //Assings the pred, self and succ nodes as it self, sets the chord as inexistent
    THISNODE.pred.nodenumber=THISNODE.self.nodenumber;
    strcpy(THISNODE.pred.ipadress,THISNODE.self.ipadress);
    strcpy(THISNODE.pred.port,THISNODE.self.port);
    THISNODE.succ.nodenumber=THISNODE.self.nodenumber;
    strcpy(THISNODE.succ.ipadress,THISNODE.self.ipadress);
    strcpy(THISNODE.succ.port,THISNODE.self.port);
    THISNODE.chord.nodenumber=-1;
    strcpy(THISNODE.chord.ipadress,"\0");
    strcpy(THISNODE.chord.port,"\0");
}

/*********************************************************************************
* Function name: pentry()
*
* Arguments: none
*
* Return: int - new client socket created (predecessor of this node)
*
* Side-effects: Modification of the values in the THISNODE structure.
*
* Description: Clears the chord and sucessor node structure and sends a message
*              to the predessor contaning the information of this node.
*              (Performs the 1st step of the pentry operation)
*
**********************************************************************************/
int pentry(){
    char buffer[128];
    int client=0;
    memset(buffer,0,sizeof buffer);
    //Prepares the self message to be sent
    sprintf(buffer,"SELF %d %s %s\n",THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);
    //Sets the successor and chord nodes as inexisting
    THISNODE.succ.nodenumber=-1;
    strcpy(THISNODE.succ.ipadress,"\0");
    strcpy(THISNODE.succ.port,"\0");
    THISNODE.chord.nodenumber=-1;
    strcpy(THISNODE.chord.ipadress,"\0");
    strcpy(THISNODE.chord.port,"\0");
    //Creates a tcp client with the predecessor node
    if((client=create_tcp_client(THISNODE.pred.ipadress,THISNODE.pred.port))==-1)
        return -1;
    //Sends the "SELF" message, telling the node we want to enter the ring
    if(write(client,buffer,strlen(buffer))==-1){
        printf("Write error. Wans't abble to send TCP message!\n");
        return -1;
    }
    return client;//Returns the tcp client socket
    
}

/*********************************************************************************
* Function name: leave()
*
* Arguments: int *client - Socket through which we can comunicate with our predecessor
*            int *tcpserver - Socket of the tcpserver
*            int *udpserver - Socket of the udpserver
*            int sucessor - Socket through which we can comunicate with our sucessor
*            int *chordsocket - Socket through which we can comunicate with our shortcut if it exists
*            struct addrinfo **chord - Shortcut information struct if it exists
*
* Return: void
*
* Side-effects: Modification of the values in the THISNODE structure termination
*               of every socket that is open.
*
* Dependencies: The sucessor socket should already be created. 
*
* Description: Sends a message that informs the sucessor of their new predessor.
*              It will also clear the predecessor and successor of the current node for
*              futher use. It will then close every socket that is currently open in this node
*              (Performs the 1st step of the leave operation)
*
**********************************************************************************/
void leave(int *client, int *tcpserver, int *udpserver, int *successor, int *chordsocket, struct addrinfo **chord){
    char buffer[128];
    int flag=1;
    if(THISNODE.self.nodenumber==THISNODE.pred.nodenumber && THISNODE.self.nodenumber==THISNODE.succ.nodenumber)
        flag=0;//We are the only node in the ring so we wont be sending a message to ourselfs
    memset(buffer,0,sizeof buffer);
    //Prepares the PRED message to send to our sucessor informing it of its new predecessor
    sprintf(buffer,"PRED %d %s %s\n",THISNODE.pred.nodenumber,THISNODE.pred.ipadress,THISNODE.pred.port);
    //Clears the predecessor and successor nodes
    THISNODE.pred.nodenumber=-1;
    strcpy(THISNODE.pred.ipadress,"\0");
    strcpy(THISNODE.pred.port,"\0");
    THISNODE.succ.nodenumber=-1;
    strcpy(THISNODE.succ.ipadress,"\0");
    strcpy(THISNODE.succ.port,"\0");
    THISNODE.chord.nodenumber=-1;
    strcpy(THISNODE.chord.ipadress,"\0");
    strcpy(THISNODE.chord.port,"\0");
    if(flag)//We werent alone in the ring so we will send a message to our sucessor
        if(write(*successor,buffer,strlen(buffer))==-1){
            printf("Write error. Wans't abble to send TCP message!\n");
            flag=2;
        }
    //Close every socket that may be open, open sockets will have a value different to -1
    if(*successor!=-1)
        close(*successor);
    *successor=-1;
    if(*client!=-1)
        close(*client);
    *client=-1;
    close(*tcpserver);
    *tcpserver=-1;
    close(*udpserver);
    *udpserver=-1;
    if(*chordsocket!=-1){
        close(*chordsocket);
        *chordsocket=-1;
        freeaddrinfo(*chord);
    }
    if(flag==2)
        exit(EXIT_FAILURE);
}

/*********************************************************************************
* Function name: find()
*
* Arguments: int sucessor - Socket through which we can comunicate with our sucessor
*            int key - The key to be found
*            int chord - Socket through which we can comunicate with our shortcut if it exists
*            struct addrinfo *chordinfo - Shortcut information struct if it exists
*            int flag - Type o find we are doing 1- "normal" 2- bentry find
*            int udpserver - Socket through which we can comunicate with the node that connected
*                            to us through the bentry operation using udp
*            struct sockaddr_in *addr - UDP connected node information
*
* Return: int 1 - something went wrong, close program 0 - everything ok
*
* Side-effects: Modification of the findvec.
*
* Dependencies: The sockets that will be used should be already assigned to their
*               servers/ clients in order for the messages to be sent.
*
* Description: Assigns a random sequence number and starts the find operation.
*              If the node already has the answer we either send it to the UDP
*              connect node that used bentry or we print the information on the
*              screen. If this doesnt happen we will send a "FND" message through
*              either the shortcut or the sucessor node depending on which is closer.
*
**********************************************************************************/
int find(int successor, int key, int chord, struct addrinfo *chordinfo, int flag, int udpserver, struct sockaddr_in *addr){
    char buffer[128];
    int seqnumber=0;
    do{
        seqnumber= random()%100;//Generate a random number between 0 and 99
    }while(findvec[seqnumber-1].type!=-1);//Confirm if the generated number is already in use by a find operation
    findvec[seqnumber-1].type=flag;//This is either be 1 or 0 (check function brief)
    findvec[seqnumber-1].seqnumber=seqnumber;//Atribute the sequence number
    findvec[seqnumber-1].key=key;//Atribute the key that is being searched
    if(addr!=NULL && udpserver!=0){//Atribute the client we have to send the message to in case of this beeing an "EFND" operation
        findvec[seqnumber-1].addr=*addr;
        findvec[seqnumber-1].udpserver=udpserver;
    }
    //Check if the distance from us to the key is greater than the distance from our sucessor to the key
    //In case this is true we will send a FND message to our sucessor or chord
    if(distance(THISNODE.self.nodenumber,key)>distance(THISNODE.succ.nodenumber,key)){
        memset(buffer,0,sizeof buffer);
        sprintf(buffer,"FND %d %d %d %s %s\n",key,seqnumber,THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);
        //If we have a shortcut check which has the smallest distance to the key (the shortcut or the sucessor)
        if(THISNODE.chord.nodenumber== -1 || (distance(THISNODE.succ.nodenumber,key)<distance(THISNODE.chord.nodenumber,key))){
                if(write(successor,buffer,strlen(buffer))==-1){//Send the message to the sucessor
                    printf("Write error. Wans't abble to send TCP message!\n");
                    return 1;
                }
            }
            else{//Send the message to the shortcut because its at a shorter distance from the key
                buffer[strlen(buffer)-1]='\0';
                if(sendudpmessage(buffer,chord,chordinfo,NULL)==1){
                    printf("I wasn't abble to establish connection with my shortcut. I will be sending my message trough my sucessor\n");
                    if(write(successor,buffer,strlen(buffer))==-1){
                        printf("Write error. Wans't abble to send TCP message!\n");
                        return 1;
                    }
                }
            }
    }
    else{//We are the node that contains the key
        if(findvec[seqnumber-1].type==0){//It was a "normal" find, just print the information
                printf("The key %d was found in the ring! Here are the informations of the node that is storing it:\n \
                Node Number: %d \n \
                Ip Adress: %s \n \
                Port: %s \n",findvec[seqnumber-1].key,THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);
                findvec[seqnumber-1].type=-1;
                return 0;
            }
        else if(findvec[seqnumber-1].type==1){//It was an "EFND"
                sprintf(buffer,"EPRED %d %s %s",THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);
                //Send an "EPRED" message to the node that sent us a message
                sendudpmessage(buffer,udpserver,NULL,addr);//Ignore if we dont receive the ACK
                findvec[seqnumber-1].type=-1;
            }
        return 0;
    }
    return 0;
}

/*********************************************************************************
* Function name: bentry()
*
* Arguments: int bootnode - The number of the node we want to send the bentry message to
*            char bootip[] - The ip of the boot node
*            char bootport[] - The port of the boot node
*
* Return: int - socket through which we can communicate with our predecessor.
*
* Side-effects: None to report.
*
* Description: Creates an udp client that connects to the bootip node. It then
*              sends and udp message containg the "EFND" message and waits for a
*              "EPRED" message, calling the pentry function after this.
*
**********************************************************************************/
int bentry(int bootnode,char bootip[], char bootport[]){
    char buffer[128], message[128];
    int udpclient,opt, tcpclient,n=0, flag=0, timeoutflag=0;
    fd_set inputs;
    struct addrinfo *res;
    struct timeval timeout;
    socklen_t addrlen;
    struct sockaddr_in addr;
    //Create an udp client so that we can send a message to the node that will start the find operation
    udpclient=create_udp_client(bootip,bootport,&res);
    //Prepare the "EFND" message
    sprintf(buffer,"EFND %d",THISNODE.self.nodenumber);
    //Send the message to the client we created
    if(sendto(udpclient,buffer,strlen(buffer),0,res->ai_addr,res->ai_addrlen)==-1)
        PERROR("sendto error");
    while(1){
        memset(buffer,0,sizeof buffer);
        //Setting up flags for Select        
        FD_ZERO(&inputs); //Clears the inputs we are checking
        FD_SET(0,&inputs); //Set stdin channel on
        FD_SET(udpclient,&inputs);
        timeout.tv_sec=5;
        timeout.tv_usec=0;
        opt=select(FD_SETSIZE, &inputs, NULL, NULL, &(timeout));
        if(opt==-1){//Select error
            PERROR("select error");
        }
        else if(opt==0){//Select Timeout event
            timeoutflag++;
            if(timeoutflag==3){//Allow for 3 timeouts (15 seconds) before we receive the "EPRED"
                printf("After 3 sent messages and 15 seconds there was no response.\n");
                printf("Please confirm the node you are trying to connect to exists\n");
                close(udpclient);
                freeaddrinfo(res);
                return -1;
            }
            if(flag==1)//Still havent received an ACK so we will resend the message
               if(sendto(udpclient,buffer,strlen(buffer),0,res->ai_addr,res->ai_addrlen)==-1)
                    PERROR("sendto error");
        }
        else{
            if(FD_ISSET(0,&inputs)){//Interuption caused by stdin, load user interface
                if((n=read(0,buffer,256))!=0)//Reads string from stdin
                    if(n==-1)//Unable to read
                        PERROR("Read errror");
                //Inform the user that we are waiting for a response, ignore every command that he tries to use
                printf("There is an ongoing bentry operation, please wait before using another command\n");                    
            }
            if(FD_ISSET(udpclient,&inputs)!=0){//Received a message
                addrlen=sizeof(addr);
                memset(message,0,128);
                //Receives a message from the client we 1st sent a message to
                if(recvfrom(udpclient,message,128,0,(struct sockaddr*)&addr,&addrlen)==-1)
                    PERROR("Receive error");
                if(strcmp(message,"ACK")==0)
                    flag=0;//When we timeout we know that we timeout for lack of "EPRED" response
                else if(strstr(message,"EPRED")!=NULL){//Send an "ACK" back to the sender
                    if(sendto(udpclient,"ACK",3,0, (struct sockaddr*)&addr, addrlen)==-1)
                        PERROR("Sendto error");
                    break;
                }                 
            }
        }
    }
    close(udpclient);
    freeaddrinfo(res);
    //Handle the "EPRED" information received
    if(sscanf(message,"%*s %d %s %s",&THISNODE.pred.nodenumber,THISNODE.pred.ipadress,THISNODE.pred.port)!=3){
        printf("Sscanf error. Bad message!\n");
        return 1;
    }
    //Call the pentry so the node can enter in the ring
    if((tcpclient=pentry())==-1)
        return -1;
    return tcpclient;
}

/*********************************************************************************
* Function name: show()
*
* Arguments: none
*
* Return: void
*
* Side-effects: None to report
*
* Description: Prints the avaible information of the node on to the stdout.
*
**********************************************************************************/
void show(){
    printf("    Here are my informations:\n \
    Node Number: %d \n \
    Ip Adress: %s \n \
    Port: %s \n \n",THISNODE.self.nodenumber,THISNODE.self.ipadress,THISNODE.self.port);

    if(THISNODE.pred.nodenumber!=-1)//The node is in a ring
    printf("    Here are the informations of my predecessor\n \
    Node Number: %d \n \
    Ip Adress: %s \n \
    Port: %s \n \n\
    Here are the informations of my successor\n \
    Node Number: %d \n \
    Ip Adress: %s \n \
    Port: %s \n\n",THISNODE.pred.nodenumber,THISNODE.pred.ipadress,THISNODE.pred.port,
    THISNODE.succ.nodenumber,THISNODE.succ.ipadress,THISNODE.succ.port);
    else
        printf("    Im still not apart of any ring.\n\n");
    if(THISNODE.chord.nodenumber!=-1){//The node has a shortcut
    printf("    Here are the informations of my shortcut\n \
    Node Number: %d \n \
    Ip Adress: %s \n \
    Port: %s \n\n",THISNODE.chord.nodenumber,THISNODE.chord.ipadress,THISNODE.chord.port);
    }
}

/*********************************************************************************
* Function name: user_interface()
*
* Arguments: char input[] - received command from the user
*            int *client - Socket through which we can comunicate with our predecessor
*            int *tcpserver - Socket of the tcpserver
*            int *udpserver - Socket of the udpserver
*            int sucessor - Socket through which we can comunicate with our sucessor
*            int *chordsocket - Socket through which we can comunicate with our shortcut if it exists
*            struct addrinfo **chord - Shortcut information struct if it exists
*
* Return: int - (-1) Exit option, 0 every other option (should be ignored)
*
* Side-effects: Possible modification and closing of the sockets. 
*
* Dependencies: Depending of the option, the sockets that will be used should already be
*               assigned to their servers/ clients in order for the messages to be sent.
*
* Description: Analyzes which option was typed by the user, calling the necessary
*              functions to perform this action.
*
**********************************************************************************/
int user_interface(char input[],int *client, int *tcpserver, int *udpserver, int *successor, int *chordsocket, struct addrinfo **chord){
    int auxnode=0;
    static int flag=0;//0 - not apart of a ring, 1 - in a ring 2 - in a ring and with a shortcut
    char auxip[64]="\0",auxport[64]="\0", opt[64]="\0";
    if(input==NULL){//There is no string
        printf("Invalid option... Use the comand \"h\" for a list of available commands\n");
        return 0;
    }
    if(sscanf(input,"%s",opt)!=1){//Unable to read a string
        printf("Invalid option... Use the comand \"h\" for a list of available commands\n");
        return 0;
    }
    if(flag==0){//The node is not in a ring
            if(strcmp(opt,"n")==0 || strcmp(opt,"new")==0){ //Creation of a ring
                flag=1;//This node now belongs in a ring
                Create_Servers(tcpserver,udpserver);
                newring();
            }
            else if(strcmp(opt,"b")==0 || strcmp(opt,"bentry")==0){ //Bentry
                //Get a node number, an ip and a port from what the user typed
                if(sscanf(input,"%*s %d %s %s",&auxnode,auxip,auxport)!=3){
                        printf("Wrong format!\n");
                        return 0;
                    }
                if(nodeverify(auxnode,auxip,auxport))//Check if the information is valid
                    return 0;
                flag=1;//This node now belongs in a ring
                Create_Servers(tcpserver,udpserver);
                *client=bentry(auxnode,auxip,auxport);
                if(*client==-1){//We werent abble to join the ring
                    flag=0;//It wasnt abble to join the ring
                    close(*tcpserver);
                    close(*udpserver);
                    *tcpserver=-1;
                    *udpserver=-1;
                }
            }
            else if(strcmp(opt,"p")==0 || strcmp(opt,"pentry")==0){ //Pentry
                //Get a node number, an ip and a port from what the user typed
                if(sscanf(input,"%*s %d %s %s",&THISNODE.pred.nodenumber,THISNODE.pred.ipadress,THISNODE.pred.port)!=3){
                    printf("Wrong format!\n");
                    return 0;
                }
                //Check if the information is valid
                if(nodeverify(THISNODE.pred.nodenumber,THISNODE.pred.ipadress,THISNODE.pred.port))
                    return 0;
                Create_Servers(tcpserver,udpserver);
                if((*client=pentry())==-1)//We werent abble to create the client and enter the ring
                    return 0;
                flag=1;//This node now belongs in a ring
            }
            else if(strcmp(opt,"c")==0 || strcmp(opt,"chord")==0) //Chord
                printf("This node is not apart of any ring. Unable to use this option\n");

            else if(strcmp(opt,"d")==0 || strcmp(opt,"dchord")==0 || strcmp(opt,"echord")==0) //Echord now beeing dchord because the e option is already taken
                printf("This node is not apart of any ring. Unable to use this option\n");

            else if(strcmp(opt,"s")==0 || strcmp(opt,"show")==0) //Show
                show();

            else if(strcmp(opt,"f")==0 || strcmp(opt,"find")==0) //find
                printf("This node is not apart of any ring... Unable to start find command...\n");
                 
            else if(strcmp(opt,"l")==0 || strcmp(opt,"leave")==0) //leave
                printf("This node is not apart of any ring...\n");

            else if(strcmp(opt,"h")==0 || strcmp(opt,"help")==0) //help
                help();

            else if(strcmp(opt,"e")==0 || strcmp(opt,"exit")==0){ //Exit
                printf("Exiting program....\n");
                return -1;
            }
            else //Any other thing the user types
                printf("Invalid option... Use the comand \"h\" for a list of available commands\n");
    }
    else if(flag){//We are already in a ring
            if(strcmp(opt,"n")==0 || strcmp(opt,"new")==0) //new
                printf("Unable to create a ring with this node. This node already belongs to a ring.\n");
            
            else if(strcmp(opt,"b")==0 || strcmp(opt,"bentry")==0) //bentry
                printf("This node already belongs to a ring. Please remove it from the ring first\n");

            else if(strcmp(opt,"p")==0 || strcmp(opt,"pentry")==0) //pentry
                printf("This node already belongs to a ring. Please remove it from the ring first\n");

            else if(strcmp(opt,"c")==0 || strcmp(opt,"chord")==0){//chord
                if(flag==2){//We already have a shortcut, the user should delete it first
                    printf("This node already has a chord. Please delete it before trying to create another.\n");
                    return 0;
                }
                //Get a node number, an ip and a port from what the user typed
                if(sscanf(input,"%*s %d %s %s",&auxnode,auxip,auxport)!=3){
                    printf("Wrong format!\n");
                    return 0;
                }
                if(THISNODE.succ.nodenumber==auxnode){//It doesnt make sense to create a shortcut to our sucessor
                    printf("Please create a shortcut to another node other than your sucessor.\n");
                    return 0;
                }
                if(THISNODE.self.nodenumber==auxnode){//It doesnt make sense to create a shortcut to ourselfs
                    printf("Please create a shortcut to another node other than yourself.\n");
                    return 0;
                }
                //Check if the information is valid
                if(nodeverify(auxnode,auxip,auxport))
                    return 0;
                THISNODE.chord.nodenumber=auxnode;
                strcpy(THISNODE.chord.ipadress,auxip);
                strcpy(THISNODE.chord.port,auxport);
                *chordsocket = create_udp_client(auxip, auxport,chord);//Create an udp client to our shortcut
                flag=2;//It indicates we are in a ring and we have a shortcut
            }
            else if(strcmp(opt,"d")==0 || strcmp(opt,"dchord")==0 || strcmp(opt,"echord")==0){ //Echord now beeing dchord because the e option is already taken
                if(flag!=2){//Flag==2 if we have a chord created
                    printf("This node doesn't have a chord.\n");
                    return 0;
                }
                flag=1;//Set the flag as if we dont have a chord
                THISNODE.chord.nodenumber=-1;
                strcpy(THISNODE.chord.ipadress,"\0");
                strcpy(THISNODE.chord.port,"\0");
                close(*chordsocket);
                *chordsocket=-1;
                freeaddrinfo(*chord);
            }
            else if(strcmp(opt,"s")==0 || strcmp(opt,"show")==0) //Show
                show();
            else if(strcmp(opt,"f")==0 || strcmp(opt,"find")==0){//Find
                //Get the key from the user that should be searched
                if(sscanf(input,"%*s %d",&auxnode)!=1){
                    printf("Wrong format!\n");
                    return 0;
                }
                if(auxnode<0 || auxnode>32){//Check if the key is valid
                    printf("Invalid key to find!\n");
                    return 0;
                }
                if(find(*successor,auxnode, *chordsocket,*chord,0,0,NULL)){
                    leave(client,tcpserver,udpserver,successor,chordsocket,chord);
                    return -1;
                }
                return 0;
            }
            else if(strcmp(opt,"l")==0 || strcmp(opt,"leave")==0){//Leave
                leave(client,tcpserver,udpserver,successor,chordsocket,chord);
                flag=0;//This node is no longer apart of a ring
            }
            else if(strcmp(opt,"h")==0 || strcmp(opt,"help")==0)//Help
                help();
            else if(strcmp(opt,"e")==0 || strcmp(opt,"exit")==0){//Exit the program
                //To avoid erros we will first take the node out of the ring and then leave
                user_interface("leave",client,tcpserver,udpserver,successor,chordsocket,chord);
                printf("Exiting program....\n");
                return -1;
            }
            else //Any other thing the user types
                printf("Invalid option... Use the comand \"h\" for a list of available commands\n");
    }
    return 0;
}

/*********************************************************************************
* Function name: nodeverify()
*
* Arguments: int nodenumber - Number of the node we want to verify if its valid
*            char ipadress[] - The ip of said node
*            char port[] - The port of said node
*
* Return: int 1 - some type of error, 0 - everything is good
*
* Side-effects: None to report
*
* Description: Analyzes if the node number, ip adress, and port are valid in this
*              context. If they are, it will also simplify the ip adress by removing
*              left leading zeros and return this "new" ip.
*               
**********************************************************************************/
int nodeverify(int nodenumber, char ipadress[], char port[]){
    int aux=0, i, flag=0, newiplen=0, one,two, three,four;
    char auxip[128];

    memset(auxip,0,128);
    if(nodenumber<0 || nodenumber>32){//The node number should be between 1 and 32
        printf("Invalid node number! Please pick a number between 1 and 32.\n");
        return 1;
    }
    for(i=0;i<strlen(ipadress);i++){//Go through the string and count how many dots we have
        if(ipadress[i]=='.')
            aux++;
    }
    if(aux!=3){//there should be exactly 3 dots
        printf("Invalid ip address. Please write a valid one!\n");
        return 1;
    }
    i=0;
    while(ipadress[i]!='\0'){//Go throught the ip and remove the left zeros in each space
        if(ipadress[i]=='.'){//We are at a dot, continue to the next space
            auxip[newiplen++]=ipadress[i];//Copy to the new auxip
            flag=0;
            i++;
            continue;
        }
        if(ipadress[i]>'9' || ipadress[i]<'0'){//If we have a letter its invalid
            printf("Invalid ip address. Please write a valid one!\n");
            return 1;
        }
        if(flag==0){//We still havent found a number different than 0
            if(ipadress[i]!='0'){//if its diferent than 0 copy it to the new string and set the flag
                auxip[newiplen++]=ipadress[i];
                flag=1;
                i++;
            }
            else{
                if(ipadress[i+1]=='.')//If its a field with only a 0 we copy it, if not we dont copy it to the new string
                    auxip[newiplen++]=ipadress[i];
                i++;
            }
        }
        else{
            auxip[newiplen++]=ipadress[i];//Every number after the 1st non zero number in the field we copy
            i++;
        }

    }
    memset(ipadress,0,64);
    strcpy(ipadress,auxip);//Copy the new, simplified ip and replace the old one
    //get each number from each field of the ip
    if(sscanf(ipadress,"%d %*c %d %*c %d %*c %d",&one, &two,&three,&four)!=4){
        printf("Invalid ip address. Please write a valid one!\n");
        return 1;
    }
    //Check if each number is between 0 and 254
    if((one<0||one>254)||(two<0||two>254)||(three<0||three>254)||(four<0||four>254)){
        printf("Invalid ip address. Please write a valid one!\n");
        return 1;
    }
    //Get a port number
    if(sscanf(port,"%d",&one)!=1){
        printf("Invalid node port. Please write a valid one\n");
        return 1;
    }
    if(one<1 || one>65535){//The port should be superior to 0 and less than 65535
        printf("Invalid node port. PLease write a valid one!\n");
        return 1;
    }
    for(i=0;i<strlen(port);i++){//Check if the user only wrote numbers in the port field
        if(port[i]<'0'||port[i]>'9'){
            printf("Invalid node port. PLease write a valid one!\n");
            return 1;
        }
    }
    return 0;
}

/*********************************************************************************
* Function name: help()
*
* Arguments: none
*
* Return: void
*
* Side-effects: None to report
*
* Description: prints a help message to the terminal.
*               
**********************************************************************************/
void help(){
    printf("\n\tHere are the available commands:\n\n \
    n - Creation of a ring containg only the node. \n\n \
    b [boot] [boot.IP] [boot.port] - \n \
        Entry of the node in the ring in which belongs the node \n \
        \"boot\" with IP adress \"boot.IP\" and port \"boot.port\".\n\n \
    p [pred] [pred.IP] [pred.port] - \n \
        Entry of the node in the ring knowing that its predeces-\n \
        -sor is the node \"pred\" with IP adress \"pred.IP\" and\n \
        port \"pred.port\".\n\n \
    c [i] [i.IP] [i.port] - \n \
        Creation of a shortcut to the node \"i\" with IP adress \n \
        \"i.IP\" and port \"i.port\".\n\n \
    d - Delete the shortcut. \n\n \
    s - Shows the node state.\n\n \
    f [k] - \n \
        Looks for the \"k\" key, returning the key, the IP adress, \n \
        and the node port to which it belongs. \n\n \
    l - Exit the ring node.\n\n \
    h - Shows this help message.\n\n \
    e - Exits the application.\n\n");
}
