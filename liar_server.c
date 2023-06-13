#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mosquitto.h>
#include<time.h>

#define BUF_SIZE 128
#define MAX_USER 8
#define MIN_USER 1
#define MQTT_HOST 	"127.0.0.1"
#define MQTT_PORT	1883

char MQTT_TOPIC_LIAR[BUF_SIZE] = "ME";

int FLAG = 0;
int p_cnt = 0;

int liar = 0;
int s_num = 0;
int c_flag = 0;
int turn = 0;
int game =0;
int player[8]={0,0,0,0,0,0,0,0};
int out = 0;

int user_cnt = 0;
int start_msg = 0;
int game_start = 0;



typedef struct{
	int sock;
	char nickname[BUF_SIZE];
	char topic[BUF_SIZE];
}USERS;

void error_handling(char * msg);
void read_message(int i, fd_set *master, int serv_sock, int fd_max);
void send_all_clients(int j, int i, int serv_sock, int read_cnt, char *buf, fd_set *master, int isall);
void add_user(int sock, char nickname[BUF_SIZE]);
void delete_user(int sock, fd_set *master, int serv_sock, int fd_max);
void set_user();
int find_user(int sock);
void send_join_msg(int clnt_sock, int i, int serv_sock, fd_set *master, int fd_max);
void mosquitto_start();

USERS users[MAX_USER];

