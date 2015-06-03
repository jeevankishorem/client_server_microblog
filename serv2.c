#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <libpq-fe.h>
#include <pthread.h>

#define MAXBUFSIZE 4096

typedef struct th_args {                //A structure for thread's arguments.
    int     sock;
    PGconn  *conn;
    pthread_mutex_t th_mutex;
} th_args_dt;

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

void tweet_server();
PGconn *connectdb();
int tcp_sock(int port);
tweet_resp_dt *login(PGconn *conn,char *usr, char *pwd);
tweet_resp_dt *signup(PGconn *conn,char *usr, char *pwd);
tweet_resp_dt *logout(PGconn *conn,int handler);
char *recv_buf(int sock);
void send_buf(int sock, char *buffer);
tweet_resp_dt *process_query( PGconn * conn, char *command, const char * query_text);
void loadbalancer();

int main(int argc, char **argv)
{
    PGconn     *conn;
    int     msock, ssock, clen;
    struct  sockaddr_in clin;
    char    *caddr;
    pthread_t tid, lbid;
    pthread_attr_t tattr, lattr;


    if (argc != 2) {
        printf("Error: <filename> <portno>\n");
        exit(0);
    }

    int portno = atoi(argv[1]);
    //Connect to the backend database server.
    conn = connectdb();

    //Create a TCP socket.
    msock = tcp_sock(portno);

    printf("\n****MICROBLOGGING SERVER****\n");

    pthread_attr_init(&lattr);
    pthread_attr_setdetachstate(&lattr,PTHREAD_CREATE_DETACHED);

    if (pthread_create(&lbid, &lattr, (void*(*)(void *))loadbalancer, (void *)NULL) < 0)
    {
        printf("Error: Unable to create the start thread.\n");
        exit(0);
    }



    while (1) {

        printf("\nWaiting for the remote host...\n");
        clen = sizeof(clin);
        if ((ssock = accept(msock, (struct sockaddr *)&clin,&clen)) < 0) {
            printf("Error: Failed to connect to remote host.\n");
            exit(0);
        }

        caddr = inet_ntoa(clin.sin_addr);
        printf("Established conn to %s[:%d]\n", caddr,htons(clin.sin_port));

        th_args_dt *args;
        args = (th_args_dt *)malloc(sizeof(th_args_dt));
        args->sock = ssock;
        args->conn = conn;

        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
        pthread_mutex_init(&args->th_mutex,0);

        if (pthread_create(&tid, &tattr, (void*(*)(void *))tweet_server, (void *)args) < 0)
        {
            printf("Error: Unable to create the start thread.\n");
            exit(0);
        }
        //pthread_join(tid, NULL);
        pthread_attr_destroy(&tattr);
        pthread_mutex_destroy (&args->th_mutex);
        //close(ssock);
    }

    PQfinish(conn);
   // close(msock);

    return 0;
}

