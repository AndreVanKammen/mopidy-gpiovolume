#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

  const char shared_memory_name[] = "/gpiovolume";

const int grayPin1 = 0;
const int grayPin2 = 2;
const int standByTogglePin = 3;
const int powerStatePin = 4;
const int dacClockPin = 24;
const int dacDataPin = 29;
const int maxVol = 27;
const int AmpOffTimeOut = 10000;

volatile bool syncVolumeToSharedMem = true;
volatile bool syncAmpOnToSharedMem = true;
volatile bool ignoreGrayISR = false;

bool autoAmpOn = false;
bool currentAmpOn = false;

int currentVol = 4;
int newVol = currentVol;

unsigned int lastSeen = 0;

void grayInit(int p1, int p2)
{
  // Put a zero in the output so we can simulate the gray counter
  // by switching from input to output (the pull-up installed on
  // the amplifier takes care of the HIGH signal)
  digitalWrite(p1, 0);
  digitalWrite(p2, 0);

  // Set to input and enable pull down resistors,
  // this way we can read the gary counts from the knob turns
  pinMode(p1, INPUT);
  pinMode(p2, INPUT);
  pullUpDnControl(p1, PUD_DOWN);
  pullUpDnControl(p2, PUD_DOWN);
}

void grayCode(int p1, int p2, int count)
{
  // Disable pull up/down resistors
  pullUpDnControl(p1,PUD_OFF);
  pullUpDnControl(p2,PUD_OFF);

  // Switch the io's from in to out to generate a gray code pattern
  //
  for (int i = 0; i<count; i++)
  {
    pinMode(p1, OUTPUT); delayMicroseconds(18000);
    pinMode(p2, OUTPUT); delayMicroseconds( 2000);
    pinMode(p1, INPUT ); delayMicroseconds(18000);
    pinMode(p2, INPUT ); delayMicroseconds( 2000);
  }
  pullUpDnControl(p1,PUD_DOWN);
  pullUpDnControl(p2,PUD_DOWN);
}

void syncVolumeToGPIO() {
  ignoreGrayISR = true;
  // Wait and try to turn down volume to 0
  delay(250); grayCode(0, 2, maxVol);
  // Second time. because the amplifier misses some during it's boot
  delay(250); grayCode(0, 2, maxVol);
  // Set volume to known value
  delay(250); grayCode(2, 0, currentVol);
  ignoreGrayISR = false;
}

void setPower(bool on) {
  bool isOn = digitalRead(powerStatePin) == 1;
  if (isOn==on)
    return;

  pullUpDnControl(standByTogglePin, PUD_OFF);
  pinMode(standByTogglePin, OUTPUT );
  delay(50);
  pinMode(standByTogglePin, INPUT );
  pullUpDnControl(standByTogglePin, PUD_DOWN);

  if (on) {
    if (currentVol>10)
      currentVol = 10;
    syncVolumeToGPIO();
  }
}

char oldState = '0';
void handleGrayISR(void) {
  if (!ignoreGrayISR) {
    char newState =
      digitalRead(2)
      ?(digitalRead(0)?'A':'B')
      :(digitalRead(0)?'D':'C');

    // The logitech Z-680 seems to trigger on these states
    if (oldState == 'D' && newState=='C')
    {
      newVol = currentVol<maxVol ? ++currentVol : currentVol;
      syncVolumeToSharedMem = true;
    }
    if (oldState == 'A' && newState=='B')
    {
      newVol = currentVol>0 ? --currentVol : currentVol;
      syncVolumeToSharedMem = true;
    }

    oldState = newState;
  }
}

void handlePowerISR(void) {
  syncAmpOnToSharedMem = true;
}

