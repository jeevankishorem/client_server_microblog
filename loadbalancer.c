#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
//#define SIZE sizeof(struct sockaddr_in)

/*typedef struct server_info {
        char ipaddr[15];
        int port;
        pthread_mutex_t th_mutex;
} server_dt;*/

#define MAXBUFSIZE 4096

typedef struct server_database {
    int server_id;
    char ipaddr[15];
    int port;
    int count;
    int is_alive;
    pthread_mutex_t th_mutex;
} server_dt;


server_dt *primary_server;
server_dt *secondary_server;

typedef struct arguments {
    int sock;
} args_dt;

typedef struct rqst {
    char *command;
    int handler;
    long int datalength;
    char *data;
} tweet_req_dt;

typedef struct rslt {
    char command[15];
    int st_code;
    long int dlength;
    char *data;
} tweet_resp_dt;

struct sockaddr_in server;
void server_check();
void tcp();
void create_newconnect();
void session(void *q);
void timer();
char *recv_buf(int sock);


int main (int argc, char **argv)
{
    int msock, ssock,clen;
    int portno;

    struct sockaddr_in server,cli;
    struct hostent *cli_details;
    char *caddr;
    pthread_t tid;
    pthread_attr_t tattr;

    //Test for correct number of arguments
    if (argc != 2)
    {
        fprintf(stderr, "Usage: <filename> <port>");
        exit(0);
    }



    primary_server = (server_dt *)malloc(sizeof(server_dt));
    secondary_server = (server_dt *)malloc(sizeof(server_dt));

    primary_server->server_id = 1;
    strcpy(primary_server->ipaddr , "127.0.0.1");
    primary_server->port = 6001;
    primary_server->count = 0;
    primary_server->is_alive = 0;

    secondary_server->server_id = 2;
    strcpy(secondary_server->ipaddr ,"127.0.0.1");
    secondary_server->port = 7001;
    secondary_server->count = 0;
    secondary_server->is_alive = 0;


    //server_dt *global_server;
    //global_server = (server_dt *)malloc(sizeof(server_dt));

    portno = atoi(argv[1]);//convert  port # to int
    server.sin_family = AF_INET;
    server.sin_port = htons(portno);
    server.sin_addr.s_addr = htonl(INADDR_ANY);//use client ip address

    msock = socket(PF_INET,SOCK_STREAM,0);
    if (msock<0) {
      printf("Error: Failed to create a socket.\n");
      exit(0);
    }

    if (bind(msock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Error: Failed to bind the socket.\n");
        exit(0);
    }
    

    if (listen(msock,5) <0) {
        printf("Error: Failed to set socket in passive mode.\n");
        exit(0);
    }

    printf("\n\n****************LOAD BALANCER********************\n\n");

    while(1) {

      clen = sizeof(cli);
      if ((ssock = accept(msock, (struct sockaddr *)&cli,&clen)) < 0) {
        printf("Error: Failed to connect to remote host.\n");
        exit(0);
      }

      cli_details = gethostbyaddr((const char *)&cli.sin_addr.s_addr, sizeof(cli.sin_addr.s_addr), PF_INET);
      caddr = inet_ntoa(cli.sin_addr);

     // printf("Connection established to %s(%s) \n",(char *)cli_details->h_name,caddr);


      /*args_dt *args = (args_dt *)malloc(sizeof(args_dt));
      args->sock = ssock;
      args->server_list[0] = primary_server;
      args->server_list[1] = secondary_server;*/

      pthread_attr_init(&tattr);
      pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
      pthread_mutex_init(&primary_server->th_mutex,0);
      pthread_mutex_init(&secondary_server->th_mutex,0);


      if (pthread_create(&tid,&tattr,(void*(*)(void *))session,(void *)ssock) < 0) {
        printf("Error: Failed to create a new thread.\n");
        exit(0);
      }
    }
    return 0;
}

void session(void *q)
{
    int sock = (int)q;
    char *request, *reply, *format;
    char *tmp1, *data1, *data2;
 //   th_args_dt *args = (th_args_dt *)q;
    pthread_t tid;
    pthread_attr_t tattr;
    tweet_req_dt *tweet_req = (tweet_req_dt *)malloc(sizeof(tweet_req_dt));

    tweet_resp_dt *result= (tweet_resp_dt *)malloc(sizeof(tweet_resp_dt));
    /*receive buffer*/

    request = recv_buf(sock);
    //printf("Request received: %s\n",request);
    
    char *tmp;
    tmp = strtok(request," ");
    tweet_req->command = tmp;

    if (strcmp(tweet_req->command,"CLIENT") == 0) {
        printf("CLIENT Requested the server address...\n");

        char ipaddr[15];
        int port;
        
        pthread_mutex_lock(&primary_server->th_mutex);
        if (primary_server->is_alive == 1) {
          strcpy(ipaddr,primary_server->ipaddr);
          port = primary_server->port;
        }
        else {

          pthread_mutex_lock(&secondary_server->th_mutex);
          strcpy(ipaddr,secondary_server->ipaddr);
          port = secondary_server->port;
          pthread_mutex_unlock(&secondary_server->th_mutex);

        }
        pthread_mutex_unlock(&primary_server->th_mutex);


        pthread_mutex_lock(&primary_server->th_mutex);
        if (primary_server->is_alive == 1) {
          result->st_code = 200;
        }
        else {

          pthread_mutex_lock(&secondary_server->th_mutex);
          result->st_code = 400;
          pthread_mutex_unlock(&secondary_server->th_mutex);

        }
        pthread_mutex_unlock(&primary_server->th_mutex);


        strcpy(result->command,tmp);
        result->dlength = (strlen(ipaddr) + 3);
        format = "%s %d %ld\r\n\r\n%s %d\0";
        reply = (char *)malloc(strlen(format)+strlen(result->command)+sizeof(result->st_code)+sizeof(result->dlength)+strlen(ipaddr)+sizeof(port));
        sprintf(reply,format,result->command,result->st_code, result->dlength,ipaddr,port);
        printf("\nData Sent:%s\n",reply);

        send_buf(sock,reply);
        free(reply);
        free(request);
        free(result);
        free(tweet_req);
        close(sock);
        pthread_exit(0);
    }
    else if (strcmp(tweet_req->command,"SERVER") == 0) {
        int count;

        tmp = strtok(NULL,"\0");
        int server_id = atoi(tmp);
        
        printf("Keep Alive message received from SERVER%d\n",server_id);
        time_t ltime;
        ltime=time(NULL); /* get current cal time */
        printf("Message Timestamp: %s\n",asctime(localtime(&ltime)));


        if (server_id == 1) {
          pthread_mutex_lock(&primary_server->th_mutex);
          primary_server->count++;
          count = primary_server->count;
          pthread_mutex_unlock(&primary_server->th_mutex);
        }
        else if(server_id == 2) {
          pthread_mutex_lock(&secondary_server->th_mutex);
          secondary_server->count++;
          count = secondary_server->count;
          pthread_mutex_unlock(&secondary_server->th_mutex);
        }
        /*
        if (server_id == 1) {
          pthread_mutex_lock(&primary_server->th_mutex);
          printf("TIMER START: Server:>%d< & Count:>%d< & Primary Count:>%d<\n",server_id,count,primary_server->count);
          pthread_mutex_unlock(&primary_server->th_mutex);
        }
        else {
          pthread_mutex_lock(&secondary_server->th_mutex);
          printf("TIMER START: Server:>%d< & Count:>%d< & Secondary Count:>%d<\n",server_id,count,secondary_server->count);
          pthread_mutex_unlock(&secondary_server->th_mutex);
        }*/

        sleep(70);

        
        if (server_id == 1) {
            pthread_mutex_lock(&primary_server->th_mutex);
            //printf("TIMER END: Server:>%d< & Primary Count:>%d<\n",server_id,primary_server->count);
            if (count <= primary_server->count) {
              primary_server->is_alive = 1;
              printf("Server%d is ALIVE!!!\n\n",server_id);
            }
            else {
              primary_server->is_alive = 0;
              printf("Server%d is DOWN!!!\n\n",server_id);
            }
            primary_server->count--;
            pthread_mutex_unlock(&primary_server->th_mutex);
        }
        else if (server_id == 2){
            pthread_mutex_lock(&secondary_server->th_mutex);
            //printf("TIMER END: Server:>%d< & Secondary Count:>%d<\n",server_id,secondary_server->count);
            if (count <= secondary_server->count) {
              secondary_server->is_alive = 1;
              printf("Server%d is ALIVE!!!\n\n",server_id);
            }
            else {
              secondary_server->is_alive = 0;
              printf("Server%d is DOWN!!!\n\n",server_id);
            }
            secondary_server->count--;
            pthread_mutex_unlock(&secondary_server->th_mutex);

        }


        /*pthread_mutex_unlock(&primary_server->th_mutex);

        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);

        if (pthread_create(&tid,&tattr,(void*(*)(void *))timer,NULL) < 0) { 
          printf("Error: Failed to start the timer thread.\n");
          exit(0);
        }*/

        pthread_exit(0);
    }
}


/*void timer()
{

  pthread_mutex_lock(&primary_server->th_mutex);
  primary_server->count = 1;
  pthread_mutex_unlock(&primary_server->th_mutex);

  sleep(350);

  pthread_mutex_lock(&primary_server->th_mutex);
  if (primary_server->count == 0) primary_server->is_alive = 1;
  else primary_server->is_alive = 0;
  pthread_mutex_unlock(&primary_server->th_mutex);

  pthread_exit(0);

}*/


void send_buf(int sock, char *buffer)
{
    int sdata, sbits = 0;
    while(sbits < strlen(buffer))
    {   
        if ((sdata = send(sock, buffer, strlen(buffer)-sbits, 0)) <0){
            printf("Error: Failed to send data to remote server.\n");
            exit(0);
        }
        else sbits += sdata;
    }
}


char *recv_buf(int sock)
{   

    int size_buf=MAXBUFSIZE;
    int sbits;
    char *buffer = (char *)malloc(MAXBUFSIZE);

    memset(buffer, NULL, sizeof(buffer));
    while ((sbits=recv(sock, buffer, size_buf-1, 0)) > 0) {
        //("Write buffer: %s\n", buffer);
        if (strchr(buffer, '\0') != NULL )
            break;
        size_buf-=sbits;
    }

    return buffer;
}