int main(int argc, char *argv[]){
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	fd_set master, reads;
	int fd_max, i, read_cnt, j;
	socklen_t adr_sz;
	char nickname[BUF_SIZE];
	char count_user[BUF_SIZE];

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	// setting USERS struct with sock integer to 0.
	set_user();

	// initialize the discriptor.
	FD_ZERO(&master);
	FD_ZERO(&reads);

	// connect request.
	if((serv_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		error_handling("select error");

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_port = htons(atoi(argv[1]));
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
	if(listen(serv_sock, MAX_USER) == -1)
		error_handling("listen() error");
	
	FD_SET(serv_sock, &master);
	fd_max = serv_sock;

	while(1){
		reads = master;
		if(select(fd_max+1, &reads, NULL, NULL, NULL) == -1)
			error_handling("select error");

		for(i=0; i<=fd_max; i++){
			if(FD_ISSET(i, &reads)){
				if(i == serv_sock){ //connection request
					adr_sz = sizeof(clnt_adr);
					if((clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz)) == -1)
						error_handling("accept error");

					FD_SET(clnt_sock, &master);
					if(fd_max < clnt_sock)
						fd_max = clnt_sock;

					memset(nickname, 0, BUF_SIZE);
					if((read_cnt = recv(clnt_sock, nickname, BUF_SIZE, 0)) == -1){
						error_handling("recv error");
					}
					add_user(clnt_sock, nickname);
					printf("Client nickname is %s and client number is %d\n", nickname, clnt_sock);
					if(c_flag == 0){
						s_num = clnt_sock;
						c_flag =1 ;
					}
					strcat(nickname, " joined this chatroom.\n");
					fflush(stdout);

					for(j=0; j<fd_max; j++){
						send_all_clients(j, i, serv_sock, BUF_SIZE, nickname, &master, 0);
					}
					send_join_msg(clnt_sock, i, serv_sock, &master, fd_max);
					
				}
				else //read message
					read_message(i, &master, serv_sock, fd_max); 
			}
		}
		// if(game_start == 1){
		// 	break;
		// }
	}	
	

	while(1){
		for(i=0; i<=fd_max; i++){
			read_message(i, &master, serv_sock, fd_max); 
		}
	}

	return 0;
}


void send_join_msg(int clnt_sock, int i, int serv_sock, fd_set *master, int fd_max){
	char users_name[BUF_SIZE] = "";
	char join_msg[BUF_SIZE] = "";
	int j, tf = 0;

	for(j = 0; j<MAX_USER; j++){
		if(users[j].sock != -1 && users[j].sock != clnt_sock){
			tf++;
			if(strlen(users_name) != 0){
				strcat(users_name, ", ");
			}
			strcat(users_name, users[j].nickname);
		}
	}
	user_cnt = (tf+1);

	sprintf(join_msg, "%d%s", user_cnt, " user(s) in the room.\n");
	send_all_clients(clnt_sock, i, serv_sock, BUF_SIZE, join_msg, master, 0);
	strcpy(join_msg, "");
	printf("users : %s\n", join_msg);

	// if(user_cnt > 1) {
	// 	sprintf(join_msg, "%s%s", users_name, " in the room.\n");
		
	// 	send_all_clients(clnt_sock, i, serv_sock, BUF_SIZE, join_msg, master, 0);
	// }
	
	if(user_cnt >= MIN_USER && start_msg == 0){
		sprintf(join_msg, "%s", "Now there is over 3 participants. \n(Player1 can start the game!)\n");
		// 모든 유저들에게 start할 수 있다고 알리기
		for(j=0; j<=fd_max; j++){
			send_all_clients(j, i, serv_sock, BUF_SIZE, join_msg, master, 0);
		}
		start_msg = 1;
	}
}

int find_user(int sock){
	int i;
	for(i=0; i<MAX_USER; i++){
		if(users[i].sock == sock)
			return i;
	}
}

void set_user(){
	int i;
	for(i=0; i<MAX_USER; i++){
		users[i].sock = -1;
	}
}

void add_user(int sock, char nickname[BUF_SIZE]){
	int i;
	for(i=0; i<MAX_USER; i++){
		if(users[i].sock == -1){
			users[i].sock = sock;
			strcpy(users[i].nickname, nickname);
			break;
		}
	}
}

void delete_user(int sock, fd_set *master, int serv_sock, int fd_max){
	int i, j;
	char del_msg[BUF_SIZE];
	for(i=0; i<MAX_USER; i++){
		if(users[i].sock == sock){
			strcpy(del_msg, users[i].nickname);
			strcat(del_msg, " left this chatroom.\n");
			i = users[i].sock;
			users[i].sock = -1;
			memset(users[i].nickname, 0, BUF_SIZE);
			break;
		}
	}
	for(j=0; j<=fd_max; j++)
		send_all_clients(j, i, serv_sock, BUF_SIZE, del_msg, master, 0);
}

void send_all_clients(int j, int i, int serv_sock, int read_cnt, char *buf, fd_set *master, int isall){
	if(FD_ISSET(j, master)){
		if(isall == 1){
			if(j != serv_sock){
				if(send(j, buf, read_cnt, 0) == -1)
				error_handling("send all clients error");
			}
		}
		else{
			if(j != serv_sock && j != i){
				if(send(j, buf, read_cnt, 0) == -1)
					error_handling("send all clients error");
		}
		}
		
	}
}

void read_message(int i, fd_set *master, int serv_sock, int fd_max){
	int read_cnt, j;
	char buf[BUF_SIZE];
	char user_nick[BUF_SIZE];
	char start[BUF_SIZE] = "";
	int user_idx;
	
	user_idx = find_user(i);

	strcpy(user_nick, users[user_idx].nickname);

	memset(buf, 0, BUF_SIZE);
	if((read_cnt = recv(i, buf, BUF_SIZE, 0)) <= 0){
		if(read_cnt != 0)
			error_handling("recv error");
		close(i);
		FD_CLR(i, master);
	}else{
		printf("[%s] %s", user_nick, buf);
		fflush(stdout);
		strcat(user_nick, " : ");
		strcat(user_nick, buf);

		if(strcmp(buf, "q\n") == 0){
			delete_user(i, master, serv_sock, fd_max);
		}else if(strcmp(buf, "start\n") == 0){//"start"라는 단어가 들어오면
			//만약 게임의 최소 인원인 3명이 들어오면
			if(user_cnt >= MIN_USER){
				sprintf(start, "%dPlayers %s\n",user_cnt, "Game starts!");
				game_start = 1;

				for(j=0; j<=fd_max; j++){
						send_all_clients(j, i, serv_sock, BUF_SIZE, start, master, 1);
				}
				mosquitto_start();
			}else{ //게임인원이 최소 이하이면
				sprintf(start, "%s", "The game cannot begin (Need at least 3 players)\n");
				for(j=0; j<=fd_max; j++){
					
					send_all_clients(j, i, serv_sock, BUF_SIZE, start, master, 1);
				}
			}
			
		}
		else{
			for(j=0; j<=fd_max; j++){
				send_all_clients(j, i, serv_sock, BUF_SIZE, user_nick, master, 0);
			}
			memset(user_nick, 0, BUF_SIZE);
		}
	}
}

void error_handling(char * msg){
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

void publish_sensor_data(struct mosquitto *mosq);

void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{	
	int rc;
	/* Print out the connection result. mosquitto_connack_string() produces an
	 * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
	 * clients is mosquitto_reason_string().
	 */
	printf("on_connect: %s\n\n\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		/* If the connection fails for any reason, we don't want to keep on
		 * retrying in this example, so disconnect. Without this, the client
		 * will attempt to reconnect. */
		mosquitto_disconnect(mosq);
	}
	/* Making subscriptions in the on_connect() callback means that if the
	 * connection drops and is automatically resumed by the client, then the
	 * subscriptions will be recreated when the client reconnects. */
	const char *const topics[] = { "will", "Player1", "Player2", "Player3", "Player4", "Player5", "Player6", "Player7", "Player8"};
	rc = mosquitto_subscribe_multiple(mosq, NULL, 9, (char *const *const)topics, 1, 0, NULL);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
}


/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
void on_publish(struct mosquitto *mosq, void *obj, int mid)
{	
	publish_sensor_data(mosq);
	// printf("Message with mid %d has been published.\n", mid);
}

void publish_turn(struct mosquitto *mosq){
	char payload[1024];
	int rc;
	snprintf(payload, sizeof(payload),"\n********************************************\n"
										"|  One more turn?                           |\n"
										"|  -> Enter 'continue' to go more round     |\n"
										"********************************************\n"
										"|  Or Guess the liar! When done guessing,   |\n"
										"|  -> *Player1* Enter                       |\n"
                                        "|     'The liar is (player number)'         |\n"
										"|  ex) The liar is 9                        |\n"                    
										"********************************************\n");

	rc = mosquitto_publish(mosq, NULL, "all", strlen(payload), payload, 2, true);
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}

	time_t t;   // not a primitive datatype
    time(&t);
				
	char s_time[26];
	strcpy(s_time, ctime(&t));
	s_time[strlen(s_time)-1]='\0';

	snprintf(payload, sizeof(payload),"[%s] : %d Turn Ended\n", s_time, turn);
			
	rc = mosquitto_publish(mosq, NULL, "log", strlen(payload), payload, 2, true);
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}
}
void publish_liar(struct mosquitto *mosq, int g_num){
    
	char payload[1024];
	int rc;
	printf("g_nu,:%d\n", g_num);
    if(g_num == liar){
		
        snprintf(payload, sizeof(payload), "\n********************************************\n"
                                            "|          The liar is Player %d            |\n"
                                            "|          Citizen won the game!            |\n"
                                            "********************************************\n"
											"|           GAME OVER! Ctrl+C to exit!      |\n"
											"|       OR ENTER 'Resume Game' to continue  |\n"
                                            "********************************************\n"
                                             , liar);
    } else{
        snprintf(payload, sizeof(payload),"\n********************************************\n"
                                            "|          The liar is Player %d            |\n"
                                            "|           Liar won the game!              |\n"
                                            "********************************************\n"
											"|       GAME OVER! Ctrl+C to exit!          |\n"
											"|       OR ENTER 'Resume Game' to continue  |\n"
                                            "********************************************\n"
                                             , liar);
    }
			
	rc = mosquitto_publish(mosq, NULL, "all", strlen(payload), payload, 2, true);
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}

}
void log_liar(struct mosquitto *mosq, int g_num){
    
	char payload[1024];
	int rc;
	printf("g_nu,:%d\n", g_num);
	time_t t;   // not a primitive datatype
    time(&t);
				
	char s_time[26];
	strcpy(s_time, ctime(&t));
	s_time[strlen(s_time)-1]='\0';

    if(g_num == (liar)){
        snprintf(payload, sizeof(payload), "[%s] : Guessed the right liar player %d\n" 
										   "                             "
										   "Citizen won the game!\n", s_time, liar-s_num+1);
    } else{
        snprintf(payload, sizeof(payload), "[%s] : Guessed the wrong liar player %d\n" 
		                                   "                             "
										   "Liar won the game!\n", s_time, g_num);
    }
			
	rc = mosquitto_publish(mosq, NULL, "log", strlen(payload), payload, 2, true);
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}

}
void log_continue(struct mosquitto *mosq){
    
	char payload[1024];
	int rc;
	time_t t;   // not a primitive datatype
    time(&t);
				
	char s_time[26];
	strcpy(s_time, ctime(&t));
	s_time[strlen(s_time)-1]='\0';

    
    snprintf(payload, sizeof(payload), "[%s] : The player is taking round %d\n", s_time, turn+1);
			
	rc = mosquitto_publish(mosq, NULL, "log", strlen(payload), payload, 2, true);
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}

}
void log_start(struct mosquitto *mosq){
    
	char payload[1024];
	int rc;
	time_t t;   // not a primitive datatype
    time(&t);
				
	char s_time[26];
	strcpy(s_time, ctime(&t));
	s_time[strlen(s_time)-1]='\0';
	
	if(user_cnt<MIN_USER){
		snprintf(payload, sizeof(payload), "Not enough player. End the Game(Ctrl+C)\n");
		rc = mosquitto_publish(mosq, NULL, "all", strlen(payload), payload, 2, true);
	}else{
    	snprintf(payload, sizeof(payload), "[%s] : %dPlayers Game starts!\n", s_time, user_cnt);
		rc = mosquitto_publish(mosq, NULL, "log", strlen(payload), payload, 2, true);	
		FLAG=0;
		turn=0;
		p_cnt=0;
	}
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}

}
void log_will(struct mosquitto *mosq, const char* msgpayload){
    char t_tmp[2] = {msgpayload[6], '\0'};
	int o_tmp = t_tmp[0]-'0';
	player[out] = o_tmp;
	printf("Out:%d\n", o_tmp);
	printf("Player Out:%d\n", player[out]);
	out+=1;

	char payload[1024];
	int rc;
	time_t t;   // not a primitive datatype
	time(&t);
						
	char s_time[26];
	strcpy(s_time, ctime(&t));
	s_time[strlen(s_time)-1]='\0';
	user_cnt -=1;
			
	snprintf(payload, sizeof(payload), "[%s] : %s"
											   "                             "
											   "%d players left\n", s_time, msgpayload, user_cnt);
					
	rc = mosquitto_publish(mosq, NULL, "log", strlen(payload), payload, 2, true);
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}
	snprintf(payload, sizeof(payload), "\n********************************************\n"
									   "|           GAME OVER! Ctrl+C to exit!      |\n"
									   "|       OR ENTER 'Resume Game' to continue  |\n"
                                        "********************************************\n");
					
	rc = mosquitto_publish(mosq, NULL, "all", strlen(payload), payload, 2, true);
	if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
	}

}
static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    // TODO
    if(msg->payloadlen>0){
        printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);
    }
	if(strstr(msg->payload, "The word")!=NULL){
		p_cnt += 1;
		printf("p_cnt:%d\n", p_cnt);
		if(p_cnt == user_cnt){
			turn +=1;
			publish_turn(mosq);
		}
	}
	else if(strcmp(msg->payload, "continue\n")== 0){
		p_cnt = 0;
		log_continue(mosq);
	}
	else if(strstr(msg->payload, "The liar is") != NULL){
        char g_temp[BUF_SIZE];
        strcpy(g_temp,msg->payload);
        int g_num = g_temp[12] -'0';
		publish_liar(mosq, g_num);
		log_liar(mosq, g_num);
	}
	else if(strstr(msg->payload, "Resume Game") != NULL){
		log_start(mosq);
	}
	if(strcmp(msg->topic, "will")==0){
		log_will(mosq, msg->payload);
	}
}

