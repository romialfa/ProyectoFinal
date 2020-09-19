#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

#include <LiquidCrystal_I2C.h>

const int Adc1Rampa = 35;
const int Adc0Pulso = 34;
const int BombaPin = 13;
const int ValvulaPin = 12;
const int LedEsp= 2;
const int LedPlaca = 14;
const int OnOffBotton= 2;

float VRampa,VPulsos,LimiteRampa=0.7,LimitePulsos=0.68,AnteriorPulsos,AnteriorRampa;// los pulsos tiene un piso de continua de 586mv
int ValorRampa=0,ValorPulsos=0;
float Pulsaciones,PresionSistolica,PresionDistolica,FactorPresion=18.46;// multiplico po 18.46 para tener con 0.7v una presion de 12 mmhg
int FlagPulsos,ContMedicion,ContPulsos;

unsigned long TiempoAnt;

// define two tasks for Blink & AnalogRead
void TaskDesinflado( void *pvParameters );
void TaskMain( void *pvParameters );
void TaskInflado( void *pvParameters );

void FuncionPantalla(void);

struct sDisplay{
  char fila1[16];
  char fila2[16];
}

QueueHandle_t ColaInflado,ColaDesinflado;

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  pinMode(LedPin, OUTPUT);
  pinMode(MotorPin, OUTPUT);
  pinMode(ValvulaPin, OUTPUT);
  pinMode(OnOffBotton,INPUT);
  
  ColaInflado = xQueueCreate(1, sizeof(int));
  ColaDesinflado = xQueueCreate(1, sizeof(int));
  ColaDisplay = xQueueCreate(1, sizeof(sDiplay));
  
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(app_Medicion,"appMedicion",1024,NULL,2,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Wait,"appWait",1024,NULL,1,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Configuracion,"appConfiguracion",1024,NULL,2,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Calibracion,"appCalibracion",1024,NULL,2,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Teclado,"appTeclado",1024,NULL,2,NULL,ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(app_Display,"appDisplay",1024,NULL,2,NULL,ARDUINO_RUNNING_CORE);
}

void loop()
{
  // Empty. Things are done in Tasks.
}


/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/


void app_Display(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  LiquidCrystal_I2C lcd(0x27, 16, 2);
  lcd.init();
  lcd.backlight();
  sDiplay datosDisplay;
  for (;;)
    {
        xQueueReceive(ColaDisplay, &datosDisplay, portMAX_DELAY);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(datosDisplay.fila1);
        lcd.setCursor(0, 1);
        lcd.print(datosDisplay.fila2);
        vTaskDelay(500); 
        
   }
}


