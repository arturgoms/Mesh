#include <Arduino.h>
#include <ArduinoJson.h>
#include <easyMesh.h>
#include <easyWebServer.h>
#include <easyWebSocket.h>
#include "animations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define   LED           16
#define   MESH_PREFIX     ""
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555

// globals
easyMesh  mesh;  // mesh global
extern NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip;  // using the method that works for sparkfun thing
extern NeoPixelAnimator animations; // NeoPixel animation management object
extern AnimationController controllers[]; // array of add-on controllers for my animations
os_timer_t  yerpTimer;
int myid=0;
int id[50];
char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char **) malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}
void setup() {
  Serial.begin( 115200 );
  pinMode(LED, OUTPUT);
  digitalWrite(LED,LOW);
  // setup mesh
//  mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | APPLICATION ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP | APPLICATION );  // set before init() so that you can see startup messages
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.setReceiveCallback( &receivedCallback );
  mesh.setNewConnectionCallback( &newConnectionCallback );

  // setups webServer
  webServerInit();

  // setup webSocket
  webSocketInit();
  webSocketSetReceiveCallback( &wsReceiveCallback );
  webSocketSetConnectionCallback( &wsConnectionCallback );

  mesh.debugMsg( STARTUP, "\nIn setup() my chipId=%d\n", mesh.getChipId());
  id[0] = mesh.getChipId();
  strip.Begin();
  strip.Show();

  animationsInit();

  os_timer_setfn( &yerpTimer, yerpCb, NULL );
  os_timer_arm( &yerpTimer, 1000, 1 );
}

void loop() {
  mesh.update();

  static uint16_t previousConnections;
  uint16_t numConnections = mesh.connectionCount();
  if( countWsConnections() > 0 )
    numConnections++;

  if ( previousConnections != numConnections ) {
    mesh.debugMsg( GENERAL, "loop(): numConnections=%d\n", numConnections);

    if ( numConnections == 0 ) {
      controllers[smoothIdx].nextAnimation = searchingIdx;
      controllers[searchingIdx].nextAnimation = searchingIdx;
      controllers[searchingIdx].hue[0] = 0.0f;
    } else {
      controllers[searchingIdx].nextAnimation = smoothIdx;
      controllers[smoothIdx].nextAnimation = smoothIdx;
    }

    sendWsControl();

    previousConnections = numConnections;
  }

  animations.UpdateAnimations();
  strip.Show();
}

void yerpCb( void *arg ) {
  static int yerpCount;
  int connCount = 0;
  int i, countadorConn = 0;
  String msg = "Yerp=";
  bool idExiste= true;
  msg += yerpCount++;

 

  mesh.debugMsg( APPLICATION, "msg-->%s<-- stationStatus=%u numConnections=%u\n", msg.c_str(), wifi_station_get_connect_status(), mesh.connectionCount( NULL ) );

  SimpleList<meshConnectionType>::iterator connection = mesh._connections.begin();
  
  while ( connection != mesh._connections.end() ) {
    mesh.debugMsg( APPLICATION, "\tconn#%d, chipId=%d subs=%s\n", connCount++, connection->chipId, connection->subConnections.c_str() );
    //Serial.println(mesh.getChipId());
    //Serial.println(connection->chipId);
    for( i = 0; i<=connCount; i++){
      Serial.println(i);
      Serial.println(id[i]);
      if (id[i] == 0){
        
      }else{
        if (id[i] != connection->chipId){
        id[connCount] =  connection->chipId;
        }
      }
    }
    
    connection++;
  }
  
  Serial.println(id[0]);Serial.println(id[1]);Serial.println(id[2]);        
  
  String ping("ping");
  broadcastWsMessage(ping.c_str(), ping.length(), OPCODE_TEXT);
  //sendWsControl();
}

