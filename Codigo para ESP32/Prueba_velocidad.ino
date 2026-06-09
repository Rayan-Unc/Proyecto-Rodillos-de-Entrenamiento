// Agregamos las librerias que son necesarias
#include  "BluetoothSerial.h"
BluetoothSerial SerialBT;
String  Device_name = "Rodillo_Entrenamiento";

#include "esp_sleep.h"
// Ahora creamos variables para los pines que vamos a utilizar
int sen_hall = 16;
int bot_in  = 4;
int bot_fin = 17;
int bot_bt  = 5;
int bot_env = 18;
int led_g = 32;
int led_r = 33;
int led_b = 25;


const int radio = 40;  // radio del rodillo que utilizamos
const int lecturas_n  = 5;
int indice  = 0;//este valor se va a modificar por lo que lo coloco como int
const unsigned  long  t_antire_hall = 15; // tiempos de anti rebote para el boton y sensor
const unsigned  long  t_antire_bot  = 500;
// Variables con coma que vamos a necesitar
float velocidad = 0; 
float circunf = 0;
float velocidad_max = 0;
float lecturas[lecturas_n]  = {0,0,0,0,0};
float velocidad_suav  = 0;
// Variables volatiles es decir que estan en la memoria ram
// Unsigned long son valores sin signo y maslargos para poder guardar datos muy grandes 
volatile  unsigned  long  t_f_h = 0;  
volatile  unsigned  long  t_f_bi  = 0;
volatile  unsigned  long  t_f_bf  = 0;
volatile  unsigned  long  t_f_bb  = 0;
volatile  unsigned  long  t_f_en  = 0;
volatile  unsigned  long  dif_tiempo = 0;
// Banderas que necesitamos en la memoria RAM
volatile  bool  hall  = false;
volatile  bool  encen = false;
volatile  bool  fin = false;
volatile  bool  bt  = false;
volatile  bool  bt_enc  = false;
volatile  bool  bt_apagar  = false;
volatile  bool  envio = false;
// Banderas necesarias para el loop
bool  frenado = false;
bool  bt_es_en  = false;
// Valores sin signo y largos para guardar datos para los calculos
unsigned  long  copia_dif_tiempo =  0;
unsigned  long  contador  = 0;
unsigned  long  tiempo_inicio = 0;
unsigned  long  tiempo_final = 0;
unsigned  long  dif_total = 0;
unsigned  long  tiempo_frenado = 0;
unsigned  long  tiempo_envio  = 0;
// Strings para poder guardar los valores que calculamos con su signo
String  resultado_tiempo = " "; 
String  resultado_dist  = " ";
String  resultado_vel = " ";
String  resultado_vel_max = " ";

// Podemos ver las diferentes interrupciones guardadas en la memoria RAM para mayor velocidad
void IRAM_ATTR ISR_v(){
  unsigned  long  t_i_h = millis();
  if ((t_i_h - t_f_h) > t_antire_hall) {
    if(t_f_h  !=  0){
      hall = true; 
      dif_tiempo  = t_i_h - t_f_h;
    }
    t_f_h = t_i_h; 
  }
}

void  IRAM_ATTR ISR_in(){
  unsigned  long  t_i_bi =  millis();
  if ((t_i_bi- t_f_bi)> t_antire_bot) {
    t_f_bi  = t_i_bi;
    encen = true;
  }
}

void  IRAM_ATTR ISR_fin(){
    unsigned  long  t_i_bf =  millis();
    if ((t_i_bf - t_f_bf)> t_antire_bot) {
    t_f_bf  = t_i_bf;
    fin = true;
  }
}

void  IRAM_ATTR ISR_bt(){
    unsigned  long  t_i_bb =  millis();
    if ((t_i_bb - t_f_bb)> t_antire_bot) {
      t_f_bb  = t_i_bb;
      if(bt_enc){
        bt_apagar = true;
      }
      else {
        bt  = true;
    }
  }
}

void  IRAM_ATTR ISR_en(){
  unsigned  long  t_i_en =  millis();
  if ((t_i_en - t_f_en)> t_antire_bot) {
    t_f_en  = t_i_en;
    envio = true;
  }
}

