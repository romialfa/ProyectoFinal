
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>
//#include <HTTPClient.h>
#include <WebServer.h>
////////// EEPROM////////////////////////
#define AddrDatos 0 //primero dos bytes cant datos guardados
#define AddrMed 100 // 16 y 16 datos del display guardados
#define AddrCal 200 // 4 y 4 datos calibracion
#define AddrSSID 300 //50 byte SSID
#define AddrPASS 350 //50 byte PASS
#define AddrUSER 400 //50 byte usuario
#define CANTMED 5

/////////////// SEBASTIAN///////////////
#define DEFAULT_VREF    1100 
#define NO_OF_SAMPLES   64
#define MAP_DATATYPE float
#define CANT_ELEMENTOS_TABLA_CALIBRACION  4 //como mínimo se necesitan 2 puntos
#define PRESION_MAXIMA  180 //mmHg
#define PRESION_MINIMA  0 //mmHg
#define DEFAULT_STACK_SIZE 1024

//-------- Variables Globales: WiFi + EEPROM-------------
// Credenciales WiFi
char ssid[16];
char pass[16];
const char* ssid2 = "tensiometro";  // nombre de red wifi
const char* password2 = "123456"; // Contraseña red wifi - opcional softAP()
// Variables Globales
//WiFiClient espClient;
WebServer server(80);

//String apiKeyValue = "tPmAT5Ab3j7F9";
String dispositivo = "ESP32";
char *host = "201.178.46.72" ;
String strhost = "proyectofinal.ignorelist.com";
String strurl = "/medicion/reportar";
  
// Variables Globales
int contconexion = 0;
String header; // Variable para guardar el HTTP request (lo que manda el browser)
String mensaje; //respuestas

// ------------------CODIGO HTML PARA PAGINA DE CONFIGURACION-------------------
// en este string se guarda TODO el codigo HTML
String pagina = "<!DOCTYPE html>"
"<html lang='es'>"
"<head>"
"<title>Configuracion Tensiometro</title>"
"<meta charset='UTF-8' name='viewport' content='width=device-width, initial-scale=1.0'>"
"<link rel='stylesheet' href='estilo.css'>"
"</head>"
"<body>"
"<center> <div class='n'><p>"
"<b><font color='#000000' face='georgia' size='4'><marquee width='400' scrollamount='5' bgcolor='#FFFFFF'>Registro para usuarios de Tensiometro</marquee></font></b></p><center>"
"<div>"
"<h1 id = 'titulo ppal'> ACCESO WEB</h1>"
"<h2 id = 'titulo'> SERVIDOR WEB ESP32</h2>"
"<h3 id = 'subtitulo'> Modo EstaciÃ³n (STA)</h3>"
"</div>"
"<a href='escanear'><button class='boton'>ESCANEAR REDES</button>"
"</a><br><br>"
"</form>"
"<form action='guardar_conf' method='get'>"
"SSID:<br>"
"<input class='input1' name='ssid' type='text' placeholder='SSID' maxlength='30' size='30' id='SSID' ><br><br>"
"PASSWORD:<br>"
"<input class='input1' name='pass' type='password' placeholder='Contrasenia' maxlength='30' size='30' id='pass'><br><br>"
"<input class='boton' type='submit' value='ACTUALIZAR'/><br><br>"
"<form action='guardar_conf_mail' method='get'>"
"MAIL:<br>"
"<input class='input1' name='mail' type='text' placeholder='E-mail' maxlength='30' size='30' id='mail' ><br><br>"
"<input class='boton' type='submit' value='GUARDAR'/><br><br>"
"</form>";
//"</body>"
//"</html>";

String paginafin = "</body>"
"</html>";

//Estructura para elementos/filas de la tabla de calibración
typedef struct elementos_tabla_calibracion {
  float presion_mmHg;
  float cuenta_ADC; //se puso float ya que se trabaja con promedio de muestras
} elemento_tabla_calibracion;
SemaphoreHandle_t xSem_tablaCalibracion;
SemaphoreHandle_t xSem_calibrar;  //se libera al activarse la secuencia (presionar pulsador) que inicia la calibración
elemento_tabla_calibracion tabla_temporal[CANT_ELEMENTOS_TABLA_CALIBRACION];  //Reservo en memoria una cantidad de elementos/filas para la tabla de calibracion temporal



// DEFINICION DE GPIO
const int Adc1Rampa = 35;
const int Adc0Pulsos = 34;
const int BombaPin = 13;
const int ValvulaPin = 12;
const int LedEsp= 2;
const int LedPlaca = 14;
const int Botton15= 15;
const int Botton02= 15;
const int Botton04= 15;

struct sDisplay{
  char fila1[16];
  char fila2[16];
};
#define MUESTRAS 40 
#define TAMANNO_ARRAY_DATOS 2000
int sistolica,distolica;
float datosRampa[TAMANNO_ARRAY_DATOS];
float datosPulsos[TAMANNO_ARRAY_DATOS];
/* datosRampaCuenta[TAMANNO_ARRAY_DATOS];
float datosPulsosCuenta[TAMANNO_ARRAY_DATOS];
unsigned long datosTiempo[TAMANNO_ARRAY_DATOS];*/
float LimiteSupRampa=1.65,LimiteInfRampa=0.5,LimiteSupRampaCal=1.9;// los limites son en valores de cuentas del ADC
float adcRampa, adcPulsos,adcValor;
int eeAddress = 0;
  
void app_Wait( void *pvParameters );
void app_Teclado( void *pvParameters );
void app_Display( void *pvParameters );
void app_Medicion( void *pvParameters );
void app_Calibracion( void *pvParameters );
void app_Configuracion( void *pvParameters );
void app_Visualizacion( void *pvParameters );
void app_Resetear( void *pvParameters );

