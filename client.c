#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAXBUFSIZE 4096

typedef struct rqst {
    char command[15];
    int handler;
    long int datalength;
    char *data;
} tweet_req_dt;

typedef struct rslt {
    char *command;
    int st_code;
    long int datalength;
    char *data;
} tweet_resp_dt;

int tcp_csock(int port_no,char *host_name);
char * req2buffer(tweet_req_dt *twitterRequest);
void send_buffer(int sock, char *buffer);
tweet_resp_dt *recv_buffer(int sock);
//tweet_req_dt *tweet_userinput();
void keyinput(int bufsize, char **input);
void authentication(tweet_req_dt **output);
void tweet_userinput(tweet_req_dt **output);


int start,port;//This is the Global Port number
int user_handler=0;//set to 0 when logout
char hostname[50];//this is the Global host name

int main (int argc, char **argv)
{

	int 	sock;
	char 	*send_buf;

	if (argc != 3) {
		printf("Error usage: <filename> <portno>\n");
		exit(0);
	}

	port = atoi(argv[2]);

/*load BALANCER PART*/
	tweet_req_dt *tweet_request_LB = (tweet_req_dt *)malloc(sizeof(tweet_req_dt));
	tweet_resp_dt *tweet_reply_LB = NULL;
	sock = tcp_csock(port,argv[1]);//connect to the Load Blancer.

	strcat(tweet_request_LB->command,"CLIENT");
	tweet_request_LB->handler=0;
	tweet_request_LB->datalength=0;
	tweet_request_LB->data="";

	/*write the Load balancer request to a buffer*/
	send_buf=req2buffer(tweet_request_LB);

	/*send data to remote host*/
		send_buffer(sock,send_buf);

	/*receive reply and generate the twitter reply*/

	tweet_reply_LB = recv_buffer(sock);
	close(sock);
	/*The reply we get will be interpreted and parsed*/
	char *tmp1, *ip_addr_LB_SERVER, *port_LB_SERVER;
	if ((strcmp(tweet_reply_LB->command,"CLIENT") == 0)) {
		printf("Status Code: %d\n%s\n",tweet_reply_LB->st_code, tweet_reply_LB->data);


		tmp1 = strtok(tweet_reply_LB->data," ");
		ip_addr_LB_SERVER = tmp1;
		tmp1 = strtok(NULL,"\0");
		port_LB_SERVER = tmp1;
		printf("Received IP Address: %s & Port No: %s\n",ip_addr_LB_SERVER,port_LB_SERVER);
	}
	else{
		printf("The command recived from LB is not in the proper format\n");
		exit(0);
	}

	port = atoi(port_LB_SERVER);
	strcat(hostname,ip_addr_LB_SERVER);

	while (1) {

		tweet_req_dt *tweet_request = (tweet_req_dt *)malloc(sizeof(tweet_req_dt));
		tweet_resp_dt *tweet_reply = NULL;

		/*connect to remote host*/
		sock = tcp_csock(port,hostname);

		if(user_handler==0){
			authentication(&tweet_request);
		}//if user handler == 0
		else{
			tweet_userinput(&tweet_request);
		}//if user handler != 0



		/*write the twitter request to a buffer*/
		send_buf=req2buffer(tweet_request);

		/*send data to remote host*/
		send_buffer(sock,send_buf);
		/*receive reply and generate the twitter reply*/
		tweet_reply = recv_buffer(sock);

		close(sock);
	//decision should be made on the Handler rather  than the LOGIN reply you get from the Server.

		if ((strcmp(tweet_reply->command,"LOGIN") == 0) && (tweet_reply->st_code == 200)) {
			printf("Status Code: %d\n%s\n",tweet_reply->st_code, tweet_reply->data);
			user_handler = atoi(tweet_reply->data);
		}
		else {
			printf("Status Code: %d\n%s\n",tweet_reply->st_code, tweet_reply->data);
		}

		free(send_buf);
		free(tweet_request);
		free(tweet_reply);
	}
	return 0;
}