void tweet_server(void *q)
{

    char *request, *reply, *format, *query;
    th_args_dt *args = (th_args_dt *)q;
    
    tweet_req_dt *tweet_req = (tweet_req_dt *)malloc(sizeof(tweet_req_dt));

    tweet_resp_dt *result;
    /*receive buffer*/
    pthread_mutex_lock(&args->th_mutex);
    request = recv_buf(args->sock);
    pthread_mutex_unlock(&args->th_mutex);
    printf("Request received: %s\n",request);
    
    char *dup = (char *)malloc(strlen(request));
    strcpy(dup,request);
    char *tmp;
    tmp = strtok(dup," ");
    tweet_req->command = tmp;
    tmp = strtok(NULL," ");
    tweet_req->handler = atoi(tmp);
    tmp = strtok(NULL,"\r\n\r\n");
    tweet_req->datalength = atoi(tmp);
    tmp = strtok(NULL,"\0");
    tmp = tmp+3*sizeof(char);
    tweet_req->data = tmp;
    //printf("Command:%sHandler:%dDatalength:%ldData:%s\n",tweet_req->command, tweet_req->handler, tweet_req->datalength, tweet_req->data);


    /*LOGIN and SIGNUP request handling*/
    if (tweet_req->handler <= 0) {//if the user is asking for login request
            char *tmp1, *username, *password;
            tmp1 = strtok(tweet_req->data," ");
            username = tmp1;
            tmp1 = strtok(NULL,"\0");
            password = tmp1;
            printf("Received username: %s & password: %s\n",username,password);

            if (strcmp(tweet_req->command,"LOGIN") == 0) {  
                printf("Login Requested. Authenticating '%s'...\n",username);
                pthread_mutex_lock(&args->th_mutex);
                result = login(args->conn,username,password);
                pthread_mutex_unlock(&args->th_mutex);
                //printf("%s %d %ld %s\n", result->command,result->st_code, result->dlength,result->data);
            }
            else if (strcmp(tweet_req->command,"SIGNUP") == 0) {    
                printf("Signup Requested. Creating user account for '%s'...\n",username);
                pthread_mutex_lock(&args->th_mutex);
                result = signup(args->conn,username,password);
                pthread_mutex_unlock(&args->th_mutex);         
                //printf("%s %d %ld %s\n", result->command,result->st_code, result->dlength,result->data);
            }
    }
    /*other Request Handling*/
    else {
        if (strcmp(tweet_req->command,"LOGOUT") == 0) {
            printf("LOGOUT Requested. Closing session...");
            pthread_mutex_lock(&args->th_mutex);
            result = logout(args->conn,tweet_req->handler);
            pthread_mutex_unlock(&args->th_mutex);    
            //printf("%s %d %ld %s\n", result->command,result->st_code, result->dlength,result->data);
        }
        else { 
            if (strcmp(tweet_req->command,"FOLLOW") == 0) {
                printf("FOLLOW requested");
                format = "INSERT INTO follow_tb VALUES((SELECT user_id FROM session WHERE ssid= %d),(SELECT user_id FROM users WHERE user_name='%s'))";
                query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                bzero(query, sizeof(query));
                sprintf(query,format,tweet_req->handler,tweet_req->data);
                //result = process_query(args->conn, tweet_req->command, query);
            }
            else if (strcmp(tweet_req->command,"UNFOLLOW") == 0) {
                 printf("UNFOLLOW requested");
                 format = "DELETE FROM follow_tb WHERE follower= (SELECT user_id FROM session WHERE ssid=%d) AND following=(SELECT user_id FROM users WHERE user_name=('%s'))";
                 query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                 bzero(query, sizeof(query));
                 sprintf(query,format,tweet_req->handler,tweet_req->data);
                 //result = process_query(args->conn, tweet_req->command, query);
            }

            else if (strcmp(tweet_req->command,"FOLLOWERS") == 0) {
                 printf("FOLLOWERS requested");
                 format = "SELECT user_name FROM users LEFT JOIN follow_tb ON users.user_id=follow_tb.follower WHERE follow_tb.following=(SELECT user_id FROM session WHERE ssid=(%d))";
                 query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                 bzero(query, sizeof(query));
                 sprintf(query,format,tweet_req->handler);
                 //result = process_query(args->conn, tweet_req->command, query);
            }
            else if (strcmp(tweet_req->command,"FOLLOWING") == 0) {
                 printf("FOLLOWING requested");
                 format = "SELECT user_name FROM users LEFT JOIN follow_tb ON users.user_id=follow_tb.following WHERE follow_tb.follower=(SELECT user_id FROM session WHERE ssid=(%d))";
                 query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                 bzero(query, sizeof(query));
                 sprintf(query,format,tweet_req->handler);
                 //result = process_query(args->conn, tweet_req->command, query);
            }
            else if (strcmp(tweet_req->command,"TWEET") == 0) {
                 printf("TWEET requested");
                 format = "INSERT INTO tweets(tweet_text,user_id) VALUES ('%s',(SELECT user_id FROM session WHERE ssid=%d))";
                 query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                 bzero(query, sizeof(query));
                sprintf(query,format,tweet_req->data,tweet_req->handler);
                //result = process_query(args->conn, tweet_req->command, query);
            }
            else if (strcmp(tweet_req->command,"UNTWEET") == 0) {
                printf("UNTWEET requested");
                format = "DELETE FROM tweets WHERE tweet_id=%d AND user_id=(SELECT user_id FROM session WHERE ssid=%d)";
                query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                bzero(query, sizeof(query));
                sprintf(query,format,atoi(tweet_req->data),tweet_req->handler);
                //result = process_query(args->conn, tweet_req->command, query);
            }
            else if (strcmp(tweet_req->command,"ALLTWEETS") == 0) {
                printf("ALLTWEETS requested");
                format = "SELECT tweet_text,tweet_id FROM tweets LEFT JOIN follow_tb ON tweets.user_id = follow_tb.following \
WHERE follow_tb.follower= (SELECT user_id FROM session WHERE ssid=%d) \
UNION SELECT tweet_text,tweet_id FROM tweets WHERE user_id=(SELECT user_id FROM session WHERE ssid=%d)";
                query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                bzero(query, sizeof(query));
                sprintf(query,format,tweet_req->handler,tweet_req->handler);
                //result = process_query(args->conn, tweet_req->command, query);
            }
        else if (strcmp(tweet_req->command,"MYTWEETS") == 0) {
                printf("MYTWEETS requested");
                format = "SELECT tweet_text,tweet_id FROM tweets WHERE user_id IN (SELECT user_id FROM session WHERE ssid=%d) ORDER BY time_created DESC";
                query = (char *)malloc(strlen(format)+sizeof(tweet_req->handler)+strlen(tweet_req->data));
                bzero(query, sizeof(query));
                sprintf(query,format,tweet_req->handler);
                //result = process_query(args->conn, tweet_req->command, query);
            }

            pthread_mutex_lock(&args->th_mutex);
            printf("Query:%s\n",query);
            result = process_query(args->conn, tweet_req->command, query);
            pthread_mutex_unlock(&args->th_mutex);
            free(query);
        }
    }
    
    format = "%s %d %ld\r\n\r\n%s";
    reply = (char *)malloc(strlen(format)+strlen(result->command)+sizeof(result->st_code)+sizeof(result->dlength)+strlen(result->data));
    sprintf(reply,format,result->command,result->st_code, result->dlength,result->data);
    printf("\nData Sent:%s\n",reply);

    pthread_mutex_lock(&args->th_mutex);
    send_buf(args->sock, reply);
    pthread_mutex_unlock(&args->th_mutex);

    memset(request,NULL,strlen(request));
    close(args->sock);
    free(tweet_req);
    free(request);
    free(reply);
    free(result);
    free(args);
    pthread_exit(0);
}