LiquidCrystal_I2C lcd(0x27, 16, 2);
QueueHandle_t ColaDisplay,Cola_Tecl_Wait,Cola_Wait_Tecl,Cola_Wait_Conf,Cola_Conf_Wait,Cola_Wait_Cal,Cola_Cal_Wait,Cola_Wait_Med,Cola_Med_Wait,Cola_Vis_Wait,Cola_Wait_Vis,Cola_Res_Wait,Cola_Wait_Res;
float conversion_Cuentas_Presion(float );
// the setup function runs once when you press reset or power the board
void setup() {
  // initialize serial communication at 115200 bits per second:
  /////////////////SEBASTIAN/////////////////
  static uint8_t ucParameterToPass;
  TaskHandle_t xHandle = NULL;
  xSem_tablaCalibracion = xSemaphoreCreateMutex();
  xSem_calibrar = xSemaphoreCreateBinary();
  xSemaphoreGive(xSem_calibrar);  //los semáforos binarios se crean TOMADOS, entonces lo libero
  static elemento_tabla_calibracion tabla_calibracion[CANT_ELEMENTOS_TABLA_CALIBRACION];  //Reservo en memoria una cantidad de elementos/filas para la tabla de calibración
  ////////////////////////////////////////////////
  Serial.begin(115200);
  adcAttachPin(Adc1Rampa);
  adcAttachPin(Adc0Pulsos);
  lcd.init();
  lcd.backlight();
  pinMode(LedPlaca, OUTPUT);
  pinMode(BombaPin, OUTPUT);
  pinMode(ValvulaPin, OUTPUT);
  pinMode(Botton15,INPUT);
  pinMode(LedEsp,OUTPUT);
  digitalWrite(LedEsp, HIGH);
  delay(2000);
  digitalWrite(LedEsp, LOW);
  delay(2000);
  EEPROM.begin(512);
  Cola_Wait_Med = xQueueCreate(1, sizeof(char[16]));
  Cola_Med_Wait = xQueueCreate(1, sizeof(char[16]));
  Cola_Cal_Wait = xQueueCreate(1, sizeof(char[16]));
  Cola_Wait_Cal = xQueueCreate(1, sizeof(char[16]));
  Cola_Conf_Wait = xQueueCreate(1, sizeof(char[16]));
  Cola_Wait_Conf = xQueueCreate(1, sizeof(char[16]));
  Cola_Tecl_Wait = xQueueCreate(1, sizeof(char[16]));
  Cola_Wait_Tecl = xQueueCreate(1, sizeof(char[16]));
  Cola_Vis_Wait= xQueueCreate(1, sizeof(char[16]));
  Cola_Wait_Vis= xQueueCreate(1, sizeof(char[16]));
  Cola_Res_Wait= xQueueCreate(1, sizeof(char[16]));
  Cola_Wait_Res= xQueueCreate(1, sizeof(char[16]));
  ColaDisplay = xQueueCreate(1, sizeof(sDisplay));
    
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(app_Medicion,"appMedicion",8192,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Wait,"appWait",4096,NULL,2,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Configuracion,"appConfiguracion",4096,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Calibracion,"appCalibracion",4096,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Visualizacion,"appVisualizacion",4096,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Resetear,"appResetear",4096,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Teclado,"appTeclado",1024,NULL,2,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Display,"appDisplay",1024,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  
}

void loop()
{
  // Empty. Things are done in Tasks.
}

///////////////////////////TAREA DISPLAY///////////////////////////////////////////
void app_Display(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  sDisplay datosDisplay;
  for (;;)
    {
        //Serial.println("Display");
        xQueueReceive(ColaDisplay, &datosDisplay, portMAX_DELAY);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(datosDisplay.fila1);
        lcd.setCursor(0, 1);
        lcd.print(datosDisplay.fila2);
        
        vTaskDelay(500); 
        
   }
}
///////////////////////////TAREA TECLADO///////////////////////////////////////////
void app_Teclado(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int pushbotton; 
  char estado[16]="esperar";
  sDisplay datosDisplay;
  int ledState = LOW;     // the current state of LED
  for (;;)
  {
    //Serial.print("AppTeclado  estado:");
    //Serial.println(estado);
    xQueueReceive(Cola_Wait_Tecl,&estado,100);
    digitalWrite(LedEsp, LOW); 
    if(digitalRead(Botton15)==HIGH&&strcmp(estado,"esperar")==0)
    {
        vTaskDelay(50);
        ledState = LOW;
        while(digitalRead(Botton15)==HIGH&&strcmp(estado,"esperar")==0)
        {
             pushbotton++;
             
             ledState = !ledState;
             digitalWrite(LedEsp, ledState); 
             vTaskDelay(500);             
        }
        if(pushbotton<=4)
          {
            //se presiono boton menos de 2 segundos
            strcpy(estado,"medir");
            Serial.println("se pulso boton para medir");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
          }
        if(pushbotton>4&&pushbotton<=8)
          {
            //se presiono boton mas de 3 segundos
            strcpy(estado,"calibrar");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
          }
        if(pushbotton>8&&pushbotton<=12)
          {
            //se presiono boton mas de 6 segundos
            strcpy(estado,"configurar");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
          }
        if(pushbotton>12&&pushbotton<=25)
          {
            //se presiono boton mas de 6 segundos
            strcpy(estado,"visualizar");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
          }
        if(pushbotton>25)
          {
            //se presiono boton mas de 6 segundos
            strcpy(estado,"resetear");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
          }
        pushbotton=0;
    }
    if(digitalRead(Botton15)==HIGH&&strcmp(estado,"medir")==0)
    {
        vTaskDelay(50);
        if(digitalRead(Botton15)==HIGH&&strcmp(estado,"medir")==0)
        {
            //se cancela medicion  
            strcpy(estado,"cancelarmed");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
            //strcpy(estado,"esperar");
        }
    
    }
    if(digitalRead(Botton15)==HIGH&&strcmp(estado,"calibrar")==0)
    {
        vTaskDelay(50);
        if(digitalRead(Botton15)==HIGH&&strcmp(estado,"calibrar")==0)
        {
            //se cancela calibracion  
            strcpy(estado,"pushcal");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
            strcpy(estado,"calibrar");
        }
    
    }
    if(digitalRead(Botton15)==HIGH&&strcmp(estado,"configurar")==0)
    {
        vTaskDelay(50);
        if(digitalRead(Botton15)==HIGH&&strcmp(estado,"configurar")==0)
        {
            //se cancela configuracion  
            strcpy(estado,"cancelarconf");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
            strcpy(estado,"esperar");
        }
    
    }
    if(digitalRead(Botton15)==HIGH&&strcmp(estado,"visualizar")==0)
    {
        vTaskDelay(50);
        if(digitalRead(Botton15)==HIGH&&strcmp(estado,"visualizar")==0)
        {
            //se cancela configuracion  
            strcpy(estado,"cancelarvis");
            xQueueSend(Cola_Tecl_Wait,&estado, portMAX_DELAY);
            //strcpy(estado,"esperar");
        }
    
    }
  vTaskDelay(500);  // one tick delay (15ms) in between reads for stability
  }
}
///////////////////////////TAREA WAIT///////////////////////////////////////////
void app_Wait(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int cont =0,flag=0,i;
  char estado[16]="esperar";
  String auxiliar;
  char cadena[16];
  char cadena2[16];
  sDisplay datosDisplay;
  for (;;)
  {
    //Serial.print("AppWait  estado:");
    //Serial.println(estado);
    memset(datosDisplay.fila1, 0,16);
    memset(datosDisplay.fila2, 0,16);
    if(strcmp(estado,"esperar")==0)
    {
        xQueueReceive(Cola_Tecl_Wait,&estado,100);
        switch(cont){
          case 1:
              strcpy(datosDisplay.fila1,"Para Medicion..");
              strcpy(datosDisplay.fila2,"Push boton 1s");
              xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
              break;
           case 4: 
              strcpy(datosDisplay.fila1,"Para Calibrar...");
              strcpy(datosDisplay.fila2,"Push boton 3s");
              xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
              break;
           case 8:
              strcpy(datosDisplay.fila1,"Para Configurar.");
              strcpy(datosDisplay.fila2,"Push boton 5s");
              xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
              break;
           case 12:
              cont=0;
              break;
          } 
        cont++;    
    }
    if(strcmp(estado,"medir")==0)
    {
            Serial.println("estyo en wait apretando boton medir");
            xQueueSend(Cola_Wait_Med,&estado, portMAX_DELAY);
            xQueueReceive(Cola_Med_Wait,&estado,portMAX_DELAY);
    }
    if(strcmp(estado,"midiendo")==0)
    {
            xQueueReceive(Cola_Tecl_Wait,&estado,100);
            if(strcmp(estado,"cancelarmed")==0)
            {
                  xQueueSend(Cola_Wait_Med,&estado, portMAX_DELAY);
                  xQueueReceive(Cola_Med_Wait,&estado,portMAX_DELAY);                      
            }
            xQueueReceive(Cola_Med_Wait,&estado,100);
    }
    if(strcmp(estado,"finmed")==0)
    {
            vTaskDelay(3000);
            strcpy(estado,"esperar");
            xQueueSend(Cola_Wait_Tecl,&estado, portMAX_DELAY);
    }
    if(strcmp(estado,"calibrar")==0)
    {
            xQueueSend(Cola_Wait_Cal,&estado, portMAX_DELAY);
            xQueueReceive(Cola_Cal_Wait,&estado,portMAX_DELAY);
    }
    if(strcmp(estado,"calibrando")==0)
    {
            xQueueReceive(Cola_Tecl_Wait,&estado,100);
            if(strcmp(estado,"pushcal")==0)
            {
                  xQueueSend(Cola_Wait_Cal,&estado, portMAX_DELAY);                      
            }
            strcpy(estado,"calibrando");
            xQueueReceive(Cola_Cal_Wait,&estado,100);
    }
 
    if(strcmp(estado,"fincal")==0)
    {
            vTaskDelay(5000);
            strcpy(estado,"esperar");
            xQueueSend(Cola_Wait_Tecl,&estado, portMAX_DELAY);
    }
    if(strcmp(estado,"configurar")==0)
    {
            xQueueSend(Cola_Wait_Conf,&estado, portMAX_DELAY);
            xQueueReceive(Cola_Conf_Wait,&estado,portMAX_DELAY);
    }
    if(strcmp(estado,"configurando")==0)
    {
            xQueueReceive(Cola_Tecl_Wait,&estado,100);
            if(strcmp(estado,"cancelarconf")==0)
            {
                  xQueueSend(Cola_Wait_Conf,&estado, portMAX_DELAY);                      
            }
            xQueueReceive(Cola_Conf_Wait,&estado,100);
    }
 
    if(strcmp(estado,"finconf")==0)
    {
            vTaskDelay(5000);
            strcpy(estado,"esperar");
            xQueueSend(Cola_Wait_Tecl,&estado, portMAX_DELAY);
    }
    if(strcmp(estado,"visualizar")==0)
    {
            xQueueSend(Cola_Wait_Vis,&estado, portMAX_DELAY);
            xQueueReceive(Cola_Vis_Wait,&estado,portMAX_DELAY);
    }
    if(strcmp(estado,"visualizando")==0)
    {
            xQueueReceive(Cola_Tecl_Wait,&estado,100);
            if(strcmp(estado,"cancelarvis")==0)
            {
                  xQueueSend(Cola_Wait_Vis,&estado, portMAX_DELAY);                      
                  xQueueReceive(Cola_Vis_Wait,&estado,portMAX_DELAY);
            }
            xQueueReceive(Cola_Vis_Wait,&estado,100);
    }
 
    if(strcmp(estado,"finvis")==0)
    {
            vTaskDelay(5000);
            strcpy(estado,"esperar");
            xQueueSend(Cola_Wait_Tecl,&estado, portMAX_DELAY);
    }
    if(strcmp(estado,"resetear")==0)
    {
            xQueueSend(Cola_Wait_Res,&estado, portMAX_DELAY);
            xQueueReceive(Cola_Res_Wait,&estado,portMAX_DELAY);
    }
    if(strcmp(estado,"reseteando")==0)
    {
            xQueueReceive(Cola_Res_Wait,&estado,100);
    }
 
    if(strcmp(estado,"finres")==0)
    {
            vTaskDelay(5000);
            strcpy(estado,"esperar");
            xQueueSend(Cola_Wait_Tecl,&estado, portMAX_DELAY);
    }
    vTaskDelay(500); 
  }
}           
///////////////// POLINOMIO PARA MEDICION DE ADC ////////////////////////////
double ReadVoltage (byte pin) {
  double lectura  = analogRead (pin); // El voltaje de referencia es 3v3, por lo que la lectura máxima es 3v3 = 4095 en el rango de 0 a 4095
  if(lectura < 1 || lectura> 4095 ) return  0 ;
  //return -0.000000000009824 * pow (lectura, 3) + 0.000000016557283 * pow (lectura, 2) + 0.000854596860691 * lectura + 0.065440348345433;
  return - 0.000000000000016 * pow (lectura, 4 ) + 0.000000000118171 * pow (lectura, 3 ) - 0.000000301211691 * pow (lectura, 2 ) + 0.001109019271794 * lectura + 0.034143524634089 ;
}

///////////////////////////TAREA MEDICION/////////////////////////////////////////// 
void app_Medicion(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  float maximoValor,auxiliar;
  int maximoIndex,sisIndex,disIndex;
  float sisValor,disValor,presionSistolica,presionDistolica;
  float pulsaciones;
  int flag,cont=0,i,f;
  float kdis=0.8,ksis=0.4;
  char estado[16];
  char dato[16];
  char cadena[16];
  char cad2[2];
  char cad4[4];
  char cadena2[16];
  String lectura;
  sDisplay datosDisplay;
  int cantdatosguardados;
  unsigned long tiempoi,tiempof;
  float tiempot;
  int contmaximos,flagmaximo,flagarranque;
  int cantmuestras;
  //cargo tabla de calibracion de la eeprom
  for (i=0;i<CANT_ELEMENTOS_TABLA_CALIBRACION;i++)
  {
       lectura=leerEEPROM(AddrCal+8*i,4);
       tabla_temporal[i].presion_mmHg=lectura.toInt();
       Serial.print("mmgh");
       Serial.print(tabla_temporal[i].presion_mmHg);
       lectura=leerEEPROM(AddrCal+4+8*i,4);
       tabla_temporal[i].cuenta_ADC=lectura.toInt();     
       Serial.print(" cuentas ");
       Serial.println(tabla_temporal[i].cuenta_ADC);
  }
  for (;;)
  {
      
        memset(datosDisplay.fila1, 0,16);
        memset(datosDisplay.fila2, 0,16);
        strcpy(estado,"esperar");
        xQueueReceive(Cola_Wait_Med,&estado,portMAX_DELAY);   
        Serial.println("AppMedicion");
        lectura=leerEEPROM(AddrDatos,2);
        cantdatosguardados=lectura.toInt();
        Serial.print("cantdatosguardados ");
        Serial.println(cantdatosguardados);
        if(cantdatosguardados==5)// la pila de datos esta llena, borro el primerpo
        {
            quitardatosEEPROM();
            cantdatosguardados=4;
        }
        strcpy(estado,"midiendo");
        xQueueSend(Cola_Med_Wait,&estado,portMAX_DELAY);
        LimiteInfRampa=ReadVoltage(Adc1Rampa);
        sprintf(cadena,"%1.2f",LimiteInfRampa);
        strcpy(datosDisplay.fila1,"..app_Medicion..");
        strcpy(datosDisplay.fila2,cadena);
        xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
        digitalWrite(BombaPin, HIGH);
        LimiteInfRampa=LimiteInfRampa+0.04;
        Serial.print("LimiteInfRampa ");
        Serial.println(LimiteInfRampa);
        // Inflado hasta limite superior rampa
        do{
              adcValor = ReadVoltage(Adc1Rampa);
              //VRampa=(float)(((float)ValorRampa*3.3)/4096);
              vTaskDelay(40);
              xQueueReceive(Cola_Wait_Med,&estado,100);
        }while(adcValor<=LimiteSupRampa&&strcmp(estado,"cancelarmed")!=0) ;
        digitalWrite(BombaPin, LOW);
        cont=0;
        if(strcmp(estado,"cancelarmed")!=0)
        {
              vTaskDelay(1000);
              cont=0;
              // desinflado natural y guarda valores hasta limite inferior rampa
              for(i=0;i<TAMANNO_ARRAY_DATOS;i++)
              {
                  datosRampa[i]=0;
                  datosPulsos[i]=0;
              } 
              flagmaximo=0;
              contmaximos=0;
              cantmuestras=0;
              do{
                    adcRampa=0;
                    adcPulsos=0;
                    //promedia valor de adc por numero de muestras
                    for(i=0;i<MUESTRAS;i++)
                    {
                        adcRampa=adcRampa+ReadVoltage(Adc1Rampa);
                        adcPulsos=adcPulsos+ReadVoltage(Adc0Pulsos);
                        //VRampa=(float)(((float)ValorRampa*3.3)/4096);
                    }
                    datosRampa[cont]=adcRampa/MUESTRAS;
                    datosPulsos[cont]=adcPulsos/MUESTRAS;    
                    if(cantmuestras<6)
                    {
                          if(datosPulsos[cont]>0.1&&flagmaximo==0)
                          {
                              flagmaximo=1; 
                              contmaximos++;
                              if(contmaximos==1)tiempoi=millis();
                              if(contmaximos==6)tiempof=millis();        
                          }
                          if (datosPulsos[cont]<0.1&&flagmaximo==1)
                          {
                              flagmaximo=0;
                              cantmuestras++;
                              
                          }
                    }
                    vTaskDelay(40);
                    adcValor=datosRampa[cont];
                    cont++;
                    //Serial.println(adcValor);
                    xQueueReceive(Cola_Wait_Med,&estado,1);
              }while((adcValor>=LimiteInfRampa)&&(cont<TAMANNO_ARRAY_DATOS)&&(strcmp(estado,"cancelarmed")!=0));
         }
         digitalWrite(ValvulaPin, HIGH);
         //elimino primeras muestras
         for(i=0;i<20;i++)
         {
          datosPulsos[i]=0;
         }
         //procesamiento de los valores
         //busco maximo dentro del array datosPulsos
         if(strcmp(estado,"cancelarmed")!=0)
         {
              
              maximoValor=0;
              for(i=0;i<cont;i++)
              {
                   if(datosPulsos[i]>maximoValor)
                   {
                       maximoValor=datosPulsos[i];
                       maximoIndex=i;
                   }
                   /*Serial.print(datosRampa[i]);
                   Serial.print(";");
                   Serial.println(datosPulsos[i]);*/
              }
              //multiplico para valor de ksis y busco valor 
              sisValor=(ksis*datosPulsos[maximoIndex]);
              Serial.print("sisvalor ");
              Serial.print(sisValor);
              sisIndex=0;
              for(i=0;i<maximoIndex;i++)
              {
                   if(datosPulsos[i]>sisValor)
                   {
                       sisIndex=i;
                       break ;
                   }
              }
              Serial.print(" sisIndex ");
              Serial.println(sisIndex);
              //multiplico para valor de kdis y busco valor, recorro array al revez 
              disValor=(kdis*datosPulsos[maximoIndex]);
              Serial.print("disvalor ");
              Serial.print(disValor);
              disIndex=0;
              for(i=cont;i>maximoIndex;i--)
              {
                   if(datosPulsos[i]>disValor)
                   {   
                       disIndex=i;
                       break;
                   }
              }
              Serial.print(" disIndex ");
              Serial.println(disIndex);
              tiempot=tiempof-tiempoi;
              tiempot=tiempot/1000;
              Serial.print( "tiempototal ");
              Serial.print(tiempot);
              pulsaciones=(((cantmuestras-1)*60)/tiempot);
              Serial.print(" Pulsaciones ");
              Serial.println(pulsaciones);
              vTaskDelay(7000);
              digitalWrite(ValvulaPin, LOW);
              vTaskDelay(4000);
              Serial.print( "maximo valores ");
              Serial.print(datosRampa[maximoIndex]);
              Serial.print( " pulso ");
              Serial.println(datosPulsos[maximoIndex]);
              Serial.print( "sis valores ");
              Serial.print(datosRampa[sisIndex]);
              Serial.print( " pulso ");
              Serial.println(datosPulsos[sisIndex]);
              Serial.print( "dis valores ");
              Serial.print(datosRampa[disIndex]);
              Serial.print( " pulso ");
              Serial.println(datosPulsos[disIndex]);
              //convierto valor de cuentas a presion y muestro sistolica y distolica en disp
              auxiliar=datosRampa[sisIndex];
              // paso de mili a cuentas 
              presionSistolica=(auxiliar*4096)/3.3;
              presionSistolica=cuentaADC2presion(tabla_temporal,presionSistolica);
              Serial.print( " sistolica ");
              Serial.println(presionSistolica);
              vTaskDelay(500);
              auxiliar=datosRampa[disIndex];
              // paso de mili a cuentas 
              presionDistolica=(auxiliar*4096)/3.3;
              presionDistolica=cuentaADC2presion(tabla_temporal,presionDistolica);
              stringdisplay(presionSistolica,presionDistolica,pulsaciones);
              Serial.print( " distolica ");
              Serial.println(presionDistolica);
              for(i=0;i<cont;i++)
              {
                   Serial.print(datosRampa[i]);
                   Serial.print(";");
                   Serial.print(datosPulsos[i]);
                   Serial.print(";");
                   Serial.println(cuentaADC2presion(tabla_temporal,(datosRampa[i]*4096)/3.3));
              }
              i=cantdatosguardados;
              sprintf(cad4,"%1.0f",presionSistolica);
              grabarEEPROM (AddrMed+12*i,cad4,4);
              sprintf(cad4,"%1.0f",presionDistolica);
              grabarEEPROM (AddrMed+4+12*i,cad4,4);
              sprintf(cad4,"%1.0f",pulsaciones);
              grabarEEPROM (AddrMed+8+12*i,cad4,4);
              memset(cad2,0,2);
              i++;
              sprintf(cad2,"%d",i); //sumo uno al valor de cantidad datos grabados
              grabarEEPROM (AddrDatos, cad2,2);
              /////////////////////////ENVIO DE DATOS SERVIDOR/////////////////////////77
              vTaskDelay(10000);
              memset(datosDisplay.fila1, 0,16);
              memset(datosDisplay.fila2, 0,16);
              strcpy (datosDisplay.fila1,"...DATOS SEND...");
              strcpy (datosDisplay.fila2,"..boton cancela.");
              xQueueSend(ColaDisplay,&datosDisplay, portMAX_DELAY);
              xQueueReceive(Cola_Wait_Med,&estado,5000);
              if(strcmp(estado,"cancelarmed")!=0)
              {
                  digitalWrite(LedEsp, HIGH);
                  vTaskDelay(2000);
                  digitalWrite(LedEsp, LOW);
                  envioservidorweb(presionSistolica,presionDistolica,pulsaciones);
              }
         }else
              {
                    envioservidorweb(11,6,76);
                    memset(datosDisplay.fila1, 0,16);
                    memset(datosDisplay.fila2, 0,16);
                    strcpy (datosDisplay.fila1,"...MEDICION...");
                    strcpy (datosDisplay.fila2,"..CANCELADA..");
                    xQueueSend(ColaDisplay,&datosDisplay, portMAX_DELAY);
                    vTaskDelay(7000);
                    digitalWrite(ValvulaPin, LOW);
              }
         strcpy(estado,"finmed");
         xQueueSend(Cola_Med_Wait,&estado,portMAX_DELAY);                  
         vTaskDelay(500);
    }
}
void envioservidorweb(int maximo,int minimo,int pulsaciones)
{
        String lectura;
        char ssid[16];
        char pass[16];
        lectura=leerEEPROM(AddrSSID,16);
        lectura.toCharArray(ssid, 16);
        lectura=leerEEPROM(AddrPASS,16);
        lectura.toCharArray(pass,16); 
        WiFi.begin(ssid, pass);
        while (WiFi.status() != WL_CONNECTED and contconexion <50) { //Cuenta hasta 50 si no se puede conectar lo cancela
              ++contconexion;
              vTaskDelay(500);
              Serial.print(".");
        }
        if (contconexion <50) { //hay conexion de wifi
              //para usar con ip fija
              IPAddress ip(192,168,0,156); 
              IPAddress gateway(192,168,0,1); 
              IPAddress subnet(255,255,255,0); 
              WiFi.config(ip, gateway, subnet); 
              Serial.println("");
              Serial.println("WiFi conectado");
              Serial.println(WiFi.localIP());
              //lectura=leerEEPROM(AddrUSER,16);
              lectura="caca@gmail.com";
              enviardatos("email=" + lectura + "&password=" + String(123456)+"&presion_sistolica="+String(1)+"&presion_distolica="+String(2)+"&pulsaciones="+String(3));    
        }
        else { 
              Serial.println("");
              Serial.println("Error de conexion");
        }
}

String enviardatos(String datos) 
{
     String linea = "error";
     WiFiClient client;
     //strhost.toCharArray(host, 49);
     if (!client.connect(host, 80)) {
          Serial.println("Fallo de conexion");
          return linea;
     }
     client.print(String("POST ") + strurl + " HTTP/1.1" + "\r\n" + 
               "Host: " + strhost + "\r\n" +
               "Accept: */*" + "*\r\n" +
               "Content-Length: " + datos.length() + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded" + "\r\n" +
               "\r\n" + datos);            
      vTaskDelay(10);
      Serial.print("Enviando datos a SQL...");
      unsigned long timeout = millis();
      while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                  Serial.println("Cliente fuera de tiempo!");
                  client.stop();
                  return linea;
            }
      }
      // Lee todas las lineas que recibe del servidro y las imprime por la terminal serial
      while(client.available()){
            linea = client.readStringUntil('\r');
            Serial.println(linea);
      }  
       //Serial.println(linea);
      return linea;
}

