#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "wifi_taufiq"
const char* password = "abcde123";
const char* mqtt_server = "broker.mqtt-dashboard.com";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

char keyArr[17];

// Helper functions
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
    Serial.println("connecting...");
  }

  randomSeed(micros());

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  byte key[] = "0123456789ABCDEF";
  byte iv[] = "0000000000000000";
  byte ciphertext[16];
  byte plaintext[16];
  int data_len;
  int original_data_len;

  memcpy(ciphertext, payload, length);
  data_len = length;

  decrypt(ciphertext, key, iv, plaintext, data_len);

  // Remove padding from the plaintext
  int padding_len = plaintext[15];  // The last byte indicates the padding length
  int plaintext_len = data_len - padding_len;
  plaintext[plaintext_len] = '\0';  // Null-terminate the string

}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.publish("8commerce", "ESP8266 connected and ready!");
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

  // Generate a random key
  randomSeed(analogRead(0));
  for (int i = 0; i < 16; i++) {
    byte randomValue = random(36);
    if (randomValue < 10) {
      keyArr[i] = '0' + randomValue;  // Digit
    } else {
      keyArr[i] = 'A' + randomValue - 10;  // Uppercase letter
    }
  }

  // Print the generated key
  Serial.print("Generated Key: ");
  for (int i = 0; i < 16; i++) {
    Serial.print((char)keyArr[i]);
  }
  Serial.println();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    
    lastMsg = now;
    ++value;

    byte key[16];
    byte iv[] = "0000000000000000";
    byte ciphertext[16];
    byte plaintext[16];
    int data_len;
    int original_data_len;

    Serial.println();
    // Prompt the user for the plaintext
    Serial.print("Enter the plaintext (up to 16 characters): ");
    while (Serial.available() == 0) {
      // Wait for user input
    }
    String plaintextInput = Serial.readStringUntil('\n');
    plaintextInput.trim();
    plaintextInput.toCharArray((char*)plaintext, 16);
    data_len = strlen((char*)plaintext);
    original_data_len = data_len;
    Serial.println((char*)plaintext);

    String keyGenerated = String(keyArr);
    // keyInput.trim();
    keyGenerated.toCharArray((char*)key, 17);
    // Serial.println((char*)key);

    // Pad data with zeros
    while (data_len % 16 != 0) {
      plaintext[data_len] = 0;
      data_len++;
    }

    // Encrypt the data
    Serial.println();
    encrypt(plaintext, key, iv, ciphertext, data_len);
    Serial.print("Encrypted data: ");
    for (int i = 0; i < data_len; i ++) {
      Serial.printf("%02X", ciphertext[i]);
    }
    Serial.println();

    // Publish the encrypted data
    client.publish("8commerce", (char*)ciphertext, data_len);
    client.publish("8commerce", (char*)key, 17);

    // Reset IV to its original value
    memcpy(iv, "0000000000000000", 16);
  }
}
