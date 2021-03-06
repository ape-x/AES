//
//  main.cpp
//  AES128
//
//  Created by Mihnea Stefan on 19/09/2020.
//  Copyright © 2020 Mihnea Stefan. All rights reserved.
//


#include "LookUpTables.h"
#include "AES128.h"
#include "CSHA512.h"
#include "PRNG.h"


enum ModesOfOperation {
    ECB = 1,
    CBC = 2,
    OFB = 3,
    CTR = 4,
    CCM = 5
};

void cleanKeyExpansion(){
    for(int i=0;i<10;i++)
        for(int j=0;j<KEY_SIZE;j++)
            expandedKey[i][j] = 0;
}

void encryptionECB(uint8_t *text, uint8_t* encryption_key, long bytes){
        long blocks = bytes/BLOCK_SIZE;
       keyExpansion(encryption_key);
    
    for(int i=0;i<blocks;i++)
        encryptBlock(&text[i*BLOCK_SIZE], encryption_key);
    
    cleanKeyExpansion();
}

void decryptionECB(uint8_t *text, uint8_t* encryption_key, long bytes){
       long blocks = bytes/BLOCK_SIZE;
       keyExpansion(encryption_key);
      
  for(int i=0;i<blocks;i++)
       decryptBlock(&text[i*BLOCK_SIZE], encryption_key);
    
    cleanKeyExpansion();
}


void encryptionCBC(uint8_t* text, uint8_t* encryption_key, long bytes){
     long blocks = bytes/BLOCK_SIZE;
     uint8_t counter = 0;
     uint8_t *IV = PRNG((char*)encryption_key);
     
     keyExpansion(encryption_key);
     addRoundKey(text, IV);
     encryptBlock(text, encryption_key);
     
     for(int i=1;i<blocks;i++){
         addRoundKey(&text[i*BLOCK_SIZE], &text[(i-1)*BLOCK_SIZE]);
         encryptBlock(&text[i*BLOCK_SIZE], encryption_key);
     }
     
     for(long c = bytes-16;c<bytes;c++,counter++) // The IV can be made public
        text[c] = IV[counter];
     
     free(IV);
    cleanKeyExpansion();
 }

void decryptionCBC(uint8_t* text, uint8_t* encryption_key, long bytes){
    uint8_t counter = 0;
    long blocks = bytes/BLOCK_SIZE;
    uint8_t IV[KEY_SIZE];
    keyExpansion(encryption_key);
    
    for(long i = bytes-2*BLOCK_SIZE;i<bytes-BLOCK_SIZE;i++,counter++)
        IV[counter] = text[i];

    for(long i=blocks-2;i>0;i--){
        decryptBlock(&text[i*BLOCK_SIZE], encryption_key);
        addRoundKey(&text[i*BLOCK_SIZE], &text[(i-1)*BLOCK_SIZE]);
    }
    
    decryptBlock(text, encryption_key);
    addRoundKey(text, IV);
    addRoundKey(&text[bytes-2*BLOCK_SIZE], &text[bytes-2*BLOCK_SIZE]); // XOR IV with itself to set its content to 0
    cleanKeyExpansion();
}



void encryptionOFB(uint8_t *text, uint8_t *encryption_key, long bytes){
    uint8_t counter = 0;
    long blocks = bytes/BLOCK_SIZE;
    uint8_t auxIV[KEY_SIZE];
    uint8_t *IV = PRNG((char*)encryption_key);
    
    memcpy(auxIV, IV, KEY_SIZE);
    
    keyExpansion(encryption_key);
    encryptBlock(IV, encryption_key);
    
    for(int i=0;i<blocks-1;i++){
        addRoundKey(&text[i*BLOCK_SIZE], IV);
        encryptBlock(IV, encryption_key);
    }
    
    for(long c = bytes-BLOCK_SIZE;c<bytes;c++,counter++)
       text[c] = auxIV[counter];

    free(IV);
    cleanKeyExpansion();
}

void decryptionOFB(uint8_t *text, uint8_t *encryption_key, long bytes){
    uint8_t counter = 0;
    long blocks = bytes/BLOCK_SIZE;
    uint8_t IV[BLOCK_SIZE];
    
    for(long i = bytes-BLOCK_SIZE;i<bytes;i++,counter++)
        IV[counter] = text[i];
    
    keyExpansion(encryption_key);
    encryptBlock(IV, encryption_key);
    
    for(int i=0;i<blocks-1;i++){
        addRoundKey(&text[i*BLOCK_SIZE], IV);
        encryptBlock(IV, encryption_key);
    }
    
    addRoundKey(&text[bytes-1*BLOCK_SIZE], &text[bytes-1*BLOCK_SIZE]);
    cleanKeyExpansion();
}


inline static void incrementNonce(uint8_t *nonce){
    for(int i=0;i<16;i++)
        nonce[i]++;
}

void encryptionCTR(uint8_t *text, uint8_t *encryption_key, long bytes){
    uint8_t *nonce = PRNG((char*)encryption_key);
    uint8_t _nonce[KEY_SIZE];
    long blocks = bytes/BLOCK_SIZE;
    
    keyExpansion(encryption_key);
    
    for(long i = 0;i<blocks;i++){
        memcpy(_nonce, nonce, KEY_SIZE);
        encryptBlock(_nonce, encryption_key);
        addRoundKey(&text[i*BLOCK_SIZE], _nonce);
        incrementNonce(nonce);
    }

    free(nonce);
    cleanKeyExpansion();
}