const char* get_topic(int num)
{   
    const char* topic[10] ={"animal", "fruit", "handong", "movie", "actor", "job", "transportation", "game", "actor", "utensil"};
	return topic[num];
}

const char* get_word(int num)
{   
    const char* word[10] ={"rabbit", "watermelon", "YunminGo", "HarryPotter", "TomCruise", "Doctor", "Taxi", "LiarGame","TomCruise", "Fork"};
	return word[num];
}

/* This function pretends to read some data from a sensor and publish it.*/
void publish_sensor_data(struct mosquitto *mosq)
{
	char topics[4][30] = {  "all", "citizen", "log", "00"};
	char payload[1024];
	const char* temp1;
    const char* temp2;
	int rc;
	int i; 
	int num = random()%10;
	sleep(1); 	/* Prevent a storm of messages - this pretend sensor works at 1Hz */

	if(FLAG == 0){
		while(1){
			int flg = 0;
			liar = rand()%user_cnt +1;
			int j=0;
			for(j =0; j<8; j++){
				if(player[j] == liar){
					flg=1;
				}
			}
			if(flg==0) break;
		}
		printf("liar: %d\n", liar);
		char liars[2] =  {liar + '0'};
		strcpy(MQTT_TOPIC_LIAR,"ME");
		strcat(MQTT_TOPIC_LIAR, liars);
		strcpy(topics[3], MQTT_TOPIC_LIAR);
		printf("Topic:%s\n", MQTT_TOPIC_LIAR);

		// Publish three topics
		for (i = 0; i < 4; i++)
		{
			if (i==0){
				temp1 = get_topic(num);
				snprintf(payload, sizeof(payload),  "\n********************************************\n"
											"*                GAME RULE                 *\n"
											"********************************************\n"
											"| 1. The person assigned as the liar must   |\n"
											"|    not reveal their own role as the liar. |\n"
											"| 2. Except for the liar, all other people  |\n"
											"|    receive a specific word.               |\n"
											"| 3. Take turns describing the given word.  |\n"
											"|    **You must type 'The word'on your turn |\n"
											"|    ex) The word is small!                 |\n"
											"| 4. After two rounds,                      |\n"
											"|    make a prediction on who the liar is.  |\n"
											"| 5. The chosen person shares information   |\n"
											"|    about their word.                      |\n"
											"|    If the liar successfully guesses       |\n"
											"|    the word, the liar wins; otherwise,    |\n"
											"|    the others win.                        |\n"
											"*********************************************\n"
											"|          Player1 Start the Game!         |\n"
											"*********************************************\n"
											"Topic : %s", temp1);
			}
			else if(i==1){
				temp2 = get_word(num);
				snprintf(payload, sizeof(payload), "Word : %s\n\n", temp2);
			}
			else if(i==2){
				time_t t;   // not a primitive datatype
				time(&t);
						
				char s_time[26];
				strcpy(s_time, ctime(&t));
				s_time[strlen(s_time)-1]='\0';
				game +=1;
				sprintf(payload, "[%s] : The game %d started! With %dPlayers\n"
								"                             "
								"The liar is player %d\n"
								"                             "
								"Topic: %s\n"
								"                             "
								"Word : %s\n", s_time, game, user_cnt, liar, temp1, temp2);
			}
			else if(i==3){
				snprintf(payload, sizeof(payload), "You are a Liar!! Good Luck\n\n");
			}
			rc = mosquitto_publish(mosq, NULL, topics[i], strlen(payload), payload, 2, true);
			if(rc != MOSQ_ERR_SUCCESS){
				fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
			}
			
		}
			
		p_cnt = 0;
		FLAG = 1;
	}
	
}

