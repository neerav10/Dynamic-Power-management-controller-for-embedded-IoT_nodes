#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

/* =========================================================
 * WIFI CONFIGURATION
 * ========================================================= */

const char* ssid = "ND10";
const char* password = "Leomessi10";


/* =========================================================
 * SERVER CONFIGURATION
 * ========================================================= */

const char* serverUrl =
    "http://10.243.122.210:5000/api/data";


/* =========================================================
 * STM32 UART
 *
 * ESP32:
 *
 * GPIO16 = RX
 * GPIO17 = TX
 * ========================================================= */

#define STM32_RX_PIN 16
#define STM32_TX_PIN 17

#define UART_BAUD 115200


/* =========================================================
 * UART RECEIVE BUFFER
 * ========================================================= */

char uartBuffer[64];

uint8_t uartIndex = 0;


/* =========================================================
 * SENSOR DATA
 * ========================================================= */

float temperature = 0.0f;
float mq2PPM = 0.0f;


/* =========================================================
 * TIMER
 *
 * Send telemetry every 1 second
 * ========================================================= */

unsigned long lastSendTime = 0;

const unsigned long SEND_INTERVAL = 1000;


/* =========================================================
 * WIFI TIMER
 * ========================================================= */

unsigned long lastWiFiAttempt = 0;

const unsigned long WIFI_RETRY_INTERVAL = 5000;


/* =========================================================
 * PARSE STM32 UART PACKET
 *
 * Expected:
 *
 * TEMP:28.4,MQ2:325.6
 * ========================================================= */

bool ParseSensorPacket(char* packet)
{
    float temp;
    float ppm;

    int result = sscanf(
        packet,
        "TEMP:%f,MQ2:%f",
        &temp,
        &ppm
    );

    if (result == 2)
    {
        temperature = temp;
        mq2PPM = ppm;

        return true;
    }

    return false;
}


/* =========================================================
 * READ UART FROM STM32
 *
 * Packets end with:
 *
 * \n
 * ========================================================= */

void ReadSTM32UART()
{
    while (Serial2.available())
    {
        char c = Serial2.read();

        /* Ignore carriage return */
        if (c == '\r')
        {
            continue;
        }


        /* End of packet */
        if (c == '\n')
        {
            uartBuffer[uartIndex] = '\0';

            if (uartIndex > 0)
            {
                if (ParseSensorPacket(uartBuffer))
                {
                    Serial.print("STM32 -> Temperature: ");
                    Serial.print(temperature, 1);

                    Serial.print(" C | MQ2: ");
                    Serial.print(mq2PPM, 1);

                    Serial.println(" PPM");
                }
                else
                {
                    Serial.print("Invalid packet: ");
                    Serial.println(uartBuffer);
                }
            }

            /* Reset buffer */
            uartIndex = 0;
        }
        else
        {
            /* Prevent buffer overflow */
            if (uartIndex < sizeof(uartBuffer) - 1)
            {
                uartBuffer[uartIndex++] = c;
            }
            else
            {
                /* Overflow recovery */
                uartIndex = 0;
            }
        }
    }
}


/* =========================================================
 * WIFI CONNECTION
 * ========================================================= */

void MaintainWiFiConnection()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }


    unsigned long currentTime = millis();

    if ((currentTime - lastWiFiAttempt) <
        WIFI_RETRY_INTERVAL)
    {
        return;
    }


    lastWiFiAttempt = currentTime;


    Serial.println("Connecting to WiFi...");

    WiFi.begin(ssid, password);
}


/* =========================================================
 * SEND DATA TO SERVER
 * ========================================================= */