//Programas que sirven para el calculo de los diferentes valores que queremos
void Calcu_duracion(){
  unsigned long diferencia = tiempo_final - tiempo_inicio;
  if(diferencia < 60000){  // Menos de un minuto
    float seg = diferencia / 1000.0;
    resultado_tiempo = String(seg) + " Seg";
  }
  else  if(diferencia < 3600000){ //Entre 1 minuto y 1 hora
    unsigned long minutos = diferencia / 60000; // División entera 
    unsigned long resto_ms = diferencia % 60000; // Lo que sobra
    float segundos = resto_ms / 1000.0; // Pasamos el resto a segundos
    resultado_tiempo = String(minutos) + " Min y " + String(segundos) + " Seg";
  }
  else{ // Más de una hora
    unsigned long horas = diferencia / 3600000;
    unsigned long resto_ms = diferencia % 3600000;
    unsigned long minutos = resto_ms / 60000;
    resultado_tiempo = String(horas) + " Hrs y " + String(minutos) + " Min";
  }
}

void  Calcu_distancia(){
  float dist  = contador * circunf; 
  if(dist < 1000){ //menos de 1 metro
    float distancia = dist/10.0;
    resultado_dist  = String(distancia) + " Centimetros";
  }
  else  if(dist < 1000000){ //menos de 1 kilometro
    float distancia = dist/1000.0;
    resultado_dist  = String(distancia) + " Metros";
  }
  else{ // mayor a un kilometro
    float distancia = dist/1000000.0;
    resultado_dist  = String(distancia) + " Kilometros";
  }
}

void  Calcu_velocidad(){
  if(contador > 0 &&  dif_total > 0){
    float dist  = (contador  * circunf)/1000000.0; //distancia en km
    float tiempo  = dif_total/3600000.0;//tiempo en horas
    float velo  = dist/tiempo;
    resultado_vel = String(velo)  + " Km/h";
  }
  else{
    resultado_vel = "0.00 Km/h";
  }
  resultado_vel_max = String(velocidad_max) + " Km/h";
}

void setup() {
  // Definimos el modo en el que estan nuestros pines.
  pinMode(sen_hall, INPUT_PULLUP);
  pinMode(bot_in, INPUT_PULLUP);
  pinMode(bot_fin,  INPUT_PULLUP);
  pinMode(bot_bt, INPUT_PULLUP);
  pinMode(bot_env, INPUT_PULLUP);
  pinMode(led_r,  OUTPUT);
  pinMode(led_g,  OUTPUT); 
  pinMode(led_b,  OUTPUT);
  // Configuramos los pines de interrupcion
  attachInterrupt(digitalPinToInterrupt(bot_in), ISR_in, FALLING);
  attachInterrupt(digitalPinToInterrupt(bot_fin), ISR_fin, FALLING);
  attachInterrupt(digitalPinToInterrupt(bot_bt), ISR_bt, FALLING);
  attachInterrupt(digitalPinToInterrupt(bot_env),  ISR_en, FALLING);
  // Que pin se encargara de "despertar" nuestro micro
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 0);

  // Serial.begin(9600); // para realizar pruebas en computadora
  // Calculamos la circunferencia respecto a nuestro rodillo
  circunf = 2*PI*radio;
  // establecemos el valor inicial de los Leds y estado
  digitalWrite(led_r, HIGH);
  digitalWrite(led_g, LOW);
  digitalWrite(led_b, LOW);
  
  frenado  = true;
}

