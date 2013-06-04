#include <stdio.h>
#include <stdlib.h>
#include <CAENDigitizer.h>
#include <math.h>
#include <string.h>
#include "runCAEN.h"

#define nChannels  32   //number of active channels
#define channelMask 0xF //mask enabling triggers
#define groups  4       //number of active groups
#define record 50 	//number of samples per record
#define ptrigger  100	//percent of window post trigger
#define DCOffset 0xF000 //hex value DC offset
#define blockEvents 100//max number of events transmitted per block

void setupCAEN(int* handle, char** buffer, int thresh){
    uint32_t size;
    CAEN_DGTZ_ErrorCode error;
    int i;
    error = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB,0,0,0,handle);
    if(error!=CAEN_DGTZ_Success){
        printf("Failed to open digitizer: error %d \n",error);
        exit(0);
    }
    CAEN_DGTZ_BoardInfo_t info;
    CAEN_DGTZ_GetInfo(*handle,&info);
    printf("Digitizer opened\n");
    printf("ROC Firmware version: %s\n", info.ROC_FirmwareRel);
    printf("AMC Firmware version: %s\n", info.AMC_FirmwareRel);
    
    CAEN_DGTZ_Reset(*handle);

    CAEN_DGTZ_SetRecordLength(*handle, record);

    CAEN_DGTZ_SetChannelGroupMask(*handle,0,channelMask);

    for(i = 0;i<groups;i++){
        CAEN_DGTZ_SetGroupSelfTrigger(*handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY, 0x1);
        CAEN_DGTZ_SetGroupDCOffset(*handle, i, DCOffset);
        CAEN_DGTZ_SetGroupTriggerThreshold(*handle,i,thresh);
    }

    for(i=0;i<nChannels;i++){
        CAEN_DGTZ_SetTriggerPolarity(*handle,i, CAEN_DGTZ_TriggerOnRisingEdge);
    }
    CAEN_DGTZ_SetGroupEnableMask(*handle, (unsigned int)pow((float)2,(float)groups)-1);
    CAEN_DGTZ_SetAcquisitionMode(*handle, CAEN_DGTZ_SW_CONTROLLED);
    CAEN_DGTZ_SetMaxNumEventsBLT(*handle,blockEvents);
    CAEN_DGTZ_SetPostTriggerSize(*handle,ptrigger);
    CAEN_DGTZ_SetIOLevel(*handle, CAEN_DGTZ_IOLevel_NIM);
    CAEN_DGTZ_SetExtTriggerInputMode(*handle,CAEN_DGTZ_TRGMODE_DISABLED);
    //    CAEN_DGTZ_WriteRegister(*handle, CAEN_DGTZ_BROAD_CH_CONFIGBIT_SET_ADD, 1<<3); //TEST PATTERN BIT SET #################################################################
    CAEN_DGTZ_ClearData(*handle);
    error = CAEN_DGTZ_MallocReadoutBuffer(*handle, buffer, &size); 
    if(error!=CAEN_DGTZ_Success){
        printf("Failed to allocate readout buffer\n");
        printf("%d\n",error);
        exit(0);
    }
   printf("Readout buffer allocat)ed\n"); 
}


void startCAEN(int handle){
    CAEN_DGTZ_SWStartAcquisition(handle);
    CAEN_DGTZ_ClearData(handle);
}

void stopCAEN(int handle){
    CAEN_DGTZ_ClearData(handle);
    CAEN_DGTZ_SWStopAcquisition(handle);
}

void closeCAEN(int handle, char** buffer){
    CAEN_DGTZ_ClearData(handle);
    int error = CAEN_DGTZ_CloseDigitizer(handle);
    if(error!=0){
        printf("Unable to close digitizer, error %d\n", error);
    }
    else{
        printf("Digitizer closed\n");
    }
    
    CAEN_DGTZ_FreeReadoutBuffer(buffer);
}


uint16_t** allocateAndCopy(CAEN_DGTZ_UINT16_EVENT_t* Evt){
    int i,j;
    int n = nChannels;
    uint16_t **obj = (uint16_t**) malloc(n*sizeof(uint16_t*));
    for(i=0;i<n;i++){
        size_t m =(size_t) Evt->ChSize[i]*sizeof(uint16_t);
        obj[i]=(uint16_t*) malloc(m);
        memmove((void*)obj[i],(const void*)Evt->DataChannel[i], m);
        /*for(j=0;j<m/sizeof(int);j++){
            obj[i][j] = Evt->DataChannel[i][j];
        }*/
    }         
    return obj;
}


