#include <EEPROM.h>  // Biblioteca de memória NÃO-VOLÁTIL para ler/armazenar/deletar IDs
#include <SPI.h>      // Biblioteca do protocolo SPI (Interface Periférica Serial) para o controlador comunicar com dispositivos periféricos (no caso, o RFID-RC522)
#include <MFRC522.h>   // Biblioteca para o dispositivo RFID-RC522

#define statusLed 6 // Define o pino 6 como status do programa por meio de um Led
#define relay 8 // Define o pino 8 como acionamento do relé
#define wipeB 3 // Define o pino 3 como botão de reset

boolean match = false; // Inicializa a variável match (cartão de partida)
boolean programMode = false; // Inicializa a variável programMode (Modo de Programação do Cartão Master)

int successRead; // Declara a variável sucessRead, para informar se houve sucesso na leitura do cartão

byte storedCard[4];   // Declara a variável storedCard, armazena o cartão ID lido na memória EEPROM
byte readCard[4];     // Declara a variável readCard, que armazena  o cartão ID escaneado no módulo RFID-RC522
byte masterCard[4];   // Declara a variável byte masterCard, que armazena o cartão ID MASTER lido na memória EEPROM

#define SS_PIN 10 // Define pino 10 como SS (que liga no SDA do RFID-RC522)
#define RST_PIN 9 // Define pino 9 como RST (que liga no RST do RFID-RC522)

MFRC522 mfrc522(SS_PIN, RST_PIN); 

int tempoAbertura = 15; // Declara a variável tempoAbertura, tempo de abertura da porta
int tempoDelayCiclo = 2500; // Declara a variável tempoDelayCiclo, tempo de espera entre um comando e outro
int tempoStatusLed = 250; // Declara a variável tempoStatusLed, tempo no qual o Led fica aceso

///////////////////////////////////////// Setup ///////////////////////////////////

