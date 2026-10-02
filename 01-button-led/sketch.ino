const int ledpin=2;
const int buttonpin=4;
bool ledon=false;
unsigned long presscount=0;
 bool pending;
volatile bool buttonflag=false;
const unsigned long timedebounce=20;
volatile unsigned long lastisrtime=0;
void IRAM_ATTR onButton(){
  unsigned long now =millis();
  if(now-lastisrtime>timedebounce){
    buttonflag=true;
    lastisrtime=now;
  }
  
}
void setup(){
  Serial.begin(115200);
  pinMode(ledpin,OUTPUT);
  pinMode(buttonpin,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonpin),onButton,FALLING);
}
void loop(){
  pending=false;
  if(buttonflag){
    noInterrupts();
    pending=buttonflag;
    buttonflag=false;
    interrupts();
  }
  if(pending){
    delay(10);
  if(digitalRead(buttonpin) == LOW){
  presscount++;
  ledon= !ledon;
  digitalWrite(ledpin,ledon);
  Serial.print("presses:");
  Serial.println(presscount);
  
 }}

}