PGconn *connectdb()
{
    const char *conninfo = "dbname=testdb user=jeevankishore password=hijkhijk";
    PGconn     *conn;

    // Make a conn to the database
    conn = PQconnectdb(conninfo);

    // Check to see that the backend conn was successfully made
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "conn to database failed: %s", PQerrorMessage(conn));
        PQfinish(conn);
        exit(0);
    }

    return conn;
}

int tcp_sock(int port)
{
    int sock;
    struct sockaddr_in  serv;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock <0){
        printf("Error: Failed to create a socket.\n");
        exit(0);
    }

    serv.sin_family = PF_INET;
    serv.sin_port = htons(port);
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        printf("Error: Failed to bind the socket.\n");
        exit(0);
    }

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {     //Enable re-usage of same socket during server restart
        printf("Error: Failed to set sock option SO_REUSEADDR.\n");
        exit(0);
    }

    if (listen(sock,5) <0) {
        printf("Error: Failed to set socket in passive mode.\n");
        exit(0);
    }

    return sock;
}

tweet_resp_dt *login(PGconn *conn,char *usr, char *pwd)
{
    char    *sqlcmd, *ssid = "NULL";
    const char *format, *strg;
    PGresult    *res;
    tweet_resp_dt    *result;

    result = (tweet_resp_dt *)malloc(sizeof(tweet_resp_dt));

    format = "SELECT user_name FROM users WHERE user_name = '%s' AND password = '%s'";
    sqlcmd = (char *)malloc(strlen(format)+strlen(usr)+strlen(pwd)-4);
    bzero(sqlcmd, sizeof(sqlcmd));
    sprintf(sqlcmd,format,usr,pwd);

    res = PQexec(conn,(const char *)sqlcmd);
    free(sqlcmd);
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        if (PQntuples(res) == 1) {

            PQclear(res);
            format = "INSERT INTO session(user_id) SELECT (SELECT user_id FROM users WHERE user_name = '%s') WHERE \
NOT EXISTS ( SELECT 1 FROM session WHERE user_id = (SELECT user_id FROM users WHERE user_name = '%s')); \
SELECT ssid FROM session WHERE user_id = (SELECT user_id FROM users WHERE user_name = '%s')";
            sqlcmd = (char *)malloc(strlen(format)+strlen(usr)+strlen(usr)+strlen(usr));
            bzero(sqlcmd, sizeof(sqlcmd));
            sprintf(sqlcmd,format,usr,usr,usr);

            res = PQexec(conn,(const char *)sqlcmd);
            free(sqlcmd);
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                fprintf(stderr, "Creating active session failed: %s\n", PQerrorMessage(conn));
                result->st_code = 404;
            }
            else {
                result->st_code = 200;
                ssid = PQgetvalue(res, 0, 0);
            }
        }
        else
            result->st_code = 100;
    }
    else {
        fprintf(stderr, "Database search failed: %s\n", PQerrorMessage(conn));
        result->st_code = 404;
    }

    strcpy(result->command,"LOGIN");
    result->dlength=strlen(ssid);
    switch(result->st_code) {
        case 200:   strg = ssid; break;
        case 404:   strg = "LOGIN Failure. Try again."; break;
        case 100:   strg = "User does not exist."; break;
        default:    strg = "NULL"; break;
    }
    result->data=(char*)malloc(strlen(format));
    bzero(result->data, sizeof(result->data));
    sprintf(result->data,"%s",strg);
    result->dlength=strlen(result->data);
    PQclear(res);
    return result;
}

