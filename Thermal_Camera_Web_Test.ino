
/*
 MLX90640 thermal camera connected to a SparkFun Thing Plus - ESP32 WROOM
 Created by: Christopher Black
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>  // Used for I2C communication
#include <WebSocketsServer.h>
#include <Adafruit_AMG88xx.h>
#include "new_webpage.h"


// WiFi variables
char* ssid = "Moist IoT";
char* password = "sharktits2000";
WiFiServer server(80);


//declare socket related variables
WebSocketsServer webSocket = WebSocketsServer(81);

// AMG8833 variables
#define INTERPOLATED_ARRAY_SIZE 576

//Adafruit_AMG88xx amg;

#define AMG_COLS 8
#define AMG_ROWS 8

#define INTERPOLATED_COLS 24
#define INTERPOLATED_ROWS 24
static float amg8833[INTERPOLATED_COLS * INTERPOLATED_ROWS];

// Used to compress data to the client
char positive[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char negative[27] = "abcdefghijklmnopqrstuvwxyz";

TaskHandle_t TaskA;
/* this variable hold queue handle */
xQueueHandle xQueue;

int total = 0;

void interpolate_image(float *src, uint8_t src_rows, uint8_t src_cols, 
                       float *dest, uint8_t dest_rows, uint8_t dest_cols);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    // Connect to the WiFi network
    WiFi.begin(ssid, password);
    WiFi.setHostname("esp32thing1");
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        retry += 1;
        Serial.print(".");
        if (retry > 4 ) {
          // Retry after 5 seconds
          Serial.println("");
          WiFi.begin(ssid, password);
          retry = 0;
        }
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    if (!MDNS.begin("thermal")) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("ws", "tcp", 81);
        Serial.println("mDNS responder started");
    }
    
    server.begin();
    
    xQueue = xQueueCreate(1, sizeof(amg8833));
    xTaskCreatePinnedToCore(
      Task1,                  /* pvTaskCode */
      "Workload1",            /* pcName */
      100000,                   /* usStackDepth */
      NULL,                   /* pvParameters */
      1,                      /* uxPriority */
      &TaskA,                 /* pxCreatedTask */
      0);                     /* xCoreID */
    xTaskCreate(
      receiveTask,           /* Task function. */
      "receiveTask",        /* name of task. */
      10000,                    /* Stack size of task */
      NULL,                     /* parameter of the task */
      1,                        /* priority of the task */
      NULL);                    /* Task handle to keep track of created task */

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

int value = 0;
int dataVal = 0;

