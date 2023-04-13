//Proyecto "Cuidado de agua adaptativo"
//Autores: Saúl Ávila, Manuel Guillén, Sofia Guadalupe y Andres Rivera
const byte personas = 1; // Cambiar el 1 por el número de personas que se encuentran en el hogar
//#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>   //Including HTTPClient.h library to use all api

//Librerías para la pantalla
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

// WiFi
#define WIFI_SSID "Sagitario"
#define WIFI_PASSWORD "Is@be!2671D!ego2567"

// Nombre servidor smtp
#define SMTP_HOST "smtp.gmail.com"

/** Elegimos el puerto smtp 
 * 25  or esp_mail_smtp_port_25
 * 465 or esp_mail_smtp_port_465
 * 587 or esp_mail_smtp_port_587
 Para enviar un correo por Gmail a través del puerto 465 (SSL), la opción de
 aplicaciones poco seguras debe estar habilitada. https://myaccount.google.com/lesssecureapps?pli=1  */
#define SMTP_PORT esp_mail_smtp_port_465

// Credenciales de la cuenta remitente
//#define AUTHOR_EMAIL "tu_correo@gmail.com"
//#define AUTHOR_PASSWORD "XXXXXXXXX"
#define AUTHOR_EMAIL "tecmailesp32@gmail.com"
#define AUTHOR_PASSWORD "xdzz dgii wvoo trcy"

AsyncWebServer server(80);

// Declaramos una variable para referenciar la sesión SMTP 
SMTPSession smtp;

// Prototipo de la funcion callback para obtener el estado del envio del correo
void smtpCallback(SMTP_Status status);

// Flag variable to keep track if email notification was sent or not
bool emailSent = false;

//Defines para la pantalla
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

#define TFT_GREY 0x5AEB
#define EEPROM_SIZE 3
#define LOOP_PERIOD 35 // Display updates every 35 ms

String apiKey = "2330271";              //Add your Token number that bot has sent you on WhatsApp messenger
String phone_number = "+5213339721124"; //Add your WhatsApp app registered phone number (same number that bot send you in url)

String url;                            //url String will be used to store the final generated URL

float ltx = 0;    // Saved x coord of bottom of needle
uint16_t osx = 120, osy = 120; // Saved x & y coords
uint32_t updateTime = 0;       // time for next update

int old_analog =  -999; // Value last displayed
int old_digital = -999; // Value last displayed

int value[6] = {0, 0, 0, 0, 0, 0};
int old_value[6] = { -1, -1, -1, -1, -1, -1};
int d = 0;

//Variables caudalimetro
volatile double waterFlow;
float litros = 0;
float litrosprevios = 0;
int litrosideales = 50*personas;
int dia1 = 0;
int dia2 = 0;
int dia3 = 0;
int dia4 = 0;
int dia5 = 0;
int dia6 = 0;
String textMsg = "";
bool interrupcion = false;
bool estado = HIGH;
bool SeguroMensaje = false;

//Timer interrupt
const byte LED_GPIO = 2;
volatile int interruptCounter;
volatile int blinkCounter = 0;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer()
{
  interrupcion = true;
}

String valor_consumo_graficar() {
  float lit = litros;
  // Convert temperature to Fahrenheit
  //t = 1.8 * t + 32;
  if (isnan(litros)) {    
    Serial.println("Algo salió mal :(");
    return "";
  }
  else {
    Serial.println(litros);
    return String(litros);
  }
}

void setup(void)
{
  tft.init();
  tft.setRotation(0);
  
  Serial.begin(115200);
  Serial.println();
  EEPROM.begin(EEPROM_SIZE);
  Serial.print("Conectando a punto de acceso");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(200);
  }
  Serial.println("");
  Serial.println("WiFi connectado.");
  Serial.println("Direccion IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  if(!SPIFFS.begin()){
    Serial.println("Ha ocurrido un error montando el SPIFFS");
    return;
  }
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });
  server.on("/consumoagua", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", valor_consumo_graficar().c_str());
  });
  // Start server
  server.begin();