void stringdisplay(float maximo, float minimo, float pulsaciones)
{
    char cadena[16];
    char auxiliar[16];
    sDisplay datosDisplay;
    memset(datosDisplay.fila1, 0,16);
    memset(datosDisplay.fila2, 0,16);
    memset(auxiliar, 0,16);
    strcpy(cadena,"Max ");
    strcat(auxiliar,cadena);
    sprintf(cadena,"%1.0f",maximo);
    strcat(auxiliar,cadena);
    strcpy(cadena," Min ");
    strcat(auxiliar,cadena);
    sprintf(cadena,"%1.0f",minimo);
    strcat(auxiliar,cadena);
    strcpy (datosDisplay.fila1,auxiliar);
    memset(auxiliar, 0,16);
    strcpy(auxiliar,"Pul ");
    sprintf(cadena,"%1.0f",pulsaciones);
    strcat(auxiliar,cadena);
    strcpy (datosDisplay.fila2,auxiliar);
    xQueueSend(ColaDisplay,&datosDisplay, portMAX_DELAY); 
}
///////////////////////////TAREA RESETEAR///////////////////////////////////////////
void app_Resetear(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int i,flag=0;
  char estado[16];
  char cadena[16];
  char cad4[4];
  char cad2[2];
  String lectura;
  int cantdatosguardados;
  sDisplay datosDisplay;
  for (;;)
  {
        memset(datosDisplay.fila1,0,16);
        memset(datosDisplay.fila2,0,16);
        xQueueReceive(Cola_Wait_Res,&estado, portMAX_DELAY);
        strcpy(datosDisplay.fila1,"..app_Reset..");
        strcpy(datosDisplay.fila2,".............");
        xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
        strcpy(estado,"reseteando");
        xQueueSend(Cola_Res_Wait,&estado,portMAX_DELAY);
        vTaskDelay(1000);
        lectura=leerEEPROM(AddrDatos,2);
        cantdatosguardados=lectura.toInt();
        memset(cadena,0,16);
        memset(cad4,0,4);
        for(i=0;i<cantdatosguardados;i++)
        {
              grabarEEPROM((AddrMed+(12*i)),cad4,4);
              grabarEEPROM((AddrMed+4+(12*i)),cad4,4);
              grabarEEPROM((AddrMed+8+(12*i)),cad4,4);
        }
        Serial.println("ACA LLEGA");
        for(i=0;i<CANT_ELEMENTOS_TABLA_CALIBRACION;i++)
        {
              grabarEEPROM((AddrCal+(8*i)),cad4,4);
              grabarEEPROM((AddrCal+4+(8*i)),cad4,4);
        }
        grabarEEPROM(AddrSSID,cadena,16);
        grabarEEPROM(AddrPASS,cadena,16);
        grabarEEPROM(AddrUSER,cadena,16);
        i=0;
        Serial.println("ACA LLEGA2");
        sprintf(cad2,"%d",i);
        grabarEEPROM(AddrDatos,cad2,2);
        strcpy(estado,"finres");
        for(i=0;i<CANT_ELEMENTOS_TABLA_CALIBRACION;i++)
        {
              Serial.println(leerEEPROM(AddrCal+(8*i),4));
              Serial.println("------ ");
              Serial.println(leerEEPROM(AddrCal+4+(8*i),4));
        }
        xQueueSend(Cola_Res_Wait,&estado,portMAX_DELAY);
        
        vTaskDelay(500);   
   }
}    
///////////////////////////TAREA VISUALIZACION///////////////////////////////////////////
void app_Visualizacion(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int i,flag=0;
  char estado[16];
  char cadena[16];
  char cad4[4];
  String lectura;
  int maximo,minimo,pulsaciones;
  int cantdatosguardados;
  sDisplay datosDisplay;
  for (;;)
  {
        memset(datosDisplay.fila1,0,16);
        memset(datosDisplay.fila2,0,16);
        xQueueReceive(Cola_Wait_Vis,&estado, portMAX_DELAY);
        strcpy(datosDisplay.fila1,"..app_Visual..");
        strcpy(datosDisplay.fila2,".............");
        xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
        strcpy(estado,"visualizando");
        xQueueSend(Cola_Vis_Wait,&estado,portMAX_DELAY);
        vTaskDelay(3000);
        while(strcmp(estado,"cancelarvis")!=0)
        {
              xQueueReceive(Cola_Wait_Vis,&estado, 100);
              lectura=leerEEPROM(AddrDatos,2);
              cantdatosguardados=lectura.toInt();
              Serial.print(" cantidad guardada ");
              Serial.println(cantdatosguardados);
              for(i=cantdatosguardados-1;i>=0;i--)
              {
                   memset(datosDisplay.fila1,0,16);
                   memset(datosDisplay.fila2,0,16);
                   strcpy(datosDisplay.fila1,"Medcion Num...");
                   sprintf(cadena,"%d",i+1);
                   strcpy(datosDisplay.fila2,cadena);
                   xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
                   vTaskDelay(2000);
                   lectura=leerEEPROM((AddrMed+(12*i)),4);/// datos de mediciones en la EEPROM apartir de la posicion 100 se dguardan de a 12 sis,dis,pul
                   maximo=lectura.toInt();
                   lectura=leerEEPROM((AddrMed+4+(12*i)),4);
                   minimo=lectura.toInt();
                   lectura=leerEEPROM((AddrMed+8+(12*i)),4);
                   pulsaciones=lectura.toInt();
                   stringdisplay(maximo,minimo,pulsaciones);
                   vTaskDelay(2000); 
              }      
              vTaskDelay(500);
        }
        for(i=0;i<CANT_ELEMENTOS_TABLA_CALIBRACION;i++)
        {
              Serial.println(leerEEPROM(AddrCal+(8*i),4));
              Serial.println("------ ");
              Serial.println(leerEEPROM(AddrCal+4+(8*i),4));
        }
        strcpy(estado,"finvis");
        xQueueSend(Cola_Vis_Wait,&estado,portMAX_DELAY);
        vTaskDelay(500);   
   }
}    
///////////////////////////TAREA CALIBRACION///////////////////////////////////////////
void app_Calibracion(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int i,flag=0;
  char estado[16];
  char cadena[16];
  char cad4[4];
  sDisplay datosDisplay;
  for (;;)
  {
        xSemaphoreTake( xSem_calibrar, portMAX_DELAY);
        xSemaphoreTake( xSem_tablaCalibracion, portMAX_DELAY);
        float paso_mmHg = (PRESION_MAXIMA-PRESION_MINIMA)/(CANT_ELEMENTOS_TABLA_CALIBRACION-1);
        //elemento_tabla_calibracion tabla_temporal[CANT_ELEMENTOS_TABLA_CALIBRACION];  //Reservo en memoria una cantidad de elementos/filas para la tabla de calibracion temporal
        memset(datosDisplay.fila1,0,16);
        memset(datosDisplay.fila2,0,16);
        xQueueReceive(Cola_Wait_Cal,&estado, portMAX_DELAY);
        strcpy(datosDisplay.fila1,"..app_Calibr..");
        strcpy(datosDisplay.fila2,".............");
        xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
        strcpy(estado,"calibrando");
        xQueueSend(Cola_Cal_Wait,&estado,portMAX_DELAY);
        digitalWrite(BombaPin, HIGH);
        // Inflado hasta limite superior rampa
        do{
              adcValor = ReadVoltage(Adc1Rampa);
              vTaskDelay(40);
              //xQueueReceive(Cola_Wait_Med,&estado,100);
        }while(adcValor<=LimiteSupRampaCal) ;
        digitalWrite(BombaPin, LOW);
        vTaskDelay(1000);
        // desinflado natural y guarda valores apretando boton
        for(int i = CANT_ELEMENTOS_TABLA_CALIBRACION-1; i>=0; i--)
            {
                    tabla_temporal[i].presion_mmHg = PRESION_MINIMA+paso_mmHg*i;
                    Serial.println(tabla_temporal[i].presion_mmHg);
                    memset(datosDisplay.fila1,0,16);
                    memset(datosDisplay.fila2,0,16);
                    sprintf(cadena,"%1.0f",tabla_temporal[i].presion_mmHg);
                    strcpy(datosDisplay.fila1,"push boton en..");
                    strcpy(datosDisplay.fila2,cadena);
                    xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
                    xQueueReceive(Cola_Wait_Cal,&estado,portMAX_DELAY);
                    tabla_temporal[i].cuenta_ADC = ADC_muestraPromediada(NO_OF_SAMPLES);
                    vTaskDelay(pdMS_TO_TICKS(5));
            }
         digitalWrite(ValvulaPin, HIGH);
         mostrar_tabla_calibracion(tabla_temporal);  //ToDo: sacar
         digitalWrite(ValvulaPin, LOW);
         //copio la tabla de calibracion temporal en la tabla de calibracion finaL
         Serial.println("antes de copiar");
         //memcpy(pvParameters, tabla_temporal, sizeof(elemento_tabla_calibracion)*CANT_ELEMENTOS_TABLA_CALIBRACION); //ToDo: tabla_temporal en tabla calibracio
         //mostrar_tabla_calibracion((elemento_tabla_calibracion *) pvParameters);     
         //GUARDAR DATOS EN EEPROM
         /////////////////////////////////////
         for(i=0;i<CANT_ELEMENTOS_TABLA_CALIBRACION;i++)
         {
                sprintf(cad4,"%1.0f",tabla_temporal[i].presion_mmHg);
                grabarEEPROM(AddrCal+8*i,cad4,4);
                sprintf(cad4,"%1.0f",tabla_temporal[i].cuenta_ADC);
                grabarEEPROM(AddrCal+4+8*i,cad4,4);
         } 
         xSemaphoreGive( xSem_tablaCalibracion );
         xSemaphoreGive( xSem_calibrar);
         strcpy(estado,"fincal");
         xQueueSend(Cola_Cal_Wait,&estado,portMAX_DELAY);
         vTaskDelay(500);
  }
}
///////////////////////////TAREA CONFIGURACION///////////////////////////////////////////
void app_Configuracion(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int i=0,flag=0;
  char estado[16];
  char cadena[16];
  char ssid[16];
  char pass[16];
  String lectura;
  sDisplay datosDisplay;
  for (;;) // A Task shall never return or exit.
  {
        memset(datosDisplay.fila1,0,16);
        memset(datosDisplay.fila2,0,16);
        Serial.println("Config");
        xQueueReceive(Cola_Wait_Conf,&estado, portMAX_DELAY);
        strcpy(datosDisplay.fila1,"..app_Config..");
        strcpy(datosDisplay.fila2,"boton cancela");
        xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
        strcpy(estado,"configurando");
        xQueueSend(Cola_Conf_Wait,&estado,portMAX_DELAY);
        vTaskDelay(3000);
        memset(datosDisplay.fila1,0,16);
        memset(datosDisplay.fila2,0,16);
        lectura=leerEEPROM(AddrSSID,16);
        lectura.toCharArray(ssid, 16);
        strcpy(datosDisplay.fila1,ssid);
        lectura=leerEEPROM(AddrPASS,16);
        lectura.toCharArray(pass,16);
        strcpy(datosDisplay.fila2,pass);
        xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
        vTaskDelay(5000);
        xQueueReceive(Cola_Wait_Conf,&estado, 100);
        if(strcmp(estado,"cancelarconf")!=0)
        {
            WiFi.softAP(ssid2,password2);
            IPAddress myIP = WiFi.softAPIP();
            Serial.print("IP del acces point: ");
            Serial.println(myIP);
            Serial.println("WebServer iniciado");         
            server.on("/escanear", escanear); //Escanean las redes wifi disponibles  
            server.on("/", paginaconf); //esta es la pagina de configuracion
            server.on("/guardar_conf", guardar_conf); //Graba en la eeprom la configuracion de red wifi
            server.on("/guardar_conf_mail", guardar_conf_mail); //Graba en la eeprom la configuracion de usuario
            server.onNotFound(handleNotFound); //falta de respuesta 
            server.begin();
            while(strcmp(estado,"cancelarconf")!=0)
            {
                  xQueueReceive(Cola_Wait_Conf,&estado,100);
                  server.handleClient();
            }
        }
        if(strcmp(estado,"cancelarconf")==0)
        {
            memset(datosDisplay.fila1, 0,16);
            memset(datosDisplay.fila2, 0,16);
            strcpy(datosDisplay.fila1,"..CONFIG..");
            strcpy(datosDisplay.fila2,"...Saliendo..");
           xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
        }
        strcpy(estado,"finconf");
        xQueueSend(Cola_Conf_Wait,&estado,portMAX_DELAY);
        vTaskDelay(500);
  }
  
}
///////////////////////////FUNCIONES ACCES POINT///////////////////////////////////////////

