/* runCAEN library for interface with CAEN N6740 digitizer

Author: Christopher Dumas
*/

#include <stdio.h>
#include <stdlib.h>
#include <CAENDigitizer.h>
#include <math.h>
#include <string.h>
#include "runCAEN.h"

#define nChannels  4   //number of active channels
#define channelMask 0x0F //mask enabling triggers
#define groups  1       //number of active groups
#define record 150 	//number of samples per record
#define ptrigger  100	//percent of window post trigger
#define DCOffset 0xF000 //hex value DC offset
#define blockEvents 1000//max number of events transmitted per block

void setupCAEN(int* handle, char** buffer, int thresh){
/*TODO: Change this function to take as input all of the settings which are
defined in the macros above.

This function opens a connection with the CAEN Digitizer and initializes 
it withe the proper acquisition settings. The first two arguments, handle
and buffer, are designed to be pass by reference and are necessary as
input to other functions in this library
*/

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
    //    CAEN_DGTZ_WriteRegister(*handle, CAEN_DGTZ_BROAD_CH_CONFIGBIT_SET_ADD, 1<<3);
    CAEN_DGTZ_ClearData(*handle);
    error = CAEN_DGTZ_MallocReadoutBuffer(*handle, buffer, &size); 
    if(error!=CAEN_DGTZ_Success){
        printf("Failed to allocate readout buffer\n");
        printf("%d\n",error);
        exit(0);
    }
   printf("Readout buffer allocated\n"); 
}


void startCAEN(int handle){//Begin acquisition
    CAEN_DGTZ_SWStartAcquisition(handle);
    CAEN_DGTZ_ClearData(handle);
}

void stopCAEN(int handle){//Stop acquisition
    CAEN_DGTZ_ClearData(handle);
    CAEN_DGTZ_SWStopAcquisition(handle);
}

void closeCAEN(int handle, char** buffer){//close the connection to the digitizer
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

/*The allocate and copy method takes in a CAEN Event pointer
and returns a standard integer array. The purpose of this method
is so that the code in the runCAEN library uses only the data
types defined in runCAEN.h, hiding the many structured defined in
the CAEN libraries*/

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


void freeArray(void** obj){//Frees an array allocated by allocateAndCopy
                            //See freeEvent for details
    int n = nChannels; 
    int i;
    for(i = 0;i<n;i++){
        free(obj[i]);
    }
    free(obj);
}

/*This takes an eventNode pointer and frees it's memory.
MUST be invoked to free an eventNode, simply calling free(Evt)
in a readout loop will result in a memory leak and program crash*/
void freeEvent(eventNode* Evt){
    if(Evt!= NULL){
        freeArray((void**) Evt->event);
        free(Evt);
    }
}

/*Takes the head and tail of a linked list of eventNodes and appends all the
events currently in the digitizers buffers to the list*/
int getFromCAEN(int handle, char* buffer, eventNode** head, eventNode** tail){
    uint32_t bufferSize, numEvents;
    int i;
    char* eventPointer;
    CAEN_DGTZ_UINT16_EVENT_t* Evt=NULL;
    CAEN_DGTZ_AllocateEvent(handle,(void**)&Evt);
    int error;
    CAEN_DGTZ_EventInfo_t eventInfo;
    error = CAEN_DGTZ_ReadData(handle,CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,buffer,&bufferSize); //reads raw data stream from CAEN
    if(error!=0){
        printf("Failed to read from CAEN, error %d\n",error);
    }
    numEvents=0;
    if(bufferSize!=0){
        CAEN_DGTZ_GetNumEvents(handle,buffer,bufferSize,&numEvents);			//determine number of events in data stream buffer
    }
    for(i = 0;i<(int)numEvents;i++){
        CAEN_DGTZ_GetEventInfo(handle,buffer,bufferSize,i,&eventInfo,&eventPointer);		//get event info and pointer to event data
        CAEN_DGTZ_DecodeEvent(handle,eventPointer,(void**)&Evt);			//get event data structure from data pointer
        eventNode* newEvent = malloc(sizeof(eventNode));
        newEvent->event = allocateAndCopy(Evt);
        newEvent->channels = nChannels;
        newEvent->samples = record;
        newEvent->nxt = NULL;
        newEvent->prv=NULL;
        if(*head==NULL){
            *head=newEvent;
        }
        if(*tail!=NULL){
            (*tail)->nxt= newEvent;
            newEvent->prv = *tail;
        }
        *tail = newEvent;
    }
    CAEN_DGTZ_FreeEvent(handle,(void**)&Evt);
    CAEN_DGTZ_ClearData(handle);
    return (int)numEvents;
}
