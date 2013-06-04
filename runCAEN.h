typedef struct eventNode eventNode;
struct eventNode{
    int channels;
    int samples;
    uint16_t** event;
    eventNode* nxt;
    eventNode* prv;
};


int getFromCAEN(int, char*, eventNode**, eventNode**);
void setupCAEN(int*, char**, int);
void startCAEN(int);
void stopCAEN(int);
void freeEvent(eventNode*);

void closeCAEN(int,char**);
