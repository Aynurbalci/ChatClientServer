#include "commands.h"

static uint32_t crc32LookupTable[256];

FILE* logFile;
int exitCommand = 0;
char name[NAME_LENGTH];
int socketId;

void make_crc_table() {
    uint32_t POLYNOMIAL = 0xEDB88320;
    uint32_t remainder;
    unsigned char b = 0;
    do {
        // Start with the data byte
        remainder = b;
        for (uint32_t bit = 8; bit > 0; --bit) {
            
            if (remainder & 1)
                remainder = (remainder >> 1) ^ POLYNOMIAL;
            else
                remainder = (remainder >> 1);
        }
        crc32LookupTable[(size_t)b] = remainder;
    } while(0 != ++b);
}

void CRC32(char data[], size_t data_length, char bits[]) {
	uint32_t crc32 = 0xFFFFFFFFu;
	int i;
	for (i=0; i < data_length; i++) {
		uint32_t lookupIndex = (crc32 ^ data[i]) & 0xff;
		crc32 = (crc32 >> 8) ^ crc32LookupTable[lookupIndex];  
	}
	crc32 ^= 0xFFFFFFFFu;
    
    for(i = 0; i < 32; i++){
        bits[i] = '0' + ((crc32 >> i) & 0x0001);
    }
    bits[i] = 0;
}

char get_parity_bit(char data[], size_t data_length){
    int bitCount = 0;
    int i;
    for(i = 0; i < data_length; i++){
        int j;
        for(j = 0; j < sizeof(char)*8; j++){
            if((data[i] >> j) & 1){
                bitCount++;
            }
        }
    }
    
    return '0' + bitCount%2;
}