tweet_resp_dt *signup(PGconn *conn,char *usr, char *pwd)
{
    char *sqlcmd;
    const char *format, *strg;
    PGresult   *res;
    tweet_resp_dt    *result;
    
    result = (tweet_resp_dt *)malloc(sizeof(tweet_resp_dt));

    format = "SELECT user_name FROM users WHERE user_name = '%s' AND password = '%s'";
    sqlcmd = (char *)malloc(strlen(format)+strlen(usr)+strlen(pwd)-4);
    bzero(sqlcmd, sizeof(sqlcmd));
    sprintf(sqlcmd,format,usr,pwd);

    res = PQexec(conn,(const char *)sqlcmd);
    free(sqlcmd);
    if (PQresultStatus(res) == PGRES_TUPLES_OK)
    {
        if (PQntuples(res) == 0) {

            PQclear(res);
            format = "INSERT INTO users (user_name,password) VALUES ('%s','%s')";
            sqlcmd = (char *)malloc(strlen(format)+strlen(usr)+strlen(pwd)-4);
            bzero(sqlcmd, sizeof(sqlcmd));
            sprintf(sqlcmd,format,usr,pwd);

            res = PQexec(conn,(const char *)sqlcmd);
            free(sqlcmd);
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                fprintf(stderr, "Insert command failed: %s\n", PQerrorMessage(conn));
                result->st_code = 404;
            }
            else
                result->st_code = 200;
        }
        else 
            result->st_code = 100;
    }
    else {
        fprintf(stderr, "Database search failed: %s\n", PQerrorMessage(conn));
        result->st_code = 404;
    }

    strcpy(result->command,"SIGNUP");
    switch(result->st_code) {
        case 200:   strg = "SIGNUP Success. Please LOGIN to being session."; break;
        case 404:   strg = "SIGNUP Failure."; break;
        case 100:   strg = "User already exists."; break;
        default:    strg = "NULL"; break;
    }
    result->data=(char*)malloc(strlen(format));
    bzero(result->data, sizeof(result->data));
    sprintf(result->data,"%s",strg);
    result->dlength=strlen(result->data);
    PQclear(res);
    return result;
}

tweet_resp_dt *logout(PGconn *conn,int handler)
{
    char *sqlcmd;
    const char *format, *strg;
    PGresult   *res;
    tweet_resp_dt   *result;

    result = (tweet_resp_dt *)malloc(sizeof(tweet_resp_dt));

    format = "DELETE FROM session WHERE ssid = %d";
    sqlcmd = (char *)malloc(strlen(format)+sizeof(handler)-3);
    bzero(sqlcmd, sizeof(sqlcmd));
    sprintf(sqlcmd,format,handler);

    res = PQexec(conn,(const char *)sqlcmd);
    free(sqlcmd);
    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        if (atoi(PQcmdTuples(res)) != 1) {
        fprintf(stderr, "Delete command failed: %s\n", PQerrorMessage(conn));
        result->st_code = 404;
        }
        else
            result->st_code = 200;
    }
    else result->st_code = 404;
    strcpy(result->command,"LOGOUT");
    switch(result->st_code) {
        case 200:   strg = "LOGOUT Success."; break;
        case 404:   strg = "LOGOUT Failure."; break;
        default:    strg = "NULL"; break;
    }
    result->data=(char*)malloc(strlen(format));
    bzero(result->data, sizeof(result->data));
    sprintf(result->data,"%s",strg);
    result->dlength=strlen(result->data);
    PQclear(res);
    return result;
}

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
{   int size_buf=MAXBUFSIZE;
int sbits;
    char *buffer = (char *)malloc(MAXBUFSIZE);
    memset(buffer, NULL, MAXBUFSIZE);
    while ((sbits=recv(sock, buffer, size_buf-1, 0)) > 0) {
        printf("Write buffer: %s\n", buffer);
        if (strchr(buffer, '\0') != NULL )
            break;
        size_buf-=sbits;
    }
    return buffer;
}
  