void loop(){
  webSocket.loop();
  
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {
    Serial.println("New Client.");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print("<script>const ipAddress = '"); 
            client.print(WiFi.localIP());
            client.print("'</script>");
            client.println();
            // the content of the HTTP response follows the header:
            client.print(canvas_htm);
            
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

// Capture thermal image on a different thread
void Task1( void * parameter )
{
//    int tick = 0;
    
    Adafruit_AMG88xx amg;
    if (!amg.begin()) {
      Serial.println("Could not find a valid AMG88xx sensor, check wiring!");
      while (1);
    }
    delay(100); // let sensor boot up
    Serial.println("AMG8xx online!");

    float pixels[AMG_COLS * AMG_ROWS];
    
    for( ;; )
    {
      amg.readPixels(pixels);
      
      // do interpolation
      float dest_2d[INTERPOLATED_ROWS * INTERPOLATED_COLS];
      int32_t t = millis();
      interpolate_image(pixels, AMG_ROWS, AMG_COLS, dest_2d, INTERPOLATED_ROWS, INTERPOLATED_COLS);
      //Serial.print("Interpolation took "); Serial.print(millis()-t); Serial.println(" ms");
      
//      tick += 1;
//      if (tick > 10) {
//        float maxReading = amg8833[0];
//        float minReading = amg8833[0];
//        float avgReading = amg8833[0];
//        for (int x = 0 ; x < INTERPOLATED_ROWS * INTERPOLATED_COLS ; x++)
//        {
//          if (isnan(amg8833[x])) {
//            continue;
//          }
//          avgReading = (avgReading + amg8833[x]) / 2;
//          if ( amg8833[x] > maxReading) {
//            maxReading = amg8833[x];
//          }
//          if ( amg8833[x] < minReading) {
//            minReading = amg8833[x];
//          }
//        }
//        // convert to fahrenheit?
//        avgReading = avgReading * 1.8 + 32;
//        maxReading = maxReading * 1.8 + 32;
//        minReading = minReading * 1.8 + 32;
//        String output = "Max:";
//        output.concat(maxReading);
//        String minOutput = "Min:";
//        minOutput.concat(minReading);
//        String avgOutput = "Avg:";
//        avgOutput.concat(avgReading);
//        //Serial.println(output);
//        //Serial.println(minOutput);
//        //Serial.println(avgOutput);
//        tick = 0;
//      }

      delay(50);
      
      /* time to block the task until the queue has free space */
      const TickType_t xTicksToWait = pdMS_TO_TICKS(500);
      xQueueSendToFront( xQueue, &dest_2d, xTicksToWait );
      
      const TickType_t xDelay = 20 / portTICK_PERIOD_MS; // 8 Hz is 1/8 second
      vTaskDelay(xDelay);
  }
}

void receiveTask( void * parameter )
{
  /* keep the status of receiving data */
  BaseType_t xStatus;
  /* time to block the task until data is available */
  const TickType_t xTicksToWait = pdMS_TO_TICKS(500);
  for(;;){
    /* receive data from the queue */
    xStatus = xQueueReceive( xQueue, &amg8833, xTicksToWait );
    /* check whether receiving is ok or not */
    if(xStatus == pdPASS){
//      compressAndSend();
      sendNoCompress();
      total += 1;
    }
  }
  vTaskDelete( NULL );
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("Socket Disconnected.");
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.println("Socket Connected.");
                // send message to client
                webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            // send message to client
            break;
        case WStype_BIN:
        case WStype_ERROR:      
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

void sendNoCompress(){
    String thermalFrame = "[";
   
    for (int i = 1; i < (INTERPOLATED_ROWS * INTERPOLATED_COLS) + 1; i++) { // Starts at 1 so that (i % 8) will be 0 at end of 8 pixels and newline will be added
      thermalFrame += amg8833[i - 1];
      if ( i != (INTERPOLATED_ROWS * INTERPOLATED_COLS) ){
        thermalFrame += ", ";
        if ( i % 24 == 0 ){
          thermalFrame += "\n";
        }
      }
    }
    thermalFrame += "]";

    char tf[thermalFrame.length()+1];

    thermalFrame.toCharArray( tf, thermalFrame.length()+1 );

    webSocket.broadcastTXT(tf);
}

// Some precision is lost during compression but data transfer speeds are
// much faster. We're able to get a higher frame rate by compressing data.
void compressAndSend() 
{
    String resultText = "";
    int numDecimals = 1;
    int accuracy = 8;
    int previousValue = round(amg8833[0] * pow(10, numDecimals));
    previousValue = previousValue - (previousValue % accuracy);
    resultText.concat(numDecimals);
    resultText.concat(accuracy);
    resultText.concat(previousValue);
    resultText.concat(".");
    char currentLetter = 'A';
    char previousLetter = 'A';
    int letterCount = 1;
    int columnCount = 32;
    
    for (int x = 1 ; x < INTERPOLATED_ROWS * INTERPOLATED_COLS; x += 1)
    {
        int currentValue = round(amg8833[x] * pow(10, numDecimals));
        currentValue = currentValue - (currentValue % accuracy);
        if(x % columnCount == 0) {
            previousValue = round(amg8833[x - columnCount] * pow(10, numDecimals));
            previousValue = previousValue - (previousValue % accuracy);
        }
        int correction = 0;
        int diffIndex = (int)(currentValue - previousValue);
        if(abs(diffIndex) > 0) {
            diffIndex = diffIndex / accuracy;
        }
        if(diffIndex > 25) {
            //correction = (diffIndex - 25) * accuracy;
            diffIndex = 25;
        } else if(diffIndex < -25) {
            //correction = (diffIndex + 25) * accuracy;
            diffIndex = -25;
        }

        if(diffIndex >= 0) {
            currentLetter = positive[diffIndex];
        } else {
            currentLetter = negative[abs(diffIndex)];
        }
        
        if(x == 1) {
            previousLetter = currentLetter;
        } else if(currentLetter != previousLetter) {
            
            if(letterCount == 1) {
                resultText.concat(previousLetter);
            } else {
                resultText.concat(letterCount);
                resultText.concat(previousLetter);
            }
            previousLetter = currentLetter;
            letterCount = 1;
        } else {
            letterCount += 1;
        }
        
        previousValue = currentValue - correction;
    }
    if(letterCount == 1) {
        resultText.concat(previousLetter);
    } else {
        resultText.concat(letterCount);
        resultText.concat(previousLetter);
    }
    webSocket.broadcastTXT(resultText);
}