void authentication(tweet_req_dt **output)
{
	int choice;
	char *username, *password, *format;
	tweet_req_dt *tweet_request = (tweet_req_dt *)malloc(sizeof(tweet_req_dt));

	start = 1;
	while(1) {
		printf("\nSIGNUP[1] or LOGIN[2] or EXIT[9]\nEnter request type:");
		scanf("%d",&choice);
		//choice=2;
		if( choice == 9) {
			printf("You have chosen to exit this service.\n");
			exit(0);
		}
		else if ( (choice == 1) || (choice == 2)) {
			break;
		}
		else {
			printf("Error: Invalid input. Cannot be NULL.\nEnter again:");
			getchar();
			continue;
		}
	}
	getchar();

	username = (char *)malloc(20);
	password = (char *)malloc(20);
	printf("Username:");
	keyinput(20,&username);

	printf("Password:");
	keyinput(20, &password);

	//printf("Choice: %d | Username: %s | Password: %s\n",choice, username,password);


	format = "%s %s";
	tweet_request->data = (char *)malloc(strlen(format)+strlen(username)+strlen(password));
	sprintf(tweet_request->data,format,username,password);
	if (choice == 1) strcpy(tweet_request->command,"SIGNUP");
	else strcpy(tweet_request->command,"LOGIN");
	tweet_request->handler = user_handler;
	tweet_request->datalength = strlen(tweet_request->data);

	*output = tweet_request;
}

void tweet_userinput(tweet_req_dt **output)
{
	char *request=(char *)malloc(200);

	tweet_req_dt *tweet_request = (tweet_req_dt *)malloc(sizeof(tweet_req_dt));
	tweet_request->handler = user_handler;
	memset(request, 0, strlen(request));

	printf("Choose your service:");
	if (start == 1) {
	printf("\nFOLLOW <username>\n\
UNFOLLOW <username>\n\
FOLLOWERS\n\
FOLLOWING\n\
TWEET <tweet_text>\n\
UNTWEET <tweet_id>\n\
ALLTWEETS\n\
MYTWEETS\n\
LOGOUT\n\
or HELP to check available commands\n\
or type EXIT to exit the program\n");
	start = 0;
	}

	while (1){
		printf(">>");
		keyinput(200,&request);
		if (strcmp(request,"\0")==0) {
		printf("Please Enter valid Characters\n");
		printf("Choose your service:\n\
FOLLOW <username>\n\
UNFOLLOW <username>\n\
FOLLOWERS\n\
FOLLOWING\n\
TWEET <tweet_text>\n\
UNTWEET <tweet_id>\n\
ALLTWEETS\n\
MYTWEETS\n\
LOGOUT\n\
or HELP to check available commands\n\
or type EXIT to exit the program\n");continue;
		}
			printf("Request:%s\n",request);

			char *tmp;
			if ((strcmp(request,"FOLLOWERS") ==0)\
|| (strcmp(request,"FOLLOWING")==0) \
|| (strcmp(request,"ALLTWEETS")==0)\
|| (strcmp(request,"MYTWEETS")==0) \
|| (strcmp(request,"LOGOUT"))==0) {

				if(((strcmp(request,"LOGOUT"))==0)){
					user_handler=0;
				}
				strcpy(tweet_request->command,request);
				tweet_request->data = "";
				tweet_request->datalength = strlen(tweet_request->data);
				free(request);
				break;
			}
			else if((strcmp(request,"EXIT") ==0)){
					printf("Please LOGOUT before exit");
					continue;
			}
			else if((strcmp(request,"HELP") ==0)){
					printf("\nAVAILABLE COMMANDS:\nFOLLOW <username>\n\
UNFOLLOW <username>\n\
FOLLOWERS\n\
FOLLOWING\n\
TWEET <tweet_text>\n\
UNTWEET <tweet_id>\n\
ALLTWEETS\n\
MYTWEETS\n\
LOGOUT\n\
or HELP to check available commands\n\
or type EXIT to exit the program\n");
					continue;
			}
			else {
				tmp = strtok(request," ");
				if ((strcmp(tmp,"FOLLOW")==0) \
|| (strcmp(tmp,"UNFOLLOW")==0) \
|| (strcmp(tmp,"TWEET")==0) \
|| (strcmp(tmp,"UNTWEET")==0)) {
					strcpy(tweet_request->command,tmp);
					tmp = strtok(NULL,"\0");
					tweet_request->data=(char*)malloc(strlen(tmp)+1);
					strcpy(tweet_request->data,tmp);
					tweet_request->datalength = strlen(tweet_request->data);
					break;
				}
				else {
					printf("Please Enter valid Characters\n");
					continue;
				}
			}
	}
	*output = tweet_request;
}