void encryptionCCM(uint8_t *text, uint8_t *encryption_key, long bytes){
    uint8_t counter = 0;
    long blocks = bytes/BLOCK_SIZE;
    uint8_t MAC[BLOCK_SIZE];
    uint8_t temp[BLOCK_SIZE];
    
    memcpy(temp, text, BLOCK_SIZE);
    keyExpansion(encryption_key); // CBC encryption for MAC
    
    encryptBlock(temp, encryption_key);
    
    for(long i=1;i<blocks-1;i++){
        memcpy(MAC, &text[i*BLOCK_SIZE], BLOCK_SIZE);
        addRoundKey(MAC, temp);
        encryptBlock(MAC, encryption_key);
        memcpy(temp, MAC, BLOCK_SIZE);
    }

    encryptionCTR(text, encryption_key, bytes-1);
    
    for(long i=bytes-BLOCK_SIZE;i<bytes;i++,counter++)
        text[i] = MAC[counter]; // Adding MAC which will later be verified upon decryption

}


void decryptionCCM(uint8_t *text, uint8_t *encryption_key, long bytes){
    uint8_t counter = 0;
    long blocks = bytes/BLOCK_SIZE;
    uint8_t MAC[BLOCK_SIZE];
    uint8_t temp[BLOCK_SIZE];

    encryptionCTR(text, encryption_key, bytes-2);
    
    memcpy(temp, text, BLOCK_SIZE);
    keyExpansion(encryption_key); // CBC encryption for MAC
    
    encryptBlock(temp, encryption_key);
    
    for(long i=1;i<blocks-1;i++){
        memcpy(MAC, &text[i*BLOCK_SIZE], BLOCK_SIZE);
        addRoundKey(MAC, temp);
        encryptBlock(MAC, encryption_key);
        memcpy(temp, MAC, BLOCK_SIZE);
    }

    
    counter = 0;
    for(long i=bytes-BLOCK_SIZE; i<bytes;i++,counter++) // Checking MAC to see if message has been tainted
        if(MAC[counter] != text[i]){
            printf("\nMAC does not match\n");
            return;
        }else{
            text[i] = 0;
        }
}



FILE* searchForFilePlus(char* path, char* searchedItem){
		printf("\nEnter path to %s\t", searchedItem);
		scanf("%s", path);
		return fopen(path, "r");
}

long getFileSize(char* fileName){
		struct stat fSize;
		stat(fileName, &fSize);
		return fSize.st_size;
}

/*
 The Cryptographic Coat manages everything outside the encryption process, such as file handling, providing an interface to the
 available modes of operation.
 The task of any encryption* decryption* function is to perform cryptographic operations on its parameters, anything else is taken care of by the motor.
 */

void cryptographicCoat(){

		uint8_t padding = 0;
        uint8_t *buffer;
        int functionality;
        int modeOfOperation;
		FILE* fileReader;
		FILE* keyReader;
		uint8_t key[BLOCK_SIZE];
		char outputName[20];
		char filePath[PATH_MAX];
		char keyPath[PATH_MAX];
		long fileSize;
        void (*controller[5][5])(uint8_t*, uint8_t*, long) = { {encryptionECB, encryptionCBC, encryptionOFB, encryptionCTR, encryptionCCM }, { decryptionECB, decryptionCBC, decryptionOFB, encryptionCTR, decryptionCCM }  };
        
    
		printf("\nSelect functionality\n1 - Encryption\n2 - Decryption\n");
		scanf("%d", &functionality);
        
        if(functionality != 1 && functionality != 2)
                return;
    
        printf("\nSelect mode of operation\n1 - ECB\n2 - CBC\n3 - OFB\n4 - CTR\n5 - CCM\n");
        scanf("%d", &modeOfOperation);
    
        if(modeOfOperation<1 && modeOfOperation>3)
            return;
    		
		if(!(fileReader = searchForFilePlus(filePath, "file ")) || // check if files exist
			 !(keyReader = searchForFilePlus(keyPath, "key "))){
				printf("\nFile does not exist or could not be opened");
				return;
		}
		
        fread(key, 1, KEY_SIZE, keyReader);
		fclose(keyReader);
		fileSize = getFileSize(filePath);
    
        if(modeOfOperation == CBC || ((modeOfOperation == OFB || CCM )&& functionality == 1))  //Checks the mode of operation in order to determine whether or not to write/read the IV of the file
                    fileSize+=BLOCK_SIZE;
        
        buffer = (uint8_t*)malloc((fileSize+padding)*sizeof(uint8_t));
		
		if(buffer == NULL){
				printf("Insufficient memory");
				return;
		}
		
		if(fileSize % BLOCK_SIZE != 0){
				padding = (fileSize/BLOCK_SIZE+1)*BLOCK_SIZE - fileSize;
				for(long i = fileSize;i<fileSize+padding;i++)	// padding last block with white spaces
						buffer[i] = 32;
		}

		fread(buffer, 1, fileSize, fileReader);
		fclose(fileReader);

    controller[functionality-1][modeOfOperation-1](buffer, key, fileSize+padding);
    
    printf("\nEnter name of output file ");
    scanf("%s", &outputName);
		
		fileReader = fopen(outputName, "w");
    
        fwrite(buffer, 1, fileSize+padding, fileReader);
		fclose(fileReader);
		free(buffer);
}