tweet_resp_dt *process_query( PGconn * conn, char *command, const char * query_text)
{
/*This is process Quesry this takes a general Query input and gives you a output in the twitterResponse Format*/

  PGresult  *res;
  tweet_resp_dt *result = (tweet_resp_dt *)malloc(sizeof(tweet_resp_dt));
  ExecStatusType resultStatus;

  PQprintOpt        options = {0};

  FILE    *infile;
  long int  numbytes;
  char   *buffer;

  strcpy(result->command,command);

  if(( res = PQexec( conn, query_text )) == NULL )
  {
    printf( "%s\n", PQerrorMessage( conn ));
    return "";
   }

  /*Mapping Result status to the required status*/
  resultStatus=PQresultStatus(res);

  switch(resultStatus)
  {
    case PGRES_COMMAND_OK:
    /*Handling Insert and Delete Statements*/
        printf("\nThis is the Command Status :  %s\n",PQcmdStatus(res));
        printf("\nThis is the No of Rows Status :  %s\n",PQcmdTuples(res));
        switch(atoi(PQcmdTuples(res) ))
        {
            case 1://Exactly one row is affected
                result->st_code=200;
                result->dlength=0;
                result->data="Request Successful!!!";
                break;
            case 0://None of the rows are affected for Insert and Delete
                result->st_code=210;
                result->dlength=0;
                result->data="Check the values entered";
                break;
            default:
                result->st_code=220;
                result->dlength=0;
                result->data="Operation error";
                break;
        }
        break;
    case PGRES_TUPLES_OK:
        result->st_code=200;

        options.header    = 1;    /* Ask for column headers*/            
        options.align     = 1;    /* Pad short columns for alignment*/
        options.fieldSep  = "|";  /* Use a pipe as the field separator*/ 

        int txtno = rand();
        char *format = "test%d.txt";
        char *txtname = (char *)malloc(strlen(format)+sizeof(txtno));
        sprintf(txtname,format,txtno); 
        infile=fopen(txtname, "w+");
        free(txtname);

        PQprint( infile, res, &options );

        /* Get the number of bytes */
        fseek(infile, 0L, SEEK_END);
        numbytes = ftell(infile);

        /* reset the file position indicator to
        the beginning of the file */
        fseek(infile, 0L, SEEK_SET);

        /* grab sufficient memory for the
        buffer to hold the text */
        buffer = (char*)calloc(numbytes, sizeof(char));
        memset(buffer,0,numbytes*sizeof(char));
        /* memory error */


        /* copy all the text into the buffer */
        fread(buffer, sizeof(char), numbytes, infile);
        //fread(buffer, 1, numbytes, infile);
        fclose(infile);
        /* confirm we have read the file by 
        outputing it to the console*/
        //printf("This file contains this text\n\n%s", buffer);

        result->dlength=numbytes;
        result->data=buffer;
        break;
    default:
        result->st_code=400;
        result->dlength=0;
        result->data="Request Not Available";
        break;
 }

 PQclear(res);
 return result;
}

void loadbalancer()
{
    int sock;
    struct sockaddr_in lb;
    struct hostent *lb_addr;
    time_t current;

    int server_id = 2;
    
    char *format = "SERVER %d\0";
    char *send_buff = (char *)malloc(strlen(format)+sizeof(server_id));
    sprintf(send_buff,format,server_id);


    lb_addr = gethostbyname("localhost");

    bzero((char *)&lb,sizeof(lb));
    lb.sin_family = PF_INET;
    lb.sin_port = htons(8001);
    bcopy((char *)lb_addr->h_addr, (char *)&lb.sin_addr.s_addr, sizeof(lb_addr->h_length));

    while(1) {

        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock <0) {
            printf("Error: Failed to create a socket.\n");
            exit(0);
        }

        printf("Trying load balancer...\n");
        if (connect(sock, (struct sockaddr *)&lb, sizeof(lb)) < 0) {
            printf("Error: Failed to connect to the remote server.\n");
            exit(0);
        }

        printf("Connected to load balancer@%s[:%d]\n", inet_ntoa(lb.sin_addr), ntohs(lb.sin_port));

        send_buf(sock, send_buff);
        close(sock);
        time(&current);
        printf("Server Keep Alive message sent.\n");

        time_t ltime; /* calendar time */
        ltime=time(NULL); /* get current cal time */
        printf("%s",asctime(localtime(&ltime)));
        sleep(60);

    }

    pthread_exit(0);
}