//---------------------------ESCANEAR----------------------------
void escanear() {  
  int n = WiFi.scanNetworks(); //devuelve el número de redes encontradas
  Serial.println("escaneo terminado");
  if (n == 0) { //si no encuentra ninguna red
    Serial.println("no se encontraron redes");
    mensaje = "no se encontraron redes";
  }  
  else
  {
    Serial.print(n);
    Serial.println(" redes encontradas");
    mensaje = ""; // vacio el mensaje
    for (int i = 0; i < n; ++i)
    {
      // agrega al STRING "mensaje" la información de las redes encontradas 
      mensaje = (mensaje) + "<p>" + String(i + 1) + ": " + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + ") Ch: " + WiFi.channel(i) + " Enc: " + WiFi.encryptionType(i) + " </p>\r\n";
      //WiFi.encryptionType 5:WEP 2:WPA/PSK 4:WPA2/PSK 7:open network 8:WPA/WPA2/PSK
      delay(10);
    }
    Serial.println(mensaje);
    paginaconf();
  }
}
// Responder a la url raíz (root /)
void handleConnectionRoot() {
  server.send(200, "text/html", pagina);
}

void handleNotFound(){
  server.send(404, "text/plain", "Not found");
}
//---------------------GUARDAR CONFIGURACION RED y PASS-------------------------
void guardar_conf() {
  String auxiliar;
  char ssid[16];
  char pass[16];
  auxiliar=server.arg("ssid");
  auxiliar.toCharArray(ssid, 16);
  Serial.print("SSID: ");
  Serial.println(auxiliar);//Recibimos los valores que envia por GET el formulario web
  grabarEEPROM(AddrSSID,ssid,16); // se graba en posicion 0 de la memoria
  auxiliar=server.arg("pass");
  auxiliar.toCharArray(pass,16);
  Serial.print("PASS: ");
  Serial.println(auxiliar);
  grabarEEPROM(AddrPASS,pass,16);  //graba en posicion 50 el pass
  mensaje = "Datos Guardados";
  paginaconf();
}

