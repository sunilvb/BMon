#ifndef DataP_h
#define DataP_h

#include "Arduino.h"

class DataP
{

public:
    String id;
    String temp;
    String humidity;
    String time;
    DataP (String n, String id, String dept, String pos);

        
};

DataP::DataP (String n, String t, String h, String tm)
{
    id=n;
    temp = t;
    humidity = h;
    time = tm;
}

#endif