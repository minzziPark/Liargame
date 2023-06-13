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

#define BUF_SIZE 1024
#define MQTT_HOST "127.0.0.1"
#define MQTT_PORT 1883

char MQTT_TOPIC[BUF_SIZE] = "Player";
char MQTT_TOPIC_ME[BUF_SIZE] = "ME";
int t_flag = 0;
int user_cnt=0;
int who = 0;

void error_handling(char * msg);
void send_read_message(int i, int sock);
void mosquitto_start();
static void on_connect(struct mosquitto *mosq, void *obj, int reason_code);
static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg);

int main(int argc, char *argv[]){
	int sock, fd_max, i;
	struct sockaddr_in serv_adr;
	fd_set master, reads;
	char user_name[BUF_SIZE];

	if(argc!=4){
		printf("Usage : %s <IP> <port> <user name>\n", argv[0]);
		exit(1);
	}
	strcpy(user_name, argv[3]);

	if((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		error_handling("socket error");

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_port = htons(atoi(argv[2]));
	serv_adr.sin_addr.s_addr = inet_addr(argv[1]);

	if(connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("connect in main error");

	FD_ZERO(&master);
	FD_ZERO(&reads);
	FD_SET(0, &master);
	FD_SET(sock, &master);
	fd_max = sock;

	send(sock, user_name, strlen(user_name), 0);

	while(1){
		reads = master;
		if(select(fd_max+1, &reads, 0, 0, 0) == -1)
			error_handling("select error");
		
		for(i=0; i<=fd_max; i++){
			if(FD_ISSET(i, &reads))
				send_read_message(i, sock);
		}
	}
	close(sock);

	mosquitto_lib_cleanup();
	return 0;
}


void send_read_message(int i, int sock){
	char send_buf[BUF_SIZE];
	char read_buf[BUF_SIZE];
	int read_cnt;

	if(i == 0){
		fgets(send_buf, BUF_SIZE, stdin);
		if(strcmp(send_buf, "q\n") == 0){
			send(sock, send_buf, strlen(send_buf), 0);
			exit(0);
		}else{
			send(sock, send_buf, strlen(send_buf), 0);
		}
	}else{
		fflush(stdout);
		if((read_cnt = recv(sock, read_buf, BUF_SIZE, 0)) == -1){
			error_handling("recv error");
		}
		read_buf[read_cnt] = 0;
		printf("%s\n", read_buf);
		if(t_flag==0){
			//본인이 어떤 Player 인지 출력해준다.
			printf("\n------------------------------------\n"
			"You are Player %c\n"
			"------------------------------------\n\n", read_buf[0]);
			char t_tmp[2] = {read_buf[0], '\0'};

			//만약 Player 1이면 start을 눌러 게임을 시작하게 한다.
			if(read_buf[0] == '1'){
				printf("------------------------------------\n"
				"Player 1 is the captain!\n" 
				"[Enter \"start\" to begin the game.]\n"
				"-------------------------------------\n\n");
			}
			//본인의 Topic을 지정해준다. Player + #(t_tmp)
			strcat(MQTT_TOPIC, t_tmp);
			strcat(MQTT_TOPIC_ME, t_tmp);

			//딱 한번만 출력되게 하기 위한 flage
			t_flag = 1;
		}
		fflush(stdout);

		//Client가 "Game Starts!"이라는 문구를 받으면 MQTT을 실행한다.
		
		if(strstr(read_buf, "Game starts!") != NULL){
			user_cnt = read_buf[0] - '0';
			mosquitto_start();
		}
	}
}

void mosquitto_start(){
	mosquitto_lib_init();

    struct mosquitto *mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        printf("Failed to create Mosquitto client instance \n");
        exit(-1);
    }

    int rc;
    char msg[BUF_SIZE];
	
	/*Will Message 보내기*/
	//모두에게 보내도록 한다.
	const char *w_topic = "will";
	char payload[BUF_SIZE];
	
	strcpy(payload, MQTT_TOPIC);
	strcat(payload, " has left the game.\n");

	mosquitto_will_set(mosq, w_topic, sizeof(payload), payload, 2, 1);


    /* Configure callbacks. This should be done before connecting ideally. */
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_message_callback_set(mosq, on_message);

	//pub code from here!
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
	
	//keep get message and publish to other clients
    while(true){
		fgets(msg, BUF_SIZE, stdin);
        rc = mosquitto_publish(mosq, NULL, MQTT_TOPIC, sizeof(msg), msg, 1, false);
		if(rc != MOSQ_ERR_SUCCESS){
			fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
		}
    }

	mosquitto_lib_cleanup();
}

static void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
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

	char *const topics[12]= {  "will", "all", "citizen", "Player1", "Player2", "Player3", "Player4", "Player5", "Player6", "Player7", "Player8", MQTT_TOPIC_ME};
	
	rc = mosquitto_subscribe_multiple(mosq, NULL, 12, (char *const *const) topics, 1, 0, NULL);

	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
}


static void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    if(msg->payloadlen>0){
		//chatting방에 Player가 누군지 외에는 Topic이 뜨지 않게 한다.
		if(strcmp(msg->topic, "all") == 0 || strcmp(msg->topic, "liar") == 0 || strcmp(msg->topic, "citizen") == 0 || strcmp(msg->topic, "will") == 0 ||strcmp(msg->topic, MQTT_TOPIC_ME) == 0){
			printf("%s\n", (char *)msg->payload);
		}
		else{
        	printf("%s %s\n", msg->topic, (char *)msg->payload);
		}
    }
}

void error_handling(char * msg){
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