void SendTelemetry()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi not connected. Data not sent.");
        return;
    }


    HTTPClient http;


    /* Set connection timeout */
    http.setConnectTimeout(3000);

    /* Set response timeout */
    http.setTimeout(3000);


    if (!http.begin(serverUrl))
    {
        Serial.println("HTTP begin failed.");
        return;
    }


    /* JSON content */
    http.addHeader(
        "Content-Type",
        "application/json"
    );


    /* -----------------------------------------------------
     * Create JSON payload
     *
     * {
     *   "node_id":"esp32_01",
     *   "temperature":28.4,
     *   "mq2_ppm":325.6
     * }
     * ----------------------------------------------------- */

    String requestBody = "{";

    requestBody += "\"node_id\":\"esp32_01\",";
    requestBody += "\"temperature\":";
    requestBody += String(temperature, 2);
    requestBody += ",";
    requestBody += "\"mq2_ppm\":";
    requestBody += String(mq2PPM, 2);

    requestBody += "}";


    Serial.print("Sending POST: ");
    Serial.println(requestBody);


    /* Send POST */
    int httpResponseCode =
        http.POST(requestBody);


    /* -----------------------------------------------------
     * Process response
     * ----------------------------------------------------- */

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response Code: ");
        Serial.println(httpResponseCode);


        String response =
            http.getString();


        Serial.print("Server Response: ");
        Serial.println(response);
    }
    else
    {
        Serial.print("HTTP Error: ");

        Serial.println(
            http.errorToString(
                httpResponseCode
            )
        );
    }


    http.end();
}


/* =========================================================
 * SETUP
 * ========================================================= */

void setup()
{
    /* USB Serial Monitor */
    Serial.begin(115200);


    /* STM32 UART */
    Serial2.begin(
        UART_BAUD,
        SERIAL_8N1,
        STM32_RX_PIN,
        STM32_TX_PIN
    );


    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32 TELEMETRY NODE");
    Serial.println("==============================");


    /* Start WiFi */
    WiFi.mode(WIFI_STA);

    WiFi.begin(
        ssid,
        password
    );


    Serial.println("Starting WiFi connection...");
}


/* =========================================================
 * MAIN LOOP
 * ========================================================= */

void loop()
{
    /* -----------------------------------------------------
     * 1. Continuously read STM32 UART
     * ----------------------------------------------------- */

    ReadSTM32UART();


    /* -----------------------------------------------------
     * 2. Maintain WiFi connection
     * ----------------------------------------------------- */

    MaintainWiFiConnection();


    /* -----------------------------------------------------
     * 3. Print WiFi status
     * ----------------------------------------------------- */

    static bool previousWiFiState = false;

    bool currentWiFiState =
        (WiFi.status() == WL_CONNECTED);


    if (currentWiFiState != previousWiFiState)
    {
        previousWiFiState = currentWiFiState;


        if (currentWiFiState)
        {
            Serial.println();
            Serial.println("WiFi Connected!");

            Serial.print("ESP32 IP: ");
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println("WiFi Disconnected!");
        }
    }


    /* -----------------------------------------------------
     * 4. Send telemetry every 1 second
     *
     * No delay()
     * Uses millis()
     * ----------------------------------------------------- */

    unsigned long currentTime = millis();


    if ((currentTime - lastSendTime) >=
        SEND_INTERVAL)
    {
        lastSendTime = currentTime;

        SendTelemetry();
    }
}






















































































//#include <Arduino.h>
//
//void setup()
//{
//    Serial.begin(115200);
//
//    Serial2.begin(
//        115200,
//        SERIAL_8N1,
//        16,
//        17
//    );
//
//    Serial.println("ESP32 RX");
//}
//
//void loop()
//{
//    while (Serial2.available())
//    {
//        Serial.write(Serial2.read());
//    }
//}


