void setup() {
  
  // Configuração dos pinos do Arduíno
  pinMode(statusLed, OUTPUT); // Define o pino como saída
  pinMode(relay, OUTPUT); // Define o pino como saída
  
  digitalWrite(relay, LOW); // Certifica-se que o relé está desligado
  digitalWrite(statusLed, LOW); // Certifica-se que o Led está desligado
  
  // Configuração de Protocolo
  Serial.begin(9600);	 // Inicializa comunicação serial com o PC
  SPI.begin();           // Hardware RFID-RC522 usa protocolo SPI
  mfrc522.PCD_Init();    // Inicializa Hardware RFID-RC522
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); // Configura a antena para ganho máximo. Isso aumenta a distância de leitura

  pinMode(wipeB, INPUT_PULLUP);  // Configura WipeB para conectar a 5V internamente
  if (digitalRead(wipeB) == LOW) {     // Quando o botão é pressionado, o pino é aterrado
    digitalWrite(statusLed, HIGH);   // Led sinaliza que usuário irá resetar memória
    Serial.println("!!! Botao Reset de Memoria Pressionado !!!");
    Serial.println("Voce tem 5 segundos para cancelar");
    Serial.println("Esta acao ira remover todos os registros e nao pode ser desfeita");
    delay(5000);    // Dá ao usuário tempo suficiente para cancelar a operação
    if (digitalRead(wipeB) == LOW) {  // Se o botão continua pressionado, limpar EEPROM
      Serial.println("!!! Limpando memoria EEPROM !!!");
      for (int x=0; x<1024; x=x+1){ //Faz um loop em todos os endereços EEPROM
        if (EEPROM.read(x) == 0){ // Se EEPROM já estiver zerada, ok
        } 
        else{
          EEPROM.write(x, 0); // Se não, zerar EEPROM (leva 3,3ms)
        }
      }
      Serial.println("!!! Memoria Apagada !!!");
      
    piscaStatusLed(5, tempoStatusLed*5, tempoStatusLed);
    
    }
    else {
      Serial.println("!!! Cancelando.... !!!");
      
     piscaStatusLed(2, tempoStatusLed*5, tempoStatusLed);
     
    }
  }
  // Verifica se existe um cartão mestre definido, se não, deixa o usuário escolher o cartão
  // Também usado para redefinir cartão mestre
  if (EEPROM.read(1) != 1) { // Verifica se existe um cartão mestre definido
    Serial.println("Cartao MESTRE nao definido");
    Serial.println("Cadastre um cartao MESTRE");
    do {
      successRead = getID(); // sets successRead to 1 when we get read from reader otherwise 0
      digitalWrite(statusLed, HIGH); // Visualize Master Card need to be defined
      delay(200);
      digitalWrite(statusLed, LOW);
      delay(200);
    }
    while (!successRead); //the program will not go further while you not get a successful read
    for ( int j = 0; j < 4; j++ ) { // Loop 4 times
      EEPROM.write( 2 +j, readCard[j] ); // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1,1); //Write to EEPROM we defined Master Card.
    Serial.println("Master Card Defined");
  }
  Serial.println("##### RFID Door Acces Control v2.0.8 #####"); //For debug purposes
  Serial.println("Master Card's UID");
  for ( int i = 0; i < 4; i++ ) {     // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2+i); // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println("Waiting PICCs to be scanned :)");
  cycleLeds();    // Everything ready lets give user some feedback by cycling leds
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
  do {
    successRead = getID(); // sets successRead to 1 when we get read from reader otherwise 0
    if (programMode) {
    }
    else {
      normalModeOn(); // Normal mode, blue Power LED is on, all others are off
    }
  }
  while (!successRead); //the program will not go further while you not get a successful read
  if (programMode) {
    if ( isMaster(readCard) ) {  //If master card scanned again exit program mode
      Serial.println("This is Master Card"); 
      Serial.println("Exiting Program Mode");
      Serial.println("-----------------------------");
      piscaStatusLed(1, tempoStatusLed*5, tempoStatusLed);
      programMode = false;
      return;
    }
    else {	
      if ( findID(readCard) ) { //If scanned card is known delete it
        Serial.println("I know this PICC, so removing");
        deleteID(readCard);
        Serial.println("-----------------------------");
        piscaStatusLed(2, tempoStatusLed, tempoStatusLed/2);
      }
      else {                    // If scanned card is not known add it
        Serial.println("I do not know this PICC, adding...");
        writeID(readCard);
        Serial.println("-----------------------------");
      }
    }
  }
  else {
    if ( isMaster(readCard) ) {  // If scanned card's ID matches Master Card's ID enter program mode
      programMode = true;
      Serial.println("Hello Master - Entered Program Mode");
      int count = EEPROM.read(0); // Read the first Byte of EEPROM that
      Serial.print("I have ");    // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(" record(s) on EEPROM");
      Serial.println("");
      Serial.println("Scan a PICC to ADD or REMOVE");
      Serial.println("-----------------------------");
      piscaStatusLed(1, tempoStatusLed*5, tempoStatusLed);
    }
    else {
      if ( findID(readCard) ) {        // If not, see if the card is in the EEPROM 
        Serial.println("Welcome, You shall pass");
        openDoor(300);                // Open the door lock for 300 ms
      }
      else {				// If not, show that the ID was not valid
        Serial.println("You shall not pass");
        failed(); 
      }
    }
  }
}

///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
int getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println("Scanned PICC's UID:");
  for (int i = 0; i < 4; i++) {  // 
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
 
  for (int i=0; i < 4; i++){
    digitalWrite(statusLed, HIGH); // Turn on Status Led
    delay(tempoStatusLed);
    digitalWrite(statusLed, LOW); // Turn off Status Led
    delay(tempoStatusLed);
  }
  
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(statusLed, LOW); // Blue LED ON and ready to read card
  digitalWrite(relay, LOW); // Make sure Door is Locked
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( int number ) {
  int start = (number * 4 ) + 2; // Figure out starting position
  for ( int i = 0; i < 4; i++ ) { // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start+i); // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) { // Before we write to the EEPROM, check to see if we have seen this card before!
    int num = EEPROM.read(0); // Get the numer of used spaces, position 0 stores the number of ID cards
    int start = ( num * 4 ) + 6; // Figure out where the next slot starts
    num++; // Increment the counter by one
    EEPROM.write( 0, num ); // Write the new count to the counter
    for ( int j = 0; j < 4; j++ ) { // Loop 4 times
      EEPROM.write( start+j, a[j] ); // Write the array values to EEPROM in the right position
    }
    successWrite();
  }
  else {
    failedWrite();
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) { // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite(); // If not
  }
  else {
    int num = EEPROM.read(0); // Get the numer of used spaces, position 0 stores the number of ID cards
    int slot; // Figure out the slot number of the card
    int start;// = ( num * 4 ) + 6; // Figure out where the next slot starts
    int looping; // The number of times the loop repeats
    int j;
    int count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a ); //Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--; // Decrement the counter by one
    EEPROM.write( 0, num ); // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) { // Loop the card shift times
      EEPROM.write( start+j, EEPROM.read(start+4+j)); // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( int k = 0; k < 4; k++ ) { //Shifting loop
      EEPROM.write( start+j+k, 0);
    }
    successDelete();
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != NULL ) // Make sure there is something in the array first
    match = true; // Assume they match at first
  for ( int k = 0; k < 4; k++ ) { // Loop 4 times
    if ( a[k] != b[k] ) // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) { // Check to see if if match is still true
    return true; // Return true
  }
  else  {
    return false; // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
int findIDSLOT( byte find[] ) {
  int count = EEPROM.read(0); // Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) { // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if( checkTwo( find, storedCard ) ) { // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i; // The slot number of the card
      break; // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  int count = EEPROM.read(0); // Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) {  // Loop once for each EEPROM entry
    readID(i); // Read an ID from EEPROM, it is stored in storedCard[4]
    if( checkTwo( find, storedCard ) ) {  // Check to see if the storedCard read from EEPROM
      return true;
      break; // Stop looking we found it
    }
    else {  // If not, return false   
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 5 times to indicate a successful write to EEPROM
void successWrite() {
 
  piscaStatusLed(1, tempoStatusLed, tempoStatusLed/2);
  
  Serial.println("Succesfully added ID record to EEPROM");
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 10 times to indicate a failed write to EEPROM
void failedWrite() {
   
  for (int i=0; i < 10; i++){
    digitalWrite(statusLed, HIGH); // Turn on Status Led
    delay(tempoStatusLed);
    digitalWrite(statusLed, LOW); // Turn off Status Led
    delay(tempoStatusLed);
  }
 
  Serial.println("Failed! There is something wrong with ID or bad EEPROM");
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  Serial.println("Succesfully removed ID record from EEPROM");
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

///////////////////////////////////////// Unlock Door   ///////////////////////////////////
void openDoor( int setDelay ) {
  
  digitalWrite(relay, HIGH); // Unlock door!
  delay(tempoAbertura); // Hold door lock open for given seconds
  digitalWrite(relay, LOW); // Relock door
  digitalWrite(statusLed, LOW); // Turn off Status Led
  
  piscaStatusLed(1,tempoStatusLed,tempoStatusLed/2); //avisa que o processo funcionou atraves do led
  
  delay(tempoDelayCiclo);
  
}

///////////////////////////////////////// Failed Access  ///////////////////////////////////
void failed() {
  
  piscaStatusLed(3,tempoStatusLed,tempoStatusLed/2); //avisa que o processo falhou atraves do led
 
  delay(tempoDelayCiclo);
  
}

///////////////////////////////////////// Funcao Pisca  ///////////////////////////////////

void piscaStatusLed(int numero, int tempoLigado, int tempoDesligado) {

  for (int i=0; i < numero; i++){
    digitalWrite(statusLed, HIGH); // Turn on Status Led
     delay(tempoLigado);
     digitalWrite(statusLed, LOW); // Turn off Status Led
     delay(tempoDesligado);
  }
  
}