int main (int argc, char * argv[])
{
  char *pSharedMemory = NULL;

  int fd;
  int rc;

  umask(0);

  fd = shm_open(shared_memory_name, O_RDWR | O_CREAT, 0666);

  if (fd == -1) {
    fd = 0;
    //TODO logging
    //printf("Creating the shared memory failed; errno is %d\n", errno);
  } else {
    // The memory is created as a file that's 0 bytes long. Resize it.
    rc = ftruncate(fd, 8);
    if (rc) {
      //TODO logging
      //printf("Resizing the shared memory failed; errno is %d\n", errno);
    } else {
      // MMap the shared memory
      //void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
      pSharedMemory = (char*)mmap((void *)0, (size_t)8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (pSharedMemory == MAP_FAILED) {
        pSharedMemory = NULL;
        //TODO logging
        //printf("MMapping the shared memory failed; errno is %d\n", errno);
      }
      else {
        //TODO logging
        //printf("pSharedMemory = %p", pSharedMemory);
      }
    }
  }

  wiringPiSetup();

  grayInit(grayPin1, grayPin2);

  digitalWrite(standByTogglePin, 0);
  pinMode(standByTogglePin, INPUT );
  pullUpDnControl(standByTogglePin, PUD_DOWN);

  pinMode(powerStatePin, INPUT );
  currentAmpOn = autoAmpOn = digitalRead(powerStatePin) == 1;

  if (currentAmpOn)
    syncVolumeToGPIO();


  if (!autoAmpOn)
    lastSeen = millis()-40000;
  else
    lastSeen = millis();

  wiringPiISR(grayPin1, INT_EDGE_BOTH, &handleGrayISR);
  wiringPiISR(grayPin2, INT_EDGE_BOTH, &handleGrayISR);
  wiringPiISR(standByTogglePin, INT_EDGE_BOTH, &handlePowerISR);

  for (;;) {

    if (newVol!=currentVol) {
      ignoreGrayISR = true;
      if (newVol>currentVol) {
        currentVol++;
        grayCode(2,0,(currentVol==maxVol)?10:1);
      } else {
        currentVol--;
        grayCode(0,2,(currentVol==0)?10:1);
      }
      ignoreGrayISR = true;
    } else {
      delay(33);

      unsigned int newMillis = millis();

      // Read the state of the i2s clock and data pin and see
      // if they are different from the defaults i found by trial on error
      if (// digitalRead(dacClockPin)!=1 || some play's leave clock running
        digitalRead(dacDataPin)!=0) {
          lastSeen = newMillis;
      }

      // If it was longer then timout ago turn of the amplifier
      if ((newMillis-lastSeen)>AmpOffTimeOut) {
        if (autoAmpOn) {
          // Only do auto stuff if state matches
          if (autoAmpOn==currentAmpOn) {
            syncAmpOnToSharedMem = true;
            setPower(false);
          }
          autoAmpOn = false;
        }

        // Keep in range so it wont overflow in the calculation;
        lastSeen = newMillis-AmpOffTimeOut-1000;
      } else {
        if (!autoAmpOn) {
          // Only do auto stuff if state matches
          if (autoAmpOn==currentAmpOn) { 
            syncAmpOnToSharedMem = true;
            setPower(true);
          }
          autoAmpOn = true;
        }
      }
    }

    if (pSharedMemory!=NULL) {
      if (syncVolumeToSharedMem) {
        *pSharedMemory = newVol * 5;
        syncVolumeToSharedMem = false;
      } else {
        if (newVol != *pSharedMemory) {
          newVol = (*pSharedMemory) / 5;
        }
      }
      if (syncAmpOnToSharedMem) {
        delay(50);
        *(pSharedMemory+1) = currentAmpOn = digitalRead(powerStatePin) == 1;
        syncAmpOnToSharedMem = false;
      } else {
        bool newAmpOn = *(pSharedMemory+1) != 0;
        if (currentAmpOn != newAmpOn) {
          currentAmpOn = newAmpOn;
          setPower(currentAmpOn);
        }
      }
    }
  }

  return 0;
}