//#include <WiFi.h>
//#include <HTTPClient.h>
//
//// WiFi Configuration
//const char* ssid = "ND10";
//const char* password = "Leomessi10";
//
//// Server Endpoint (Replace with your server machine's local IP address)
//const char* serverUrl = "http://10.80.33.210:5000/api/data";
//
//void setup() {
//  Serial.begin(115200);
//  delay(1000);
//
//  // Connect to WiFi
//  WiFi.begin(ssid, password);
//  Serial.print("Connecting to WiFi");
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    Serial.print(".");
//  }
//  Serial.println("\nWiFi Connected!");
//  Serial.print("ESP32 IP: ");
//  Serial.println(WiFi.localIP());
//}
//
//void loop() {
//  if (WiFi.status() == WL_CONNECTED) {
//    HTTPClient http;
//
//    http.begin(serverUrl);
//    http.addHeader("Content-Type", "application/json");
//
//    // Generate dummy payload
//    float temperature = 25.0 + (random(0, 100) / 10.0);
//    float humidity = 50.0 + (random(0, 100) / 10.0);
//
//    String requestBody = "{\"node_id\":\"esp32_01\",\"temperature\":" + 
//                         String(temperature, 2) + 
//                         ",\"humidity\":" + 
//                         String(humidity, 2) + "}";
//
//    Serial.print("Sending POST request: ");
//    Serial.println(requestBody);
//
//    int httpResponseCode = http.POST(requestBody);
//
//    if (httpResponseCode > 0) {
//      String response = http.getString();
//      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
//      Serial.println("Response: " + response);
//    } else {
//      Serial.printf("Error occurred: %s\n", http.errorToString(httpResponseCode).c_str());
//    }
//
//    http.end();
//  } else {
//    Serial.println("WiFi Disconnected!");
//  }
//
//  delay(1000); // Send data every 5 seconds
//}













