char * req2buffer(tweet_req_dt *twitterRequest)
{/*This Function is used on the client side to put the structure twitterRequest into a buffer ..
 	 this function returns the buffer size when executed*/
	char *format = "%s %d %ld\r\n\r\n%s";
	int bufLength = (strlen(format)+strlen(twitterRequest->command)+sizeof(twitterRequest->handler)+sizeof(twitterRequest->datalength)+strlen(twitterRequest->data)+1);
	char *tempBuf=(char *)malloc(bufLength);
	sprintf(tempBuf,format,twitterRequest->command,twitterRequest->handler,\
		twitterRequest->datalength,twitterRequest->data);
	return tempBuf;
}

void send_buffer(int sock, char *buffer)
{
		int	sdata, sbits = 0;
		//printf("SENDing Buffer:%s",buffer);
	   	while(sbits < strlen(buffer))
	   	{	
		  	if ((sdata = send(sock, buffer, strlen(buffer)-sbits, 0)) <0){
				printf("Error: Failed to send data to remote server.\n");
				exit(0);
		  	}
		  	else sbits += sdata;
	   	}
}

tweet_resp_dt *recv_buffer(int sock)
{
	char *tempBuf=(char *)malloc(MAXBUFSIZE);
    char *buffer = (char *)malloc(MAXBUFSIZE);

    memset(buffer,NULL, strlen(buffer));
    while (recv(sock, buffer, strlen(buffer)-1, 0) > 0) {
        //printf("Write buffer: %s\n", buffer);
        if (strchr(buffer, '\0') != NULL )
            break;
    }

    strcpy(tempBuf,buffer);
    tweet_resp_dt *tweet_reply;
	tweet_reply = (tweet_resp_dt *)malloc(sizeof(tweet_resp_dt));

    char *tmp;
    tmp = strtok(tempBuf," ");
    tweet_reply->command = (char *)malloc(strlen(tmp));
    strcpy(tweet_reply->command,tmp);
    tmp = strtok(NULL," ");
    tweet_reply->st_code = atoi(tmp);
    tmp = strtok(NULL,"\r\n\r\n");
    tweet_reply->datalength = atoi(tmp);
    tmp = strtok(NULL,"\0");
    tmp = tmp+3*sizeof(char);
    tweet_reply->data = (char *)malloc(strlen(tmp));
    strcpy(tweet_reply->data,tmp);

    memset(buffer,NULL, strlen(buffer));
    free(tempBuf);
    free(buffer);
   	return tweet_reply;
}


/*create socket and Connect to the socket*/
int tcp_csock(int port_no,char *host_name)
{

	struct sockaddr_in serv;
	struct hostent *s_addr;
	int sock;


	s_addr = gethostbyname(host_name);

	bzero((char *)&serv,sizeof(serv));
	serv.sin_family = PF_INET;
	serv.sin_port = htons(port_no);
	bcopy((char *)s_addr->h_addr, (char *)&serv.sin_addr.s_addr, sizeof(s_addr->h_length));


	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock <0) {
		printf("Error: Failed to create a socket.\n");
		exit(0);
	}

	printf("Trying remote server...\n");
	if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
		printf("Error: Failed to connect to the remote server.\n");
		exit(0);
	}

	printf("Connected to server@%s[:%d]\n", inet_ntoa(serv.sin_addr), ntohs(serv.sin_port));
	return sock;
}


void keyinput(int bufsize, char **input)
{
	char buffer[bufsize]; char *ptr;

	while(1) {
		//printf("Enter a text>");
		fgets(buffer,sizeof(buffer)+1,stdin);

		if ((ptr = strchr(buffer,'\n')) != NULL) {
			*ptr = '\0';
		}
		else {
			char temp[10];
			while ((strchr(temp,'\n') == NULL)) {
				fgets(temp,sizeof(temp),stdin);
			}
			//free(buffer);
			//printf("Temp:%s\n",temp);
			memset(temp,0,sizeof(temp));
			printf("Input Overflow. Try again.\n");
			continue;
		}

		/*if (strcmp(buffer,"\0") == 0) { printf("Error: No text entered. Try again.\n"); continue; }
		else { printf("Data Received in buffer: >%s<\n\n",buffer);*/ break;
	}
	strcpy(*input,buffer);
}
