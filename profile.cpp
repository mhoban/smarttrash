#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <architecture/byte_order.h>

#define swapr(s) ((s >> 8) | (s << 8))

#define DEPTH_DIVISOR 51.25
#define TEMP_DIVISOR 2.5

void bin_prnt_byte(int x)
{
   int n;
   for(n=0; n<8; n++)
   {
      if((x & 0x80) !=0)
      {
         printf("1");
      }
      else
      {
         printf("0");
      }
      if (n==3)
      {
         printf(" "); /* insert a space between nybbles */
      }
      x = x<<1;
   }
}

int main(int argc, char *argv[]) 
{
  FILE *f=NULL;
  float runningDepth=0.0, runningTemp=0.0, depthOffset=0.0;
  char value;
  unsigned long bytesRead=0;
  unsigned int elapsedTime = 0;
  short timeMultiplier=1;
  bool tick=false;
  bool first_line=1;

  if (!(f=fopen(argv[1],"rb"))) {
    printf("file b0rk\n");
    exit(1);
  }

  while (!feof(f)) {
    timeMultiplier=1;
    tick=false;
    unsigned char byte = fgetc(f);
    if ((char)byte != EOF) {
      bytesRead++;
      if (byte == 0xff) {
        //alarm, data =  7 bits of next byte
        //printf("alarm (someother one)\n");
        unsigned char alarm = fgetc(f);
        bytesRead++;
        switch(alarm)
        {
          case 0x01:
            printf("Alarm 1\n");
            break;
          case 0x02:
            printf("Alarm 2\n");
            break;
          case 0x04:
            printf("Workload\n");
            break;
          case 0x08:
          case 0x10:
          case 0x20:
            printf("Unknown\n");
            break;
          case 0x40:
            printf("Bookmark / safety stop timer\n");
            break;
        }
      }
      else if (byte == 0b11111110) {
        //absolute temp, data =  2 bytes (degrees c = data/TEMP_DIVISOR)
        unsigned short temp = 0;
        if (fread(&temp,sizeof(unsigned short),1,f)) {
          temp = swapr(temp);
          runningTemp = temp/TEMP_DIVISOR;
          //printf("absolute temp:\t\t%f\t",(float)((float)temp/TEMP_DIVISOR));
        }
      } 
      else if ((byte & 0b11111110) == 0b11111100) {
        //absolute depth, data = 17 bits (meters = data / DEPTH_DIVISOR)
        //1111110d dddddddd dddddddd
        unsigned depth;
        unsigned short val;
        if (fread(&val,sizeof(unsigned short),1,f)) {
          val = swapr(val);
          depth = (unsigned)((byte&0x01) << 16) + val;
          runningDepth = (float)((float)val/DEPTH_DIVISOR);
          depthOffset = runningDepth;
          tick = true;
        }
      }
      else if ((byte & 0b11111100) == 0b11111000) {
        //delta temperature, data = 10 bits (degrees c = data/TEMP_DIVISOR)
        //111110dd dddddddd
        unsigned char nextbyte = fgetc(f);
        short temp;
        short deltaT;
        bytesRead++;
        if ((byte & 0x02) == 0x02)
          temp = byte | 0x7f;
        else
          temp = byte & 0x01;
        temp = (temp << 8) + nextbyte;
        deltaT = (float)((float)temp/TEMP_DIVISOR);
        runningTemp += deltaT;
        //printf("unresolved delta temperature format\t");
      }
      else if ((byte & 0b11111000) == 0b11110000) {
        //delta depth, data = 11 bits (meters = data / DEPTH_DIVISOR)
        //11110ddd dddddddd
        short depth;
        float deltaD;
        unsigned char nextbyte = fgetc(f);
        bytesRead++;
        if ((byte & 0x04) == 0x04)
          depth = byte | 0xfc;
        else
          depth = byte & 0x03;
        depth = (depth << 8) + nextbyte;
        deltaD = (float)((float)depth/DEPTH_DIVISOR);
        runningDepth += deltaD;
        tick=true;
      }
      else if ((byte & 0b11110000) == 0b11100000) {
        //alarm, data = 4 bits
        //1110bbbb
        //printf("alarm (probably bookmark)\t");
      }
      else if ((byte & 0b11100000) == 0b11000000) {
        //time, data = 5 bits
        //110ddddd
        timeMultiplier = byte & 0x1f;
        tick=true;
        //elapsedTime += (time*4);
      }
      else if ((byte & 0b11000000) == 0b10000000) {
        //delta temperature, data = 6 bits
        //10bbbbbb
        float deltaT;
        if ((byte & 0x020) == 0x20)   /* Test bit 5 of byte, the sign bit      */
          value = (byte | 0xe0);        /* Set  the most significant 6 bits to 1 */
        else value = (byte & 0x1f);    /* use only the least significant 5 bits */
        deltaT = (float)((float)value/TEMP_DIVISOR);
        runningTemp += deltaT;
        //printf("delta temp:\t\t%f\t",deltaT);

      }
      else if ((byte & 0b10000000) == 0) {
        //delta depth, data = 7 bits
        //0ddddddd
        float deltaD;
        if ((byte & 0x40) == 0x40)
          value = (byte | 0xc0);
        else value = (byte & 0x3f);
        deltaD = (float)((float)value / DEPTH_DIVISOR);
        runningDepth += deltaD;
        tick=true;
      }
      //printf("running depth: %f\trunning temp: %f\n",runningDepth-depthOffset,runningTemp);
      if (first_line) {
        first_line=false;
        printf("time\tdepth\ttemperature\n");
      }
      /*imperial: printf ("%02d+%02d\t%f\t%f\n",elapsedTime/60,elapsedTime%60,(runningDepth-depthOffset)*3.28084,runningTemp*1.8+32);*/
      if (tick) {
        //printf ("%02d:%02d:%02d\t%f\t%f\n",elapsedTime/60/60,elapsedTime/60,elapsedTime%60,(runningDepth-depthOffset),runningTemp);
        printf("%02d+%02d\t%f\t%f\n",elapsedTime/60,elapsedTime%60,(runningDepth-depthOffset),runningTemp);
        //printf("%d\t%f\t%f\n",elapsedTime,(runningDepth-depthOffset),runningTemp);
        elapsedTime += (timeMultiplier*4);
      }
    }
  }
  printf("done\n");
  fclose(f);
  return 0;
}