void newConnectionCallback( bool adopt ) {
  if ( adopt == false ) {
    String control = buildControl();
    mesh.sendBroadcast( control );
  }
}
void receivedCallback( uint32_t from, String &msg ) {
  char **tokens;
  char bufferr[10];
  char data[100]="";
  mesh.debugMsg( APPLICATION, "receivedCallback(): from:%d\n", from);
  
  broadcastWsMessage(msg.c_str(), msg.length(), OPCODE_TEXT);

  mesh.debugMsg( APPLICATION, "control=%s\n", msg.c_str());
  //strcat(data, msg.c_str());
  //tokens = str_split(data, ';');
  //if (tokens)
   // {
    //    int e;
     //   for (e = 0; *(tokens + e); e++)
      //  {
       //   sprintf(bufferr, "%d", mesh.getChipId()); // transforma o id da placa em string 
        //    if(strcmp(*(tokens + e), bufferr) == 0){ // compara com o id na string se for igual ele entra no if
         //       //float value = atof(*(tokens + e + 1));
          //      if(strcmp(*(tokens + e + 1), bufferr) == 0){
           //     //if(value <= 0.5){
            //    digitalWrite(LED, LOW);
             //   }else{
              //  digitalWrite(LED, HIGH);
               // }
           // }
       // }
   // }
  DynamicJsonBuffer jsonBuffer(50);
  JsonObject& control = jsonBuffer.parseObject( msg );



  for ( int i = 0; i < ( mesh.connectionCount( NULL ) + 1 ); i++) {
    float hue = control[String(i)];
    controllers[smoothIdx].hue[i] = hue;
  }
}

void wsConnectionCallback( void ) {
  mesh.debugMsg( APPLICATION, "wsConnectionCallback():\n");
}

void wsReceiveCallback( char *payloadData ) {
  char data[100] = "";
  char bufferr[10];
  char a[15];
  String e;
  int num = mesh.connectionCount( NULL ), i=0, p=0, conexoes;
  mesh.debugMsg( APPLICATION, "wsReceiveCallback(): payloadData=%s\n", payloadData );
  //TODO Criar string com valores separados por ; e enviar no sendBoradcast
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& control = jsonBuffer.parseObject(payloadData);
  Serial.println(payloadData);
  
  Serial.println(control.size());
  //sprintf(bufferr, "%d", mesh.getChipId());
  //strcat(data, bufferr);
  //strcat(bufferr, "");
  //strcat(data, ";");
  //String valueStr = control["0"];
 //Serial.println(valueStr);
  //float chipidValue = valueStr.toFloat();
  //Serial.println(chipidValue);
  //if (chipidValue <= 0.5){
  //  digitalWrite(LED, LOW);
  //  }else{
  //  digitalWrite(LED, HIGH);            
 // }
  //sprintf(bufferr, "%s", valueStr.c_str());
  //strcat(data, bufferr);
  //strcat(bufferr, "");
  //strcat(data, ";");
  for(i=0;i<control.size(); i ++){
      sprintf(bufferr, "%d", id[i]);
      strcat(data, bufferr);
      strcat(bufferr, "");
      strcat(data, ";");
      sprintf(a, "%d", i);
      String value = control[a];
      sprintf(bufferr, "%s", value.c_str());
      strcat(data, bufferr);
      strcat(data, ";");
      strcat(bufferr, "");
  }
  Serial.println(data);
  String msg(data);
  mesh.sendBroadcast( msg );
  if ( strcmp( payloadData, "wsOpened") == 0) {  // hack to give the browser time to get the ws up and running
    mesh.debugMsg( APPLICATION, "wsReceiveCallback(): received wsOpened\n" );
    sendWsControl();
    return;
  }

  if (!control.success()) {   // Test if parsing succeeded.
    mesh.debugMsg( APPLICATION, "wsReceiveCallback(): parseObject() failed. payload=%s<--\n", payloadData);
    return;
  }
  
  uint16_t blips = mesh.connectionCount() + 1;
  if ( blips > MAX_BLIPS )
    blips = MAX_BLIPS;
    
  for ( int i = 0; i < blips; i++) {
    String temp(i);
    float hue = control[temp];
    controllers[smoothIdx].hue[i] = hue;
  }
}

void sendWsControl( void ) {
  mesh.debugMsg( APPLICATION, "sendWsControl():\n");
  
  String control = buildControl();
  broadcastWsMessage(control.c_str(), control.length(), OPCODE_TEXT);
}

String buildControl ( void ) {
  uint16_t blips = mesh.connectionCount() + 1;
  mesh.debugMsg( APPLICATION, "buildControl(): blips=%d\n", blips);

  if ( blips > 3 ) {
    mesh.debugMsg( APPLICATION, " blips out of range =%d\n", blips);
    blips = 3;
  }

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& control = jsonBuffer.createObject();
  for (int i = 0; i < blips; i++ ) {
    control[String(i)] = String(controllers[smoothIdx].hue[i]);
  }

  String ret;
  control.printTo(ret);
  return ret;
}


