#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>

const char* ssid = "Kirana tapi lancar";
const char* password = "abcde123";
const char* mqtt_server = "broker.mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient client(espClient);

// Variables to store the decryption key
byte decryptionKey[16];
bool hasDecryptionKey = false;

void swap(byte* a, byte* b) {
  byte temp = *a;
  *a = *b;
  *b = temp;
}

void key_schedule(byte* key, byte* round_keys) {
  int i, j;
  byte S[256];

  // Initialize S-box
  for (i = 0; i < 256; i++) {
    S[i] = i;
  }

  // Permute S-box using key
  j = 0;
  for (i = 0; i < 256; i++) {
    j = (j + key[i % 16] + S[i]) % 256;
    swap(&S[i], &S[j]);
  }

  // Generate round keys
  j = 0;
  for (i = 0; i < 40; i++) {
    round_keys[i] = S[(i + 1) % 256];
    j = (j + round_keys[i]) % 256;
    swap(&S[i], &S[j]);
  }
}

void encrypt(byte* data, byte* key, byte* iv, byte* ciphertext, int data_len) {
  byte round_keys[40];
  byte keystream[16];
  byte block[16];
  int i, j, k;

  key_schedule(key, round_keys);

  for (i = 0; i < data_len; i += 16) {
    memcpy(block, iv, 16);
    for (j = 0; j < 4; j++) {
      for (k = 0; k < 16; k++) {
        keystream[j * 4 + k] = block[k];
      }
      block[15]++;
      if (block[15] == 0) {
        block[14]++;
      }
    }
    for (j = 0; j < 16; j++) {
      keystream[j] ^= round_keys[j + 24];
    }

    for (j = 0; j < 16; j++) {
      ciphertext[i + j] = data[i + j] ^ keystream[j];
    }

    memcpy(iv, ciphertext + i, 16);
  }
}

void decrypt(byte* ciphertext, byte* key, byte* iv, byte* plaintext, int data_len) {
  byte round_keys[40];
  byte keystream[16];
  byte block[16];
  int i, j, k;

  key_schedule(key, round_keys);

  for (i = 0; i < data_len; i += 16) {
    memcpy(block, iv, 16);
    for (j = 0; j < 4; j++) {
      for (k = 0; k < 16; k++) {
        keystream[j * 4 + k] = block[k];
      }
      block[15]++;
      if (block[15] == 0) {
        block[14]++;
      }
    }
    for (j = 0; j < 16; j++) {
      keystream[j] ^= round_keys[j + 24];
    }

    for (j = 0; j < 16; j++) {
      plaintext[i + j] = ciphertext[i + j] ^ keystream[j];
    }

    memcpy(iv, ciphertext + i, 16);
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, "8commerce") == 0) {
    if (hasDecryptionKey) {
      byte iv[] = "0000000000000000";
      byte ciphertext[length];
      byte plaintext[length];
      int data_len = length;
      int original_data_len;

      memcpy(ciphertext, payload, length);

      // Decrypt the ciphertext
      decrypt(ciphertext, decryptionKey, iv, plaintext, data_len);

      // Remove padding from the plaintext
      int padding_len = plaintext[length - 1];  // The last byte indicates the padding length
      int plaintext_len = data_len - padding_len;
      plaintext[plaintext_len] = '\0';  // Null-terminate the string

      if (strlen((char*)plaintext)<17){
        Serial.print("Decrypted data: ");
        Serial.println((char*)plaintext);
        // Publish the decrypted plaintext
        client.publish("decrypted", (char*)plaintext);
      } else {
        
      }

    } else {
      Serial.println("Decryption key not set. Cannot decrypt ciphertext.");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.publish("decrypted", "ESP8266 connected and ready!");
      client.subscribe("8commerce");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Prompt the user to enter the decryption key
  Serial.println("Enter the decryption key (16 characters):");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read user input for decryption key if not set
  if (!hasDecryptionKey && Serial.available()) {
    String keyInput = Serial.readStringUntil('\n');
    keyInput.trim();

    if (keyInput.length() == 16) {
      keyInput.getBytes(decryptionKey, 17);
      hasDecryptionKey = true;
      Serial.println("Decryption key set.");
      
    } else {
      Serial.println("Invalid decryption key. Please enter 16 characters.");
    }

    // Clear the serial buffer
    while (Serial.available()) {
      Serial.read();
    }
  }
}