//////////////////////////////////////////////////////////////////////////////////////////////////////////
  tft.fillScreen(TFT_BLACK);
  waterFlow = 0;
  pinMode(25, INPUT);
  //attachInterrupt(25, pulse, RISING);
  analogMeter(); // Draw analogue meter
  
  // Draw 6 linear meters
  byte d = 40;
  plotLinear("D1", 0, 160);
  plotLinear("D2", 1 * d, 160);
  plotLinear("D3", 2 * d, 160);
  plotLinear("D4", 3 * d, 160);
  plotLinear("D5", 4 * d, 160);
  plotLinear("D6", 5 * d, 160);
  updateTime = millis(); // Next update time

  // Inicialización del timer interrupt
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  //86400000000 para 24 horas
  timerAlarmWrite(timer, 100000000, true);
  timerAlarmEnable(timer);
}

void loop()
{
  if (digitalRead(25) == estado){
    waterFlow ++;
    estado = !estado;
  }
  if (waterFlow == 50) //450
  {
    waterFlow = 0;
    litros ++;
  }
  if (litros > litrosideales && SeguroMensaje == false){
    SeguroMensaje = true;
    message_to_whatsapp("Has excedido el límite de agua propuesto :(");  // you send your own message just change "hello from TechTOnions" to your message.
    }
  if (interrupcion == true){ // && !emailSent
    SeguroMensaje = false;
    dia1 = dia2;
    dia2 = dia3;
    dia3 = dia4;
    dia4 = dia5;
    dia5 = dia6;
    dia6 = litros;
    litrosprevios = litros;
    litros = 0;
    EEPROM.write(0, int(litrosprevios));
    EEPROM.commit();
    litrosprevios = EEPROM.read(0);
    litrosideales = EEPROM.read(1);
    if(litrosprevios > litrosideales){
      textMsg = String("Estas por encima de los ") + String(litrosideales) + String(" litros propuestos, puedes mejorar . ");
      if (litrosprevios-10 <= 50){
        EEPROM.write(1, 50);
        EEPROM.commit();
        textMsg = textMsg + String("Ahora vamos por los 50 litros ;)  ");
      }
      else{
        EEPROM.write(1, litrosprevios-10);
        EEPROM.commit();
        textMsg = textMsg + String("Intenta consumir menos agua, tu meta se ha ajustado a ") + String(litrosprevios-10) + String("litros.  ");
      }
    }
    else if(litrosprevios < litrosideales && litrosprevios < 50){
      textMsg = "Excelente, hoy consumiste menos de los 50 litros recomendados :D . ";
      EEPROM.write(1, 50);
      EEPROM.commit();
    }
    else if(litrosprevios < litrosideales && litrosprevios > 50){
      textMsg = String("Excelente, hoy consumiste menos de los ") + String(litrosideales) + String(" litros propuestos :D . ");
      if (litrosprevios-10 <= 50){
        EEPROM.write(1, 50);
        EEPROM.commit();
        textMsg = textMsg + String("Ahora vamos por los 50 litros ;)  ");
      }
      else{
        EEPROM.write(1, litrosprevios-10);
        EEPROM.commit();
        textMsg = textMsg + String("Veamos si puedes con otros 10 litros menos  :) ");
      }
    }
    else{
      textMsg = "Haz cumplido con la meta ;)";
    }
    //char buffer[1000];
    //sprintf(buffer, "El día de hoy consumiste %d litros de agua \n %s",litros, textMsg);
    String emailMessage = textMsg + String(" El día de hoy consumiste ") + String(litrosprevios) + String(" litros de agua");
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if(sendEmailNotification(emailMessage)) {
      Serial.println(emailMessage);
      emailSent = true;
    }
    else {
      Serial.println("Email failed to send");
    }   
    interrupcion = false;
  }
  
  if (updateTime <= millis())
  {
    updateTime = millis() + LOOP_PERIOD;

    d += 4; if (d >= 360) d = 0;

    //value[0] = map(analogRead(A0), 0, 1023, 0, 100); // Test with value form Analogue 0

    // Create a Sine wave for testing
    value[0] = dia1;
    value[1] = dia2;
    value[2] = dia3;
    value[3] = dia4;
    value[4] = dia5;
    value[5] = dia6;

    //unsigned long t = millis();

    plotPointer();

    plotNeedle(litros, 0);

    //Serial.println(millis()-t); // Print time taken for meter update
  }
}


