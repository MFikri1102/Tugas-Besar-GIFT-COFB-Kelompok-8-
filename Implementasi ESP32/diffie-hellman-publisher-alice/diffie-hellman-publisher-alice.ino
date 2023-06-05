#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Fik";
const char* password = "23456789";
const char* mqtt_server = "broker.mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Define the MQTT broker and topic

#define MQTT_TOPIC_ALICE "diffie-hellman-alice"
#define MQTT_TOPIC_BOB "diffie-hellman-bob"

// Define the public prime number and base
long int p = 62319833; // Q public prime number
long int g = 880507609; // P public base

// Define the private and public keys
long int a; // private key
long int A; // public key

long int b;

// Define the shared secret key
long int keyA;

long int sharedSecretKey;

// A function to compute a^m mod n
int compute(long int a, long int m, long int n) {
    long int r;
    long int y = 1;

    while (m > 0) {
        r = m % 2;
        if (r == 1) {
            y = (y*a) % n;
        }
        a = a*a % n;
        m = m / 2;
    }

    return y;
}

// A function to calculate the length of a number by iteration
int calculateLengthByIteration(long long n){ 
  if( n < 0 ) 
    n = -n; 
   
  int length = 0; 
  do{ 
    length = length + 1; 
    n = n / 10; 
  }while( n > 0 ); 
   
  return length; 
} 

// A callback function to handle incoming messages from the subscriber
void callback(char* topic, byte* payload, unsigned int length) {
  // Convert the payload to a long int
  long int otherPartyKey = atol((char*)payload);

  // Compute the shared secret key using the other party's public key and the private key
  if (strcmp(topic, MQTT_TOPIC_ALICE) == 0) {
    sharedSecretKey = compute(otherPartyKey, a, p);
  } else if (strcmp(topic, MQTT_TOPIC_BOB) == 0) {
    sharedSecretKey = compute(otherPartyKey, b, p);
  }

  // Pad the key with zeros if it is less than 17 digits
  while (calculateLengthByIteration(sharedSecretKey) < 17) {
    sharedSecretKey *= 10;
  }

  // Print the shared secret key
  Serial.print("Shared secret key: ");
  Serial.println(sharedSecretKey);
}



void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Connect to the WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  
  // Set the MQTT broker and callback function
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // Choose a random private key between 1 and p-1
  a = random(1, p-1);
  b = random(1, p-1);
  
  // Compute the public key using g and a
  A = compute(g, a, p);
  
}

void loop() {
  // Connect to the MQTT broker if not connected
  if (!client.connected()) {
    Serial.println("Connecting to MQTT broker...");

    if (client.connect("publisher")) {
      Serial.println("Connected to MQTT broker");
      
      // Subscribe to the MQTT topic
      client.subscribe(MQTT_TOPIC_BOB, 1);
      
      // Publish the public key A to the MQTT topic
      char buffer[20];
      sprintf(buffer, "%ld", A);
      client.publish(MQTT_TOPIC_ALICE, buffer, true);
      
      // Print the public key A
      Serial.printf("Alice's public key is %s\n", buffer);

    }
  }
  
  // Loop through the client functions
  client.loop();
}