void mosquitto_start(){
    mosquitto_lib_cleanup();
    struct mosquitto *mosq;
	int rc;

	/* Required before calling other mosquitto functions */
	mosquitto_lib_init();

	/* Create a new client instance.
	 * id = NULL -> ask the broker to generate a client id for us
	 * clean session = true -> the broker should remove old sessions when we connect
	 * obj = NULL -> we aren't passing any of our private data for callbacks
	 */
	mosq = mosquitto_new(NULL, true, NULL);
	if(mosq == NULL){
		fprintf(stderr, "Error: Out of memory.\n");
		exit(-1);
	}

	/* Configure callbacks. This should be done before connecting ideally. */
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_message_callback_set(mosq, on_message);
	mosquitto_publish_callback_set(mosq, on_publish);
	

	/* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
	 * This call makes the socket connection only, it does not complete the MQTT
	 * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
	 * mosquitto_loop_forever() for processing net traffic. */
	rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		exit(-1);
	}

	/* Run the network loop in a background thread, this call returns quickly. */
	rc = mosquitto_loop_start(mosq);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		exit(-1);
	}

	/* At this point the client is connected to the network socket, but may not
	 * have completed CONNECT/CONNACK.
	 * It is fairly safe to start queuing messages at this point, but if you
	 * want to be really sure you should wait until after a successful call to
	 * the connect callback.
	 * In this case we know it is 1 second before we start publishing.
	 */
	while(true){
		publish_sensor_data(mosq);
	}

	mosquitto_lib_cleanup();
}