// #########################################################################
//  Draw the analogue meter on the screen
// #########################################################################
void analogMeter()
{
  // Meter outline
  tft.fillRect(0, 0, 239, 126, TFT_GREY);
  tft.fillRect(5, 3, 230, 119, TFT_WHITE);
  tft.fillRect(5, 128, 230, 30, TFT_BLUE);
  
  tft.setTextColor(TFT_BLACK);  // Text colour
  tft.drawCentreString("Consumo semanal", 120, 132, 4);

  // Draw ticks every 5 degrees from -50 to +50 degrees (100 deg. FSD swing)
  for (int i = -50; i < 51; i += 5)
  {
    // Long scale tick length
    int tl = 15;

    // Coodinates of tick to draw
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (100 + tl) + 120;
    uint16_t y0 = sy * (100 + tl) + 140;
    uint16_t x1 = sx * 100 + 120;
    uint16_t y1 = sy * 100 + 140;

    // Coordinates of next tick for zone fill
    float sx2 = cos((i + 5 - 90) * 0.0174532925);
    float sy2 = sin((i + 5 - 90) * 0.0174532925);
    int x2 = sx2 * (100 + tl) + 120;
    int y2 = sy2 * (100 + tl) + 140;
    int x3 = sx2 * 100 + 120;
    int y3 = sy2 * 100 + 140;

    //Green zone limits
    if (i >= -50 && i < 0)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_GREEN);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_GREEN);
    }

    // Yellow zone limits
    if (i >= 0 && i < 25)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_YELLOW);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_YELLOW);
    }

    // Orange zone limits
    if (i >= 25 && i < 50)
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, TFT_RED);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_RED);
    }

    // Short scale tick length
    if (i % 25 != 0) tl = 8;

    // Recalculate coords incase tick lenght changed
    x0 = sx * (100 + tl) + 120;
    y0 = sy * (100 + tl) + 140;
    x1 = sx * 100 + 120;
    y1 = sy * 100 + 140;

    // Draw tick
    tft.drawLine(x0, y0, x1, y1, TFT_BLACK);

    // Check if labels should be drawn, with position tweaks
    if (i % 25 == 0)
    {
      // Calculate label positions
      x0 = sx * (100 + tl + 10) + 120;
      y0 = sy * (100 + tl + 10) + 140;
      switch (i / 25) {
        case -2: tft.drawCentreString("0", x0, y0 - 12, 2); break;
        case -1: tft.drawCentreString("25", x0, y0 - 9, 2); break;
        case 0: tft.drawCentreString("50", x0, y0 - 6, 2); break;
        case 1: tft.drawCentreString("75", x0, y0 - 9, 2); break;
        case 2: tft.drawCentreString("100", x0, y0 - 12, 2); break;
      }
    }

    // Now draw the arc of the scale
    sx = cos((i + 5 - 90) * 0.0174532925);
    sy = sin((i + 5 - 90) * 0.0174532925);
    x0 = sx * 100 + 120;
    y0 = sy * 100 + 140;
    // Draw scale arc, don't draw the last part
    if (i < 50) tft.drawLine(x0, y0, x1, y1, TFT_BLACK);
  }

  tft.drawString("L", 5 + 230 - 40, 119 - 20, 2); // Units at bottom right
  tft.drawCentreString("Litros hoy", 120, 70, 4); // Comment out to avoid font 4
  tft.drawRect(5, 3, 230, 119, TFT_BLACK); // Draw bezel line

  plotNeedle(0, 0); // Put meter needle at 0
}