//---------------------GUARDAR CONFIGURACION MAIL-------------------------
void guardar_conf_mail() {
  String auxiliar;
  char user[16];
  auxiliar=server.arg("mail");
  auxiliar.toCharArray(user,16);
  Serial.print("USER: ");
  Serial.println(auxiliar);//Recibimos los valores que envia por GET el formulario web
  grabarEEPROM(AddrUSER,user,16); // se graba en posicion 100 de la memoria
  mensaje = "Datos Guardados";
  paginaconf();
}
//---------------------PAGINA CONF-------------------------
void paginaconf(){
    server.send(200, "text/html", pagina + mensaje + paginafin);
    mensaje=" ";
  }

//------------------------SETUP WIFI-----------------------------
void setup_wifi() 
{
// Conexión WIFI
  WiFi.mode(WIFI_STA); //para que no inicie el SoftAP en el modo normal
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED and contconexion <50) { //Cuenta hasta 50 si no se puede conectar lo cancela
    ++contconexion;
    delay(250);
    Serial.print(".");
  }
  if (contconexion <50) {   
      Serial.println("");
      Serial.println("WiFi conectado");
      Serial.println(WiFi.localIP());
  }
  else { 
      Serial.println("");
      Serial.println("Error de conexion");
  }
}
///////////////////////FUNCIONES//////////////////////////////////
float conversion_Cuentas_Presion(float cuentas)
{
 float dato=cuentas;
 return dato;
}
//////////////////////////////////////////////////////////////////
float ADC_muestraPromediada(const unsigned int muestras)
{
  uint32_t adc_reading = 0;
  //Multisampling
  for (int i = 0; i < muestras; i++)
    adc_reading = adc_reading+analogRead(Adc1Rampa);

//    adc_reading /= NO_OF_SAMPLES;
//    //Convert adc_reading to voltage in mV
//    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
//    //printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
//    printf("%d\n", voltage)
//    //vTaskDelay(pdMS_TO_TICKS(1000));
  return adc_reading / muestras;
}
////////////////////////////////////////////////////////////////////
void mostrar_tabla_calibracion(const elemento_tabla_calibracion * tabla_calibracion)
{
    sDisplay datosDisplay;
    char cadena[16];
    memset(datosDisplay.fila1,0,16);
    memset(datosDisplay.fila2,0,16);
    strcpy(datosDisplay.fila1,"tabla calibracion");
    strcpy(datosDisplay.fila2,"..........");
    xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
    vTaskDelay(3000);
    for(int i=0; i<CANT_ELEMENTOS_TABLA_CALIBRACION; i++)
    {
          memset(datosDisplay.fila1,0,16);
          memset(datosDisplay.fila2,0,16);
          sprintf(cadena,"%1.0f",tabla_calibracion[i].presion_mmHg);
          strcpy(datosDisplay.fila1,cadena);
          sprintf(cadena,"%1.0f",tabla_calibracion[i].cuenta_ADC);
          strcpy(datosDisplay.fila2,cadena);
          xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
          vTaskDelay(3000);
    }
}
//////////////////////////////////////////////////////////////////////
MAP_DATATYPE map(MAP_DATATYPE x, MAP_DATATYPE in_min, MAP_DATATYPE in_max, MAP_DATATYPE out_min, MAP_DATATYPE out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
///////////////////////////////////////////////////////////////////////
//fn que convierte una cuenta de ADC a presion en mmHg
MAP_DATATYPE cuentaADC2presion(const elemento_tabla_calibracion * tabla_calibracion, float cuenta_ADC)
{
  int i;
  float cotaInferior, cotaSuperior;
  //Bloqueo la tabla de calibracion y también sirve para corroborar que ya haya habido una calibracion previa
  //xSemaphoreTake( xSem_tablaCalibracion, portMAX_DELAY);
  cotaInferior = tabla_calibracion[0].cuenta_ADC;
  cotaSuperior = tabla_calibracion[CANT_ELEMENTOS_TABLA_CALIBRACION-1].cuenta_ADC;

  if(cuenta_ADC < cotaInferior || cuenta_ADC > cotaSuperior)
  {
    //ESP_LOGE(__func__, "La cuenta de ADC a convertir %.2f está fuera de los rangos de la tabla de calibracion [%.2f:%.2f]\n", cuenta_ADC, tabla_calibracion[0].cuenta_ADC, tabla_calibracion[CANT_ELEMENTOS_TABLA_CALIBRACION-1].cuenta_ADC);
    //xSemaphoreGive(xSem_tablaCalibracion);
    return -1;
  }

  for(i = 0; i < CANT_ELEMENTOS_TABLA_CALIBRACION-1; i++)
  {
    cotaInferior = tabla_calibracion[i].cuenta_ADC;
    cotaSuperior = tabla_calibracion[i+1].cuenta_ADC;
    if(cuenta_ADC >= cotaInferior && cuenta_ADC <= cotaSuperior)
    {
      //xSemaphoreGive(xSem_tablaCalibracion);
      return map(cuenta_ADC, cotaInferior, cotaSuperior, tabla_calibracion[i].presion_mmHg, tabla_calibracion[i+1].presion_mmHg);
    }
  }

  //Nunca debería llegar nunca a este punto
  //ESP_LOGE(__func__, "El valor a convertir %.2f está fuera de los rangos de la tabla de calibracion [%.2f:%.2f]\n", cuenta_ADC, tabla_calibracion[0].cuenta_ADC, tabla_calibracion[CANT_ELEMENTOS_TABLA_CALIBRACION-1].cuenta_ADC);
  //xSemaphoreGive(xSem_tablaCalibracion);
  return -2;
}

String leerEEPROM(int addr,int cant) 
{ // se pasa el entero con la direccion
   byte lectura;
   String strlectura; // aca se acumulan los caracteres
   for (int i = addr; i < addr+cant; i++) {
      lectura = EEPROM.read(i);
      //if (lectura != 255) { // si llego al final (255)
        strlectura += (char)lectura;
      //}
   }
   return strlectura; // devuelve el dato leido
}
void grabarEEPROM (int addr, char const * cadena,int tamano) 
{
  char inchar[50]; // largo predefinido
  //a0.toCharArray(inchar, tamano+1);  // le agrega un caracter mas al tamaño predefinido
  for (int i = 0; i < tamano; i++) {
    EEPROM.write(addr+i, cadena[i]);  //graba en eeprom
    Serial.print(cadena[i]);
  }
  Serial.println(" ");
  //for (int i = tamano; i < 50; i++) {
    //EEPROM.write(addr+i, 255);  // graba el 255 para indicar fin de grabacion
  //}
  EEPROM.commit();
}  
void quitardatosEEPROM(void)
{
     String lectura;
     char cadena[16];
     char cad2[2];
     int  i;
     for(i= 0; i < CANTMED; i++)
     {    // subo una posicion cada mediicon grabada
          lectura=leerEEPROM((AddrMed+32+32*i),16);
          lectura.toCharArray(cadena,16);
          grabarEEPROM (AddrMed+32*i, cadena,16);
          lectura=leerEEPROM((100+48+32*i),16);
          lectura.toCharArray(cadena,16);
          grabarEEPROM (AddrMed+16+32*i, cadena,16);
          
     }
     i=4;
     sprintf(cad2,"%d",i); //resto uno al valor de cantidad datos grabados
     grabarEEPROM (AddrDatos, cad2,2);
}