void ctrl_c_and_exit(int sig) {
    exitCommand = 1;
}
void message_sender(){
    while(1){
        char message[MESSAGE_LENGTH+NAME_LENGTH];
        char messageCpy[MESSAGE_LENGTH+NAME_LENGTH];
        char msgBuffer[BUFFER_LENGTH];
        
        struct MESG* msg = (MESG*) malloc (sizeof(MESG)); //mesaj değişkeni olusturulur.

        fgets(message, MESSAGE_LENGTH+NAME_LENGTH, stdin);
        strcpy(messageCpy, message);
        char* to = strtok(messageCpy, "->");
        char* text = strtok(NULL, "->");
        if(text == NULL){
            to[strcspn(to, "\n")] = 0;
            if(strcmp(to, "quit") == 0){
                char goneMessage[5] = "GONE";
                send(socketId, goneMessage, strlen(goneMessage), 0);
                exitCommand = 1;
                break;
            }
            else{
                fprintf(stdout, "Unknown operation\n");
                continue;
            }
        }
        strcpy(msg->command, "MESG"); // mesaj komutu setlenir.
        text[strcspn(text, "\n")] = 0;
        strcpy(msg->message, text); //mesaj texti setlenir
        strcpy(msg->to, to); //kime gideceği setlenir.
        strcpy(msg->from, name); //kimin gönderdiği setlenir.
        CRC32(msg->message, sizeof(msg->message), msg->crc);//crc bitleri setlenir.
        msg->parity = get_parity_bit(msg->message, sizeof(msg->message)); // parity biti setlenir/bulunur.
        
        message_to_string(msg, msgBuffer); //tek bir string haline getirir (|)
        
        send(socketId, msgBuffer, strlen(msgBuffer), 0); // mesaj servera gönderilir.
        fprintf(logFile, "%s\n", message); //log dosyası yazılır
        free(msg);
    }
    pthread_detach(pthread_self());
}
void ReceiveChat() { //mesaj geldiğinde çalışır
    char message[BUFFER_LENGTH];
    while (1) {
		int receive = recv(socketId, message, BUFFER_LENGTH, 0); //mesajın geldiği satır
        if (receive > 0) { //mesajın varlıgı
            char copy[BUFFER_LENGTH];
            strcpy(copy, message); 
            char* CMD = strtok(copy, "|"); //mesajın komut kısmını okur
            char* token = strtok(NULL, "|"); //mesajın geri kalanını okur
            if(token == NULL){ //sadece komuttan oluşuyorsa
                fprintf(stdout, "%s\n", message); //mesajın tamamını yazdırırr
            }
            if(strcmp(CMD, "MESG") == 0){ //messaj komutu geldiyse
                struct MESG* msg = (MESG*)malloc(sizeof(MESG)); //mesaj değişkeni oluşturulur
                string_to_message(message, msg); //mesaj msgye koyulur
                char currCRC[33]; 
                CRC32(msg->message, sizeof(msg->message), currCRC); //gelen mesajın crc değeri hesaplanır
                char parity = get_parity_bit(msg->message, sizeof(msg->message)); //gelen mesajın parity değeri hesaplanır
                
                if(strcmp(msg->crc, currCRC) != 0 || msg->parity != parity){ //crc veya parity degerlerinden biri farklıysa
         
                // fprintf(stdout,"message crc:%s\n current crc:%s\n",msg->crc, currCRC); //olması gereken ve hesaplanan crcleri gosterir.
                	
                    struct MERR* err = (MERR*)malloc(sizeof(MERR));
                    strcpy(err->command, "MERR");
                    strcpy(err->from, msg->from);
                    strcpy(err->to, msg->to);
                    char errMessage[BUFFER_LENGTH];
                    message_error_to_string(err, errMessage); 
                    send(socketId, errMessage, strlen(errMessage), 0); //hatalı mesaj komutu yollanır
                }
                else{ //mesaj hatalı degilse ekrana ve loga yazar
                    fprintf(stdout, "%s:%s\n", msg->from, msg->message);
                    fprintf(logFile, "%s:%s\n", msg->from, msg->message);
                }
            }
        }  
        else if (receive == 0) {
			break;
        }
        else {
            // -1
		}
		memset(message, 0, BUFFER_LENGTH);
    }
    pthread_detach(pthread_self());
}
int main(){ 
    printf("Enter your chat name: ");
    fgets(name,NAME_LENGTH,stdin); //kullanıcı ismi alınır
    name[strcspn(name, "\n")] = 0; //stringin sonundan\n karakterini siler
    make_crc_table(); //crc tablosu oluşturulur
    struct sockaddr_in server_addr;
    pthread_t messageSenderThread;
    pthread_t receiveChatThread;
//SOKET OLUŞTURMA
    socketId = socket(AF_INET, SOCK_STREAM,0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int connection = connect(socketId, (struct socketaddr*) &server_addr, sizeof(server_addr));
    CONN* connectionCommand = (CONN*)malloc(sizeof(CONN));
    strcpy(connectionCommand->command, "CONN");
    strcpy(connectionCommand->name, name);
    char connectionMessage[BUFFER_LENGTH];
    connection_to_string(connectionCommand, connectionMessage);
    send(socketId, connectionMessage, BUFFER_LENGTH, 0);
    char message[BUFFER_LENGTH];
    recv(socketId, message, BUFFER_LENGTH, 0);
    printf("%s\n", message);
    struct stat info; //Yoksa Log klasörü oluşturulur
    char dirName[] = "./Logs";
    if( stat( dirName, &info ) != 0 ){
        mkdir(dirName, 0700);
    }
    
    time_t t = time(NULL); //log dosyası oluşturulur
    struct tm tm = *localtime(&t);
    char fileName[128];
    sprintf(fileName,"./Logs/%s-%d-%d-%d-%d-%d.txt", name, tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,tm.tm_hour,tm.tm_min);
    fprintf(stdout, "%s\n",fileName);
    logFile = fopen(fileName, "w+");
    //SENDER VE RECEIVER THREADLERİ BASLATILIR
    pthread_create(&messageSenderThread, NULL, (void*)&message_sender, NULL);
    pthread_create(&receiveChatThread, NULL, (void*)&ReceiveChat, NULL);

    //Set exit flag when ctrl+c received from keyboard
    signal(SIGINT, ctrl_c_and_exit);
    //message_sender();
    while(1){
        if(exitCommand){
            printf("Closing connection.\n");
            break;
        }
        sleep(1);
    }
    fclose(logFile);
}
