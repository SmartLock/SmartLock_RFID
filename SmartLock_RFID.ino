#include <EEPROM.h>  // Biblioteca de memória NÃO-VOLÁTIL para ler/armazenar/deletar IDs
#include <SPI.h>      // Biblioteca do protocolo SPI (Interface Periférica Serial) para o controlador comunicar com dispositivos periféricos (no caso, o RFID-RC522)
#include <MFRC522.h>   // Biblioteca para o dispositivo RFID-RC522
#include <Servo.h> // Biblioteca do servo motor

#define statusLed 6 // Define o pino 6 como status do programa por meio de um Led
#define relay 8 // Define o pino 8 como acionamento do relé
#define wipeB 3 // Define o pino 3 como botão de reset

boolean match = false; // Inicializa a variável match (comparação) como falso
boolean programMode = false; // Inicializa a variável programMode (Modo de Programação do Cartão Master) como falso

Servo servol; // Define a variável do servo motor

int successRead; // Declara a variável sucessRead, para informar se houve sucesso na leitura do cartão

// Os cartões possuem 4 bytes
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
      
    piscaStatusLed(20, tempoStatusLed/10, tempoStatusLed/10);
    
    }
    else {
      Serial.println("!!! Cancelando.... !!!");
      
    piscaStatusLed(2, tempoStatusLed/2, tempoStatusLed/2);
     
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
  Serial.println("Esperando cartoes para serem escaneado...");
  
  piscaStatusLed(3, tempoStatusLed*2, tempoStatusLed*2);
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
        servoControl(); // Aciona o servo motor
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
  
  // Assumindo que os cartões possuem 4 bytes
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

//////////////////////////////////////// Lê o cartão UID da memória EEPROM //////////////////////////////
void readID( int number ) {
  int start = (number * 4 ) + 2; // Descobre qual posição está o cartão
  for ( int i = 0; i < 4; i++ ) { // Realiza o loop 4x para pegar os 4 byte do cartão
    storedCard[i] = EEPROM.read(start+i); // Armazena o valor lido na variável storedCard
  }
}

///////////////////////////////////////// Adiciona cartão UID na memória EEPROM //////////////////////////////////
void writeID( byte a[] ) {
    if ( !findID( a ) ) { // Verifica se já existe este cartão na memória
    int num = EEPROM.read(0); // Get the numer of used spaces, position 0 stores the number of ID cards
    int start = ( num * 4 ) + 6; // Para saber onde o próximo slot vazio começa
    num++; // Incrementa a quantidade de cartões
    EEPROM.write( 0, num ); // Escreve na posição 0 da memória EEPROM a quantidade de cartões atualizada
    for ( int j = 0; j < 4; j++ ) { // Realiza o loop 4x
      EEPROM.write( start+j, a[j] ); // Escreve o novo cartão na memória EEPROM, conforme passado para função por meio do array a
     }
      successWrite(); // Notificação de que o cartão foi adicionado com sucesso
     }
    else {
    failedWrite();
  }
}

///////////////////////////////////////// Deleta cartão UID da memória EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {

    int num = EEPROM.read(0); // A variável num assume a quantidade de cartões cadastrados
    int slot; // Variável que descobre o slot do cartão
    int start;// = ( num * 4 ) + 6; // Descobre onde começa o próximo slot de cada cartão na memória 
    int looping; // O número de loops que o for realiza
    int j; // Variável usado para o loop
    int count = EEPROM.read(0); // A variável count assume a quantidade de cartões cadastrados
    slot = findIDSLOT( a ); // Descobre o número do slot do cartão a ser deletado
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--; // Decrementa - 1 (retirou um cartão)
    EEPROM.write( 0, num ); // Escreve na posição de memória 0 a quantidade de cartões atualizada
    for ( j = 0; j < looping; j++ ) { // Loop com a quantidade de cartões
      EEPROM.write( start+j, EEPROM.read(start+4+j)); // Descola os valores do próximo cartão para o cartão deletado
    }
    for ( int k = 0; k < 4; k++ ) { // Agora, apaga o cartão deslocado para que ele não fique duplicado
      EEPROM.write( start+j+k, 0);
    }
    successDelete();
  }

///////////////////////////////////////// Compara os bytes  ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != NULL ) // Certifica-se que o primeiro array não está vazio
    match = true; // Assume-se que eles são iguais a princípio
  for ( int k = 0; k < 4; k++ ) { // Faz o loop 4x
   if ( a[k] != b[k] ) // Se um byte não é semelhante na comparação, ele retorna falso
      match = false;
  }
  if ( match ) { // Verifica se a variável match ainda é verdadeira, se sim retorna verdadeiro, se não retorna falso
    return true;
  }
  else  {
    return false;
  }
}

///////////////////////////////////////// Encontrar SLOT  ///////////////////////////////////
int findIDSLOT( byte find[] ) {
  int count = EEPROM.read(0); // Conta a quantidade de cartões cadastrados
  for ( int i = 1; i <= count; i++ ) { // Loop  até a quantidade de cartões
    readID(i); // Lê o da memória EEPROM
    if( checkTwo( find, storedCard ) ) { // Verifica se o storedCard lido da EEPROM é o mesmo do cartão lido pela antena
      return i; // Retorna o SLOT do cartão
      break; // Para a operação
    }
  }
}

///////////////////////////////////////// Encontrar cartão UID na memória EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  int count = EEPROM.read(0); // Conta quantos cartões estão cadastrados
  for ( int i = 1; i <= count; i++ ) {  // Entra no loop, sendo o limite a quantidade de cartões previamente cadastrados
    readID(i); // Chama a função de ler os cartões, passando como parâmetro a contagem dos cartões
    if( checkTwo( find, storedCard ) ) {  // Compara se o cartão lido existe na memória por meio da variável find (cartão lido) com storedCard (cartão armazenado)
      return true;
      break; // Se encontrou, retornar verdadeiro e interrompe a função
    }
    else {  // Se não encontrou, retorna falso  
    }
  }
  return false;
}

///////////////////////////////////////// Notificação da adição do cartão  ///////////////////////////////////
void successWrite() {
  Serial.println("Cartao UID cadastrado com sucesso da EEPROM");
}

///////////////////////////////////////// Escrita Falhou no EEPROM  ///////////////////////////////////
void failedWrite() {
   
  piscaStatusLed(4,tempoStatusLed,tempoStatusLed/2); // Avisa que a escrita no EEPROM não foi feita corretamente
  
  Serial.println("Falha! Existe alguma coisa errada com o cartão ou com a memória EEPROM");
}

///////////////////////////////////////// Notificação da remoção do cartão ///////////////////////////////////
void successDelete() {
  Serial.println("Cartao UID removido com sucesso da EEPROM"); 
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
  
  piscaStatusLed(3,tempoStatusLed,tempoStatusLed/2); // Avisa que o acesso foi negado por meio do Led
 
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

///////////////////////////////////////// Controle do Servo-Motor  ///////////////////////////////////

void servoControl()
{
  servol.attach(2); // O controle do servo motor será feito pelo pino 2
  
  // Dá uma volta de 360º
  servol.write(0);
  delay(1000);
  servol.write(360);
  delay(1000);
  servol.write(0);
  delay(1000);
}