void loop() {

  if(hall){
    hall = false;
    noInterrupts();
    copia_dif_tiempo  = dif_tiempo;
    interrupts();
    if(copia_dif_tiempo > 0){
      if(copia_dif_tiempo < 2000){  //Aca nos aseguramos que la informacion que nos llega es del entrenamiento y no de que el deportista esta detenido.
        dif_total = dif_total + copia_dif_tiempo;
        contador++;
        velocidad = (circunf/copia_dif_tiempo)*3.6;
        lecturas[indice] = velocidad;
        indice++;
        
        if(indice >=  lecturas_n){
          indice  = 0;
        }
        float suma  = 0;
        
        for(int i = 0;  i < lecturas_n; i++){
          suma  = suma  + lecturas[i];
        }
        velocidad_suav  = suma/lecturas_n;
        
        if(velocidad_suav  > velocidad_max){
          velocidad_max = velocidad_suav;
        }
        
      }
      // por si queremos chequear el valor de velocidad que nos llega en la computadora
      //Serial.print(velocidad_suav);
      //Serial.println(" Km/h");
    }
  }
  //Para enviar nuestra velocidad instantanea por bluetooth

  if (bt_enc && SerialBT.hasClient() && frenado == false) { // nos aseguramos de que no estamos frenados, conectados a bluetooth y alguien esta conectado
    if (millis() - tiempo_envio >= 1000) { 
      tiempo_envio = millis(); 
      SerialBT.print(velocidad_suav); 
      SerialBT.println(" Km/h");
    }
  }

  // Esta funcion es para poder utilizar la aplicacion y que el deportista no tenga que tocar los botones fisicos
  if(bt_enc &&  SerialBT.available()){
    char  comando = SerialBT.read();
    switch(comando){
      case  'A':
      encen = true;
      break;

      case  'B':
      fin = true;
      break;

      case  'C':
      envio = true;
      break;

      default:
      break;
    }
  }
  // Para salir del modo frenado y comenzar a medir la velocidad
  if(encen){
      tiempo_inicio = millis();
      attachInterrupt(digitalPinToInterrupt(sen_hall), ISR_v, FALLING);
      digitalWrite(led_g, HIGH);
      digitalWrite(led_r, LOW);
      contador  = 0;
      dif_total = 0;
      copia_dif_tiempo  = 0;
      velocidad_max = 0;
      t_f_h = 0;
      velocidad = 0;
      velocidad_suav  = 0;
      indice  = 0;
      for(int i = 0;  i < lecturas_n; i++){
        lecturas[i] = 0;
      }
      encen = false;
      frenado = false;
      tiempo_frenado  = millis();
  }
  // Para poder volver a estado frenado, aparte de calcular las variables.
  if(fin){
    if(tiempo_inicio > 0){
      detachInterrupt(digitalPinToInterrupt(sen_hall));
      tiempo_final  = millis();
      digitalWrite(led_r, HIGH);
      digitalWrite(led_g, LOW);
      Calcu_duracion();
      Calcu_distancia();
      Calcu_velocidad();
      tiempo_inicio = 0;
      tiempo_final = 0;
      contador  = 0;
      dif_total = 0;
      velocidad_max = 0;
    }
    tiempo_frenado = millis();
    velocidad_suav  = 0;
    velocidad = 0;
    for (int i = 0; i < lecturas_n; i++) {
      lecturas[i] = 0;}
    frenado = true;
    fin = false;
    encen = false;
    bt_apagar = false;
    bt  = false;
    envio = false;
  }
  // Ahora enviaremos toda la informacion de la sesion de entrenamiento al celular.
  if(envio){
    if(bt_enc &&  SerialBT.hasClient()  && frenado){
      SerialBT.print("El entrenamiento duro: ");
      SerialBT.print(resultado_tiempo);
      SerialBT.print(",");
      SerialBT.print("Se recorrieron: ");
      SerialBT.print(resultado_dist);
      SerialBT.print(",");
      SerialBT.print("La velocidad media fue: ");
      SerialBT.print(resultado_vel);
      SerialBT.print(",");
      SerialBT.print("La velocidad maxima fue: ");
      SerialBT.println(resultado_vel_max);
    }
    envio = false;
    tiempo_frenado  = millis();
  }
  // Para encender modo bluetooth o apagarlo.
  if(bt && frenado && !bt_apagar){
    if(!bt_enc){
      SerialBT.begin(Device_name);
      bt_enc = true;
      digitalWrite(led_b, HIGH);
    }
    bt  = false;
    tiempo_frenado  = millis();
  }

  if(bt_enc &&  bt_apagar &&  frenado){
    SerialBT.end();
    digitalWrite(led_b, LOW);
    bt  = false;
    bt_apagar  = false;
    bt_enc  = false;
    tiempo_frenado  = millis();
  }
  // si pasan 5 minutos sin actividad, el micro entra en modo sleep
  if(frenado){
    if((millis() - tiempo_frenado) > 300000){ 
      digitalWrite(led_r, LOW);
      digitalWrite(led_g, LOW);
      digitalWrite(led_b, LOW);
      if(bt_enc){
        SerialBT.end();
        bt_enc  = false;
        bt_es_en  = true;
      }
      esp_light_sleep_start();
      digitalWrite(led_r, HIGH);
      if(bt_es_en){
        bt_enc  = true;
        bt_es_en  = false;
        SerialBT.begin(Device_name);
        digitalWrite(led_b, HIGH);
      }
      tiempo_frenado = millis();
      hall  = false;
      encen = false;
      fin = false;
      bt  = false;
      bt_apagar = false;
      envio = false;
    }
  }
}