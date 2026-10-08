#include "tiltmouse/mouse_transport.h"
int main(){tm_mouse_report_t r; return tm_mouse_report_make(1,2,0,&r)?0:1;}