void freeArray(void** obj){
    int n = nChannels; 
    int i;
    for(i = 0;i<n;i++){
        free(obj[i]);
    }
    free(obj);
}

void freeEvent(eventNode* Evt){
    if(Evt!=NULL) freeArray((void**) Evt->event);
    free((void*)Evt);
}
void getFromCAEN(int handle, char* buffer, buffNode** head, buffNode** tail){
    uint32_t bufferSize, numEvents;
    int i;
    char* eventPointer;
    CAEN_DGTZ_UINT16_EVENT_t* Evt=NULL;
    CAEN_DGTZ_AllocateEvent(handle,(void**)&Evt);
    int error;
    error = CAEN_DGTZ_ReadData(handle,CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,buffer,&bufferSize); //reads raw data stream from CAEN
    if(error!=0){
        printf("Failed to read from CAEN, error %d\n",error);
    }
    numEvents=0;
    if(bufferSize!=0){
        buffNode* newBuff = malloc(sizeof(buffNode));
        newBuff->buffer = malloc(bufferSize);
        memmove((void*)newBuff->buffer,(const void*)buffer,bufferSize);
        newBuff->size = bufferSize;
        newBuff->next = NULL;
        newBuff->prev = NULL;
        if(*head==NULL) *head=newBuff;
        if(*tail!=NULL{
            (*tail)->next=newBuff;
            newBuff->prev = *tail;
        }
        *tail = newBuff;
        CAEN_DGTZ_GetNumEvents(handle,buffer,bufferSize,&numEvents);			//determine number of events in data stream buffer
    }
    globalEventCount+=numEvents;
    for(i = 0;i<(int)numEvents;i++){
        CAEN_DGTZ_GetEventInfo(handle,buffer,bufferSize,i,&eventInfo,&eventPointer);		//get event info and pointer to event data
        CAEN_DGTZ_DecodeEvent(handle,eventPointer,(void**)&Evt);			//get event data structure from data pointer
        eventNode* newEvent = malloc(sizeof(eventNode));
        newEvent->event = allocateAndCopy(Evt);
        newEvent->channels = nChannels;
        newEvent->samples = record;
        newEvent->next = NULL;
        newEvent->prev=NULL;
        if(*head==NULL) *head=newEvent;
        if(*tail!=NULL){
            (*tail)->next= newEvent;
            newEvent->prev = *tail;
        }
        *tail = newEvent;
    }
    CAEN_DGTZ_FreeEvent(handle,(void**)&Evt);
    CAEN_DGTZ_ClearData(handle);
}

void decodeBuffer(int handle, buffNode** headBuff, buffNode** tailBuff, eventNode** headEvent, eventNode** tailEvent){
    uint32_t numEvents,size;
    int i;
    char* bufferPoint,eventPointer;
    CAEN_DGTZ_EventInfo_t eventInfo;
    CAEN_DGTZ_UINT16_EVENT_t* Evt=NULL;
    CAEN_DGTZ_AllocateEvent(handle,(void**)&Evt);
    buffNode* current = *headBuff;
    while(current!=NULL){
        size = current->size;
        bufferPoint = current->buffer;
        CAEN_DGTZ_GetNumEvents(handle,bufferPoint,size,&numEvents);
        for(i=0;i<(int)numEvents;i++){
            CAEN_DGTZ_GetEventInfo(handle,bufferPoint,size,i,&eventInfo,&eventPointer);
            CAEN_DGTZ_DecodeEvent(handle,eventPointer,(void**)&Evt);
        eventNode* newEvent = malloc(sizeof(eventNode));
        newEvent->event = allocateAndCopy(Evt);
        newEvent->channels = nChannels;
        newEvent->samples = record;
        newEvent->next = NULL;
        newEvent->prev=NULL;
        if(*headEvent==NULL) *headEvent=newEvent;
        if(*tailEvent!=NULL){
            (*tailEvent)->next= newEvent;
            newEvent->prev = *tailEvent;
        }
        *tailEvent = newEvent;
        }
        current = current->next;
    }

}
