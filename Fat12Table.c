#include "Fat12Table.h"


uint8_t *mainTable;


uint16_t TableValue(uint16_t index,void *Table){
    AssignTable(Table);
    uint16_t result=0;

    uint16_t tableIndex=3*index/2;
    uint8_t a,b;
    a=mainTable[tableIndex];
    b=mainTable[tableIndex+1];

    if(index%2){
        result+=a>>4;
        result+=b<<4;
    }
    else{
        b=b<<4;
        b=b>>4;
        result+=a;
        result+=b<<8;
    }

    return result;
}
void AssignTableValue(uint16_t index, uint16_t value){
    uint16_t tableIndex=3*index/2;
    uint8_t a,b;
    a=mainTable[tableIndex];
    b=mainTable[tableIndex+1];

    uint8_t newA=0,newB=0;
    uint16_t temp;


    if(index%2){
        temp=value;
        temp=temp<<12;
        temp=temp>>8;
        newA=temp;

        temp=value;
        temp=temp<<4;
        temp=temp>>8;
        newB=temp;

        a=a<<4;
        a=a>>4;

        newA+=a;

    }
    else{
        temp=value;
        temp=temp<<8;
        temp=temp>>8;
        newA=temp;

        temp=value;
        temp=temp<<4;
        temp=temp>>12;
        newB=temp;

        b=b>>4;
        b=b<<4;

        newB+=b;

    }

    mainTable[tableIndex]=newA;
    mainTable[tableIndex+1]=newB;

}

void AssignTable(void *table){
    mainTable=table;
}