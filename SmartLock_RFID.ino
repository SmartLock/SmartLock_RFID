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
    digitalWrite(statusLed, HIGH);   // Led sinaliza que usuário irá resetar a memória
    Serial.println("!!! Botao Reset de Memoria Pressionado !!!");
    Serial.println("Voce tem 5 segundos para cancelar");
    Serial.println("Esta acao ira remover todos os registros e nao pode ser desfeita");
    delay(5000);    // Dá ao usuário tempo suficiente para cancelar a operação (5s)
    if (digitalRead(wipeB) == LOW) {  // Se o botão continua pressionado, limpar EEPROM
      Serial.println("!!! Limpando memoria EEPROM !!!");
      for (int x=0; x<1024; x=x+1){ //Faz um loop em todos os endereços EEPROM
        if (EEPROM.read(x) == 0){ // Se EEPROM já estiver zerada, ok
        } 
        else{
          EEPROM.write(x, 0); // Se não, zerar EEPROM (leva 3.3ms)
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
  if (EEPROM.read(1) != 1) { // Verifica se existe um cartão mestre definido no começo do programa (o espaço de memória 1 serve para indicar se há ou não cartão mestre definido)
    Serial.println("Cartao MESTRE nao definido");
    Serial.println("Cadastre um cartao MESTRE");
    do {
      successRead = getID(); // Altera a variável successRead para 1 quando a leitura for válida, caso contrário assume 0
    }
    while (!successRead); // O programa não avança enquanto não realiza a leitura corretamente
    for ( int j = 0; j < 4; j++ ) { // Faz o Loop 4x para escrever na memória PICCs de 4 bytes
      EEPROM.write( 2 +j, readCard[j] ); // Escreve na memória o cartão escaneado a partir do endereço 2
    }
    EEPROM.write(1,1); // Escreve no espaço de memória 1 que houve definição do cartão mestre
    Serial.println("Cartao Mestre Definido");
  }
  Serial.println("##### Projeto SMARTLOCK Versao 1.0 #####"); //For debug purposes
  Serial.println("UID do Cartao Mestre:");
  for ( int i = 0; i < 4; i++ ) {     // Lê o cartão mestre da memória
    masterCard[i] = EEPROM.read(2+i); // A variável masterCard recebe o UID do cartão vindo da memória
    Serial.print(masterCard[i], HEX); // Mostra na tela o UID
  }
  Serial.println("");
  Serial.println("Esperando cartoess para ser escaneado: ");
  
  piscaStatusLed(1, tempoStatusLed*5, tempoStatusLed);
}

///////////////////////////////////////// Loop Principal ///////////////////////////////////
void loop () {
  do {
    successRead = getID(); // Altera a variável successRead para 1 quando a leitura for válida, caso contrário assume 0
    if (programMode) {
    }
    else {
      normalModeOn(); // Garante o estado normal (Led e Relé desligados)
    }
  }
  while (!successRead); // O programa não avança enquanto não realiza a leitura corretamente (pela primeira vez, o programa nunca segue o primeiro If, pois o programMode é falso)
  if (programMode) {
    if ( isMaster(readCard) ) { // Verifica se o cartão escaneado é o cartão mestre, com o Modo de Programação Aberto
      Serial.println("Voce inseriu o cartao mestre"); 
      Serial.println("Saindo do Modo de Programacao...");
      Serial.println("-----------------------------");
      piscaStatusLed(1, tempoStatusLed*5, tempoStatusLed);
      programMode = false; // Sai do Modo de Programação
      return;
    }
    else {	
      if ( findID(readCard) ) { // Se dentro do Modo de Programação, a antena escanear um cartão já cadastrado....
        Serial.println("Eu conheco esse cartao, portanto irei BLOQUEA-LO");
        deleteID(readCard);
        Serial.println("-----------------------------");
        piscaStatusLed(2, tempoStatusLed, tempoStatusLed/2);
      }
      else {                    // Se dentro do Modo de Programação, a antena escanear um cartão já não cadastrado....
        Serial.println("Eu nao conheco esse cartao, portanto irei LIBERA-LO");
        writeID(readCard);
        Serial.println("-----------------------------");
        piscaStatusLed(4, tempoStatusLed, tempoStatusLed/2);
      }
    }
  }
  else {
    if ( isMaster(readCard) ) {  // Se o cartão escaneado combina com o cartão mestre, entra no Modo de Programação
      programMode = true;
      Serial.println("Ola Master - Voce entrou no Modo de Programacao");
      int count = EEPROM.read(0); // Conta e armazena quantos cartoes tem acesso a fechadura
      Serial.print("Eu tenho ");
      Serial.print(count);
      Serial.print(" cartoes com acesso a essa fechadura");
      Serial.println("");
      Serial.println("Insira um cartao para LIBERAR OU BLOQUEAR o acesso");
      Serial.println("-----------------------------");
      piscaStatusLed(1, tempoStatusLed*5, tempoStatusLed);
    }
    else {
      if ( findID(readCard) ) {        // Libera acesso ao usuario, caso o cartao esteja cadastrado
        Serial.println("Bem-Vindo!");
        openDoor(tempoAbertura);                // Abre a porta
      }
      else {				// Nega acesso ao usuario, caso o cartao não esteja cadastrado
        Serial.println("O cartao inserido NAO tem acesso!");
        failed(); // Sinaliza por meio do Led que não foi possível abrir a fechadura
      }
    }
  }
}

///////////////////////////////////////// Obtendo UID dos cartões /////////////////////////////////
int getID() {
  // Se preparando para ler cartões
  if ( ! mfrc522.PICC_IsNewCardPresent()) { // Se um cartão foi posicionado corretamente no RFID, continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) { // Se o programa conseguiu obter o serial do cartão, continue
    return 0;
  }
  
  // Assumindo que os cartões possuem 4 byte
    Serial.println("UID do cartao escaneado:");
  for (int i = 0; i < 4; i++) {  // 
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Para de ler
  return 1; // Retorna 1 para informar que o cartão foi lido corretamente
}

//////////////////////////////////////// Led - Estado Normal ///////////////////////////////////
void normalModeOn () {
  digitalWrite(statusLed, LOW); // Led Desligado
  digitalWrite(relay, LOW); // Relé Desligado
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

////////////////////// Verifica se o cartão lido é o cartão mestre   ///////////////////////////////////
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

///////////////////////////////////////// Abre a porta  ///////////////////////////////////
void openDoor( int setDelay ) {
  
  digitalWrite(relay, HIGH); // Abre a porta
  delay(setDelay); // O sinal de abertura permanece por mais algum tempo, para garantir que a porta foi aberta
  digitalWrite(relay, LOW); // Interrompe o sinal de abertura (necessário somente um pulso)
  digitalWrite(statusLed, LOW); // Desliga o Led
  
  piscaStatusLed(1,tempoStatusLed,tempoStatusLed/2); // Avisa que o processo funcionou atraves do led
  
  delay(tempoDelayCiclo);
  
}

///////////////////////////////////////// Acesso Negado  ///////////////////////////////////
void failed() {
  
  piscaStatusLed(3,tempoStatusLed,tempoStatusLed/2); // Avisa que o processo falhou atraves do led
 
  delay(tempoDelayCiclo);
  
}

///////////////////////////////////////// Funcao Pisca  ///////////////////////////////////

void piscaStatusLed(int numero, int tempoLigado, int tempoDesligado) {

  for (int i=0; i < numero; i++){
    digitalWrite(statusLed, HIGH); // Acende o Led
     delay(tempoLigado);
     digitalWrite(statusLed, LOW); // Apaga o Led
     delay(tempoDesligado);
  }
  
}