// #########################################################################
// Update needle position
// This function is blocking while needle moves, time depends on ms_delay
// 10ms minimises needle flicker if text is drawn within needle sweep area
// Smaller values OK if text not in sweep area, zero for instant movement but
// does not look realistic... (note: 100 increments for full scale deflection)
// #########################################################################
void plotNeedle(int value, byte ms_delay)
{
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  char buf[8]; dtostrf(value, 4, 0, buf);
  tft.drawRightString(buf, 40, 119 - 20, 2);

  if (value < -10) value = -10; // Limit value to emulate needle end stops
  if (value > 110) value = 110;

  // Move the needle util new value reached
  while (!(value == old_analog))
  {
    if (old_analog < value) old_analog++;
    else old_analog--;

    if (ms_delay == 0) old_analog = value; // Update immediately id delay is 0

    float sdeg = map(old_analog, -10, 110, -150, -30); // Map value to angle
    // Calcualte tip of needle coords
    float sx = cos(sdeg * 0.0174532925);
    float sy = sin(sdeg * 0.0174532925);

    // Calculate x delta of needle start (does not start at pivot point)
    float tx = tan((sdeg + 90) * 0.0174532925);

    // Erase old needle image
    tft.drawLine(120 + 20 * ltx - 1, 140 - 20, osx - 1, osy, TFT_WHITE);
    tft.drawLine(120 + 20 * ltx, 140 - 20, osx, osy, TFT_WHITE);
    tft.drawLine(120 + 20 * ltx + 1, 140 - 20, osx + 1, osy, TFT_WHITE);

    // Re-plot text under needle
    tft.setTextColor(TFT_BLACK);
    tft.drawCentreString("Litros hoy", 120, 70, 4); // // Comment out to avoid font 4

    // Store new needle end coords for next erase
    ltx = tx;
    osx = sx * 98 + 120;
    osy = sy * 98 + 140;

    // Draw the needle in the new postion, magenta makes needle a bit bolder
    // draws 3 lines to thicken needle
    tft.drawLine(120 + 20 * ltx - 1, 140 - 20, osx - 1, osy, TFT_RED);
    tft.drawLine(120 + 20 * ltx, 140 - 20, osx, osy, TFT_MAGENTA);
    tft.drawLine(120 + 20 * ltx + 1, 140 - 20, osx + 1, osy, TFT_RED);

    // Slow needle down slightly as it approaches new postion
    if (abs(old_analog - value) < 10) ms_delay += ms_delay / 5;

    // Wait before next update
    delay(ms_delay);
  }
}

// #########################################################################
//  Draw a linear meter on the screen
// #########################################################################
void plotLinear(char *label, int x, int y)
{
  int w = 36;
  tft.drawRect(x, y, w, 155, TFT_GREY);
  tft.fillRect(x + 2, y + 19, w - 3, 155 - 38, TFT_WHITE);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawCentreString(label, x + w / 2, y + 2, 2);

  for (int i = 0; i < 110; i += 10)
  {
    tft.drawFastHLine(x + 20, y + 27 + i, 6, TFT_BLACK);
  }

  for (int i = 0; i < 110; i += 50)
  {
    tft.drawFastHLine(x + 20, y + 27 + i, 9, TFT_BLACK);
  }

  tft.fillTriangle(x + 3, y + 127, x + 3 + 16, y + 127, x + 3, y + 127 - 5, TFT_RED);
  tft.fillTriangle(x + 3, y + 127, x + 3 + 16, y + 127, x + 3, y + 127 + 5, TFT_RED);

  tft.drawCentreString("---", x + w / 2, y + 155 - 18, 2);
}

// #########################################################################
//  Adjust 6 linear meter pointer positions
// #########################################################################
void plotPointer(void)
{
  int dy = 187;
  byte pw = 16;

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  // Move the 6 pointers one pixel towards new value
  for (int i = 0; i < 6; i++)
  {
    char buf[8]; dtostrf(value[i], 4, 0, buf);
    tft.drawRightString(buf, i * 40 + 36 - 5, 187 - 27 + 155 - 18, 2);

    int dx = 3 + 40 * i;
    if (value[i] < 0) value[i] = 0; // Limit value to emulate needle end stops
    if (value[i] > 100) value[i] = 100;

    while (!(value[i] == old_value[i]))
    {
      dy = 187 + 100 - old_value[i];
      if (old_value[i] > value[i])
      {
        tft.drawLine(dx, dy - 5, dx + pw, dy, TFT_WHITE);
        old_value[i]--;
        tft.drawLine(dx, dy + 6, dx + pw, dy + 1, TFT_RED);
      }
      else
      {
        tft.drawLine(dx, dy + 5, dx + pw, dy, TFT_WHITE);
        old_value[i]++;
        tft.drawLine(dx, dy - 6, dx + pw, dy - 1, TFT_RED);
      }
    }
  }
}