//
//
//#include <WiFi.h>
//#include <HTTPClient.h>
//
///* =========================================================
// * WIFI CONFIGURATION
// * ========================================================= */
//
//const char* ssid = "ND10";
//const char* password = "Leomessi10";
//
//
///* =========================================================
// * SERVER
// * ========================================================= */
//
//const char* serverUrl =
//    "http://10.234.78.210/api/data";
//
//
///* =========================================================
// * ESP32 UART2
// *
// * GPIO16 = RX2
// * GPIO17 = TX2
// * ========================================================= */
//
//#define STM32_RX_PIN 16
//#define STM32_TX_PIN 17
//
//HardwareSerial STM32Serial(2);
//
//
///* =========================================================
// * UART BUFFER
// * ========================================================= */
//
//String uartLine = "";
//
//
///* =========================================================
// * SETUP
// * ========================================================= */
//
//void setup()
//{
//    Serial.begin(115200);
//
//    delay(1000);
//
//    /*
//     * STM32 UART
//     *
//     * 115200
//     * 8 data bits
//     * No parity
//     * 1 stop bit
//     */
//    STM32Serial.begin(
//        115200,
//        SERIAL_8N1,
//        STM32_RX_PIN,
//        STM32_TX_PIN
//    );
//
//
//    Serial.println();
//    Serial.println(
//        "================================"
//    );
//
//    Serial.println(
//        "ESP32 TELEMETRY GATEWAY"
//    );
//
//    Serial.println(
//        "STM32 UART -> WiFi -> SERVER"
//    );
//
//    Serial.println(
//        "================================"
//    );
//
//
//    /* -----------------------------------------------------
//     * CONNECT TO WIFI
//     * ----------------------------------------------------- */
//
//    WiFi.begin(
//        ssid,
//        password
//    );
//
//    Serial.print(
//        "Connecting to WiFi"
//    );
//
//
//    while (
//        WiFi.status() != WL_CONNECTED
//    )
//    {
//        delay(500);
//
//        Serial.print(".");
//    }
//
//
//    Serial.println();
//
//    Serial.println(
//        "WiFi Connected!"
//    );
//
//    Serial.print(
//        "ESP32 IP: "
//    );
//
//    Serial.println(
//        WiFi.localIP()
//    );
//}
//
//
///* =========================================================
// * PARSE STM32 UART LINE
// *
// * Expected:
// *
// * TEMP:28.4,MQ2:512.0
// *
// * ========================================================= */
//
//bool parseTelemetry(
//    String line,
//    float &temperature,
//    float &mq2_ppm
//)
//{
//    line.trim();
//
//
//    /*
//     * Find temperature
//     */
//    int tempStart =
//        line.indexOf("TEMP:");
//
//    int mq2Start =
//        line.indexOf(",MQ2:");
//
//
//    /*
//     * Validate format
//     */
//    if (
//        tempStart < 0 ||
//        mq2Start < 0
//    )
//    {
//        return false;
//    }
//
//
//    /*
//     * Extract temperature string
//     *
//     * Example:
//     *
//     * TEMP:28.4
//     */
//    String tempString =
//        line.substring(
//            tempStart + 5,
//            mq2Start
//        );
//
//
//    /*
//     * Extract MQ2 string
//     */
//    String mq2String =
//        line.substring(
//            mq2Start + 5
//        );
//
//
//    /*
//     * Convert strings to float
//     */
//    temperature =
//        tempString.toFloat();
//
//    mq2_ppm =
//        mq2String.toFloat();
//
//
//    return true;
//}
//
//
///* =========================================================
// * SEND TELEMETRY TO SERVER
// * ========================================================= */
//
//void sendToServer(
//    float temperature,
//    float mq2_ppm
//)
//{
//    /*
//     * Make sure WiFi is connected
//     */
//    if (
//        WiFi.status() != WL_CONNECTED
//    )
//    {
//        Serial.println(
//            "WiFi disconnected!"
//        );
//
//        return;
//    }
//
//
//    HTTPClient http;
//
//
//    /*
//     * Connect to Flask API
//     */
//    http.begin(serverUrl);
//
//
//    /*
//     * JSON content type
//     */
//    http.addHeader(
//        "Content-Type",
//        "application/json"
//    );
//
//
//    /*
//     * Build JSON
//     */
//    String requestBody =
//        "{"
//        "\"node_id\":\"stm32_01\","
//        "\"temperature\":" +
//        String(
//            temperature,
//            2
//        ) +
//        ","
//        "\"mq2_ppm\":" +
//        String(
//            mq2_ppm,
//            2
//        ) +
//        "}";
//
//
//    Serial.println();
//    Serial.println(
//        "Sending to server:"
//    );
//
//    Serial.println(
//        requestBody
//    );
//
//
//    /*
//     * HTTP POST
//     */
//    int httpResponseCode =
//        http.POST(
//            requestBody
//        );
//
//
//    /*
//     * Process response
//     */
//    if (
//        httpResponseCode > 0
//    )
//    {
//        String response =
//            http.getString();
//
//
//        Serial.print(
//            "HTTP Response Code: "
//        );
//
//        Serial.println(
//            httpResponseCode
//        );
//
//
//        Serial.print(
//            "Server Response: "
//        );
//
//        Serial.println(
//            response
//        );
//    }
//    else
//    {
//        Serial.print(
//            "HTTP Error: "
//        );
//
//        Serial.println(
//            http.errorToString(
//                httpResponseCode
//            )
//        );
//    }
//
//
//    http.end();
//}
//
//
///* =========================================================
// * LOOP
// * ========================================================= */
//
//void loop()
//{
//    /*
//     * Read STM32 UART
//     * until newline arrives.
//     */
//
//    while (
//        STM32Serial.available()
//    )
//    {
//        char c =
//            STM32Serial.read();
//
//
//        /*
//         * End of packet
//         */
//        if (
//            c == '\n'
//        )
//        {
//            /*
//             * Process complete line
//             */
//
//            if (
//                uartLine.length() > 0
//            )
//            {
//                Serial.print(
//                    "STM32 -> ESP32: "
//                );
//
//                Serial.println(
//                    uartLine
//                );
//
//
//                float temperature;
//
//                float mq2_ppm;
//
//
//                /*
//                 * Parse packet
//                 */
//                if (
//                    parseTelemetry(
//                        uartLine,
//                        temperature,
//                        mq2_ppm
//                    )
//                )
//                {
//                    /*
//                     * Send parsed values
//                     * to Flask server.
//                     */
//
//                    sendToServer(
//                        temperature,
//                        mq2_ppm
//                    );
//                }
//                else
//                {
//                    Serial.println(
//                        "Invalid STM32 packet"
//                    );
//                }
//
//
//                /*
//                 * Clear buffer
//                 */
//                uartLine = "";
//            }
//        }
//        else if (
//            c != '\r'
//        )
//        {
//            /*
//             * Add character
//             */
//            uartLine += c;
//
//
//            /*
//             * Safety against a corrupt/
//             * excessively long packet
//             */
//            if (
//                uartLine.length() > 100
//            )
//            {
//                uartLine = "";
//            }
//        }
//    }
//}
