// MYSQL source code from "https://zetcode.com/db/mysqlc/"
/*
 * This example shows how to write a client that subscribes to a topic and does
 * not do anything other than handle the messages that are received.
 */

#include <mosquitto.h>
#include "/usr/include/mysql/mysql.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MQTT_HOST "127.0.0.1"
#define MQTT_PORT 1883
MYSQL *con;

void finish_with_error(MYSQL *con)
{
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	exit(1);
}

/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	/* Print out the connection result. mosquitto_connack_string() produces an
	 * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
	 * clients is mosquitto_reason_string().
	 */
	printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		/* If the connection fails for any reason, we don't want to keep on
		 * retrying in this example, so disconnect. Without this, the client
		 * will attempt to reconnect. */
		mosquitto_disconnect(mosq);
	}

	/* Making subscriptions in the on_connect() callback means that if the
	 * connection drops and is automatically resumed by the client, then the
	 * subscriptions will be recreated when the client reconnects. */
	rc = mosquitto_subscribe(mosq, NULL, "log", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
}


/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	bool have_subscription = false;

	/* In this example we only subscribe to a single topic at once, but a
	 * SUBSCRIBE can contain many topics at once, so this is one way to check
	 * them all. */
	for(i=0; i<qos_count; i++){
		printf("on_subscribe: %d:granted qos = %d\n", i, granted_qos[i]);
		if(granted_qos[i] <= 2){
			have_subscription = true;
		}
	}
	if(have_subscription == false){
		/* The broker rejected all of our subscriptions, we know we only sent
		 * the one SUBSCRIBE, so there is no point remaining connected. */
		fprintf(stderr, "Error: All subscriptions rejected.\n");
		mosquitto_disconnect(mosq);
	}
}


/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	/* This blindly prints the payload, but the payload can be anything so take care. */
	char log_msg[1024];
	printf("%s\n", (char *)msg->payload);
	sprintf(log_msg, "%s%s%s", "INSERT INTO statuses(status) VALUES('", (char *)msg->payload, "')");
	if(mysql_query(con, log_msg))
	{
		finish_with_error(con);
	}
}


int main(int argc, char *argv[])
{
	struct mosquitto *mosq;
	int rc;
	con = mysql_init(NULL);

	if(con == NULL)
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		exit(1);
	}

	// need to set your own mysql password.
	if(mysql_real_connect(con, "localhost", "root", "alswl0820", NULL, 0, NULL, 0) == NULL)
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		mysql_close(con);
		exit(1);
	}

	if(mysql_query(con, "USE liardb"))
	{
		if(strstr(mysql_error(con), "Unknown database")!=NULL)
		{
			printf("error check!!\n");
			if(mysql_query(con, "CREATE DATABASE liardb"))
			{
				fprintf(stderr, "%s\n", mysql_error(con));
				mysql_close(con);
				exit(1);
			}
		}
		mysql_query(con, "USE liardb");
	}

	//create table
	if(mysql_query(con, "SELECT * FROM statuses"))
	{
		if(strstr(mysql_error(con), "doesn't exist")!=NULL)
		{
			if(mysql_query(con, "CREATE TABLE liardb.statuses(id INT PRIMARY KEY AUTO_INCREMENT, status VARCHAR(255))"))
			{
				finish_with_error(con);
			}
		}
	}

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
		return 1;
	}

	/* Configure callbacks. This should be done before connecting ideally. */
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_subscribe_callback_set(mosq, on_subscribe);
	mosquitto_message_callback_set(mosq, on_message);

	/* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
	 * This call makes the socket connection only, it does not complete the MQTT
	 * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
	 * mosquitto_loop_forever() for processing net traffic. */
	rc = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
		return 1;
	}

	/* Run the network loop in a blocking call. The only thing we do in this
	 * example is to print incoming messages, so a blocking call here is fine.
	 *
	 * This call will continue forever, carrying automatic reconnections if
	 * necessary, until the user calls mosquitto_disconnect().
	 */
	mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_lib_cleanup();

	mysql_close(con);
	return 0;
}