void smtpCallback(SMTP_Status status)
{
  /* Imprime el estado actual*/
  Serial.println(status.info());

  /* Imprime los resultados del envio */
  if (status.success())
  {
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Mensajes enviados con éxito: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Mensajes no enviados por falla: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Mensaje No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Estado: %s\n", result.completed ? "enviado" : "no enviado");
      ESP_MAIL_PRINTF("Fecha/Hora: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Destinatario: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Asunto: %s\n", result.subject);
    }
    Serial.println("----------------\n");

    //You need to clear sending result as the memory usage will grow up as it keeps the status, timstamp and
    //pointer to const char of recipients and subject that user assigned to the SMTP_Message object.

    //Because of pointer to const char that stores instead of dynamic string, the subject and recipients value can be
    //a garbage string (pointer points to undefind location) as SMTP_Message was declared as local variable or the value changed.

    smtp.sendingResult.clear();
  }
}


bool sendEmailNotification(String emailMessage){
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);
 /* Declaramos una Sesion para poder configurar*/
  ESP_Mail_Session session;

  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
    /* Declaramos una variable para el mensaje */
    SMTP_Message message;
  
    /* Especificar los encabezados*/
    message.sender.name = "Monitoreo de agua";
    message.sender.email = AUTHOR_EMAIL;
    message.subject = "Resultados de consumo de hoy";
    message.addRecipient("Pruebas", "tecmailesp32@gmail.com");

  message.text.content = emailMessage.c_str();  //message.text.content = buffer.c_str();
  
    /** Set de caracteres para el texto:
     * us-ascii
     * utf-8
     * utf-7
     * The default value is utf-8
    */
    message.text.charSet = "utf-8";
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit; // sin codificación
  
    /** Prioridad del mensaje
     * esp_mail_smtp_priority_high or 1
     * esp_mail_smtp_priority_normal or 3
     * esp_mail_smtp_priority_low or 5
     * The default value is esp_mail_smtp_priority_low
    */
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  
  
    /* Conectar al servidor con la configuracios de la sesión*/
    !smtp.connect(&session); //if (!smtp.connect(&session))
      //return;
  
    /* Envía el correo y cierra la sesión */
    if (!MailClient.sendMail(&smtp, &message)){
      Serial.println("Error sending Email, " + smtp.errorReason());
      return false;
    }
  
    // borrar los datos de los resultados el envio
    smtp.sendingResult.clear();
    ESP_MAIL_PRINTF("Liberar memoria: %d\n", MailClient.getFreeHeap());
    message.clear();
    return true;
}


void  message_to_whatsapp(String message)       // user define function to send meassage to WhatsApp app
{
  //adding all number, your api key, your message into one complete url
  url = "https://api.callmebot.com/whatsapp.php?phone=" + phone_number + "&apikey=" + apiKey + "&text=" + urlencode(message);

  postData(); // calling postData to run the above-generated url once so that you will receive a message.
}

void postData()     //userDefine function used to call api(POST data)
{
  int httpCode;     // variable used to get the responce http code after calling api
  HTTPClient http;  // Declare object of class HTTPClient
  http.begin(url);  // begin the HTTPClient object with generated url
  httpCode = http.POST(url); // Finaly Post the URL with this function and it will store the http code
  if (httpCode == 200)      // Check if the responce http code is 200
  {
    Serial.println("Sent ok."); // print message sent ok message
  }
  else                      // if response HTTP code is not 200 it means there is some error.
  {
    Serial.println("Error."); // print error message.
  }
  http.end();          // After calling API end the HTTP client object.
}

String urlencode(String str)  // Function used for encoding the url
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}