void app_Teclado(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int pushbotton;
  char estado="esperar";
  sDiplay datosDisplay;
  for (;;)
  {
    if(digitalRead(OnOffBotton)==HIGH&&estado=="esperar")
    {
        vTaskDelay(50);
        while(digitalRead(OnOffBotton)==HIGH&&estado=="esperar")
        {
             pushbotton++;
             vTaskDelay(500);             
        }
        if(pushbotton<=4)
          {
            //se presiono boton menos de 2 segundos
            estado="medir";
            xQueueSend(ColaEstado,&estado, portMAX_DELAY);
          }
        if(pushbotton>=6&&pushbotton<=8)
          {
            //se presiono boton mas de 3 segundos
            estado="calibrar";
            xQueueSend(ColaEstado,&estado, portMAX_DELAY);
          }
        if(pushbotton>=12)
          {
            //se presiono boton mas de 6 segundos
            estado="configurar";
            xQueueSend(ColaEstado,&estado, portMAX_DELAY);
          }
        pushbotton=0;
    }
    if(digitalRead(OnOffBotton)==HIGH&&estado=="medir")
    {
        vTaskDelay(50);
        if(digitalRead(OnOffBotton)==HIGH&&estado=="medir")
        {
            //se cancela medicion  
            estado="cancelarmed";
            xQueueSend(ColaEstado,&estado, portMAX_DELAY);
            estado="esperar";
        }
    vTaskDelay(100);  // one tick delay (15ms) in between reads for stability
  }
}


void app_Wait(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int cont =0;
  char estado="esperar";
  sDiplay datosDisplay;
  for (;;)
  {
    xQueueReceive(ColaEstado,&estado, 100);
    if(estado=="esperar")
    {
        switch(cont){
          case 1:
              datosDisplay.fila1="Para Medicion....";
              datosDisplay.fila2="Push boton 1s";
              xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
              break;
           case 4: 
              datosDisplay.fila1="Para Calibracion....";
              datosDisplay.fila2="Push boton 3s";
              xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
              break;
           caee 8:
              datosDisplay.fila1="Para Configuracion....";
              datosDisplay.fila2="Push boton 5s";
              xQueueSend(ColaDisplay, &datosDisplay, portMAX_DELAY);
              break;
           case 12:
              cont=0;
              break;
          } 
        cont++;    
    }
    if(estado=="medir")
    {
            datosDisplay.fila1="...MEDIENDO...";
            datosDisplay.fila2="Boton cancelar";
            xQueueSend(ColaDisplay,&datosDisplay, portMAX_DELAY);
            xQueueSend(ColaMedicion,1, portMAX_DELAY);
            estado="midiendo";
    }
    if(estado=="calibrar")
    {
            datosDisplay.fila1="...CALIBR...";
            datosDisplay.fila2="Boton cancelar";
            xQueueSend(ColaDisplay,&datosDisplay, portMAX_DELAY);
            xQueueSend(ColaCalibrar,1, portMAX_DELAY);
    }
    if(estado=="configurar")
    {
            datosDisplay.fila1="...CONFIG...";
            datosDisplay.fila2="Boton cancelar";
            xQueueSend(ColaDisplay,&datosDisplay, portMAX_DELAY);
            xQueueSend(ColaConfigurar,1, portMAX_DELAY);
    }
    vTaskDelay(500);  // one tick delay (15ms) in between reads for stability
  }
}

            
void app_Medicion(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int tam_array=2000,maximoValor,i,maximoIndex; 
  int flag,cont=0;
  unsigned long adcRampa, adcPulsos,adcValor;
  unsigned long datoRampa[tam_array];
  unsigned long datoPulsos[tam_array];
  for (;;)
  {
        xQueueReceive(ColaMedicion,&flag, portMAX_DELAY);
        if(flag==1)
        {
              digitalWrite(BombaPin, HIGH);
              // Inflado hasta limite superior rampa
              do{
                    adcValor = analogRead(Adc1Rampa);
                    //VRampa=(float)(((float)ValorRampa*3.3)/4096);
                    vTaskDelay(40);
              }while(adcValor<=LimiteSupRampa) ;
              digitalWrite(BombaPin, LOW);
              vTaskDelay(500);
              cont=0;
              adcRampa=0;
              adcPulsos=0;
              // desinflado natural y guarda valores hasta limite inferior rampa
              for(i=0;i<tam_array;i++)
              {
                  datosRampa[i]=0;
                  datosPulsos[i]=0;
              }
              do{
                    //promedia valor de adc por numero de muestras
                    for(i=0;i<muestras;i++)
                    {
                        adcRampa=adcRampa+analogRead(Adc1Rampa);
                        adcPulsos=adcPulsos+analogRead(Adc1Pulsos);
                        //VRampa=(float)(((float)ValorRampa*3.3)/4096);
                    }
                    datosRampa[cont]=adcRampa/muestras;
                    datosPulsos[cont]=adcPulsos/muestras;    
                    vTaskDelay(40);
                    cont++;
                    adcValor=analogRead(Adc1Rampa);
              }while(VRampa>=LimiteInfRampa) ;
              //procesamiento de los valores
              //busco maximo dentro del array datosPulsos
              for(i=0;i<tam_array;i++)
              {
                  if(datosPulsos[i]>maximoValor)
                  {
                      maximoValor=datosPulsos[i];
                      maximoIndex=i;
                  }
              }
              //multiplico para valor de ksis y busco valor 
              sisValor=ksis*maximoValor;
              for(i=0;i<maximoIndex;i++)
              {
                  if(datosPulsos[i]>sisValor)
                  {
                      sisIndex=i;
                  }
              }
              //multiplico para valor de kdis y busco valor, recorro array al revez 
              disValor=kdis*maximoValor;
              for(i=tam_array;i>maximoIndex;i--)
              {
                  if(datosPulsos[i]>disValor)
                  {
                      disIndex=i;
                  }
              }
              //con adcRampa[sisIndex] y adcRampa[disIndex] paso a mmgh
              xQueueSend(ColaDesinflado, &i, portMAX_DELAY);
        }
        vTaskDelay(1000);  // one tick delay (15ms) in between reads for stability
  }
}





void app_Configuracion(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int i=0;
  for (;;) // A Task shall never return or exit.
  {
        xQueueReceive(ColaDesinflado, &i, portMAX_DELAY);
        AnteriorRampa=(LimiteRampa-0.01);
        AnteriorPulsos=(LimitePulsos-0.01);
        ContMedicion=0;
        ContPulsos=0;
        while(i==1)
        {
              if(ContMedicion<5)
              {
                    ValorPulsos = analogRead(Adc0Pin);
                    VPulsos=(float)(((float)ValorPulsos*3.3)/4096);
                    if((AnteriorPulsos<LimitePulsos)&&(VPulsos>LimitePulsos))
                    {
                          ContMedicion++;
                    }
                    AnteriorPulsos=VPulsos;
              }      
              if(ContMedicion==5)
              {
                    ValorRampa = analogRead(Adc1Pin);
                    VRampa=(float)(((float)ValorRampa*3.3)/4096);
                    PresionSistolica=VRampa*FactorPresion;//convierte el valor de presion
                    Serial.println("Presion Sistolica: ");
                    Serial.println(PresionSistolica);
                    ContMedicion++;
                    AnteriorPulsos=(LimitePulsos-0.01);
              }
              if(ContMedicion==6)
              {
                    ValorPulsos = analogRead(Adc0Pin);
                    VPulsos=(float)(((float)ValorPulsos*3.3)/4096);
                    if((AnteriorPulsos<LimitePulsos)&&(VPulsos>LimitePulsos))
                    {
                          ContPulsos++;
                          if(ContPulsos==1)TiempoAnt=millis();
                          if(ContPulsos==6)
                          {
                                Pulsaciones=(millis()-TiempoAnt)/(5*1000);
                                Serial.println("Pulsaciones: ");
                                Serial.println(Pulsaciones);
                                ContMedicion++;
                                i=0;
                          }
                    }   
              }      
              vTaskDelay(20);
        }
        //FuncionPantalla(); 
        vTaskDelay(500); 
  }
}

void app_Calibracion(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  int i;
  for (;;)
  {
        xQueueReceive(ColaInflado, &i, portMAX_DELAY);
        if(i==1)
        {
              digitalWrite(MotorPin, HIGH);
              do{
                    ValorRampa = analogRead(Adc1Pin);
                    VRampa=(float)(((float)ValorRampa*3.3)/4096);
                    vTaskDelay(40);
              }while(VRampa<=LimiteRampa);
              digitalWrite(MotorPin, LOW);
              xQueueSend(ColaDesinflado, &i, portMAX_DELAY);
        }
        i=0;
        vTaskDelay(1000);  // one tick delay (15ms) in between reads for stability
  }
}


void FuncionPantalla()
{
  
}
