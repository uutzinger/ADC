// Teensy external tiggered ADC with DMA
//
// This program uses pin23 as "external" clock for ADC conversion.
// It increases the clock and reports if adc conversion were missed.
// You will need to create a connection between pin 23 and pin 13.
//
// We are using the LPTMR hardware module:
// adclptmr uses LPTMR0 counter (pin 13) to clock ADC A0


// ------------------------------------
// Teensy 3.2
// ------------------------------------
// ADC rates
// Averaging 0 ------------------------
// 16 bit 735kS/s, VERY_HIGH, VERY_HIGH
// 12 bit 840kS/s, VERY_HIGH, VERY_HIGH
//  8 bit 940kS/s, VERY_HIGH, VERY_HIGH
// 16 bit 365kS/s, HIGH, HIGH
// 12 bit 430kS/s, HIGH, HIGH
//  8 bit 480kS/s, HIGH, HIGH
// Averaging 4 ------------------------
// 16 bit 205kS/s, VERY_HIGH, VERY_HIGH
// 12 bit 255kS/s, VERY_HIGH, VERY_HIGH
// 8  bit 290kS/s, VERY_HIGH, VERY_HIGH
// ------------------------------------

#define BUFFERSIZE 16384                           // The buffer takes dynamic memory. Stay below 85% of total dynamic memory
#include "ADC.h"
#include <DMAChannel.h>
ADC *adc = new ADC();
DMAChannel dma0;
DMAMEM static uint16_t buf[BUFFERSIZE];            // buffer

#define PIN_ADC 5                                  // ADC input pin
#define PRINTREGISTER(x) Serial.print(#x" 0x"); Serial.println(x,HEX)

// variables used in the interrupt service routines (need to be volatile)
volatile uint32_t lptmrticks, adcticks, adcval;
volatile bool     adc0_busy = false;

// In case LPTMR interrrupt is turned on in ADC library
// Its not needed
void lptmr_isr(void)
{
  LPTMR0_CSR |=  LPTMR_CSR_TCF;                    // clear
  lptmrticks++;
}

// ADC interrupt service routine
// Its not needed when DMA is enabled
void adc0_isr() {
  adcticks++;
  adcval = adc->adc0->readSingle();                // read and clear
}

// DMA interrupt service routine
// This is called when the DMA buffer is full
void dma0_isr(void) {
  adc0_busy = false;
  dma0.clearInterrupt();
  dma0.clearComplete();
}

void adc_init() {
  adc->adc0->disablePGA();        
  adc->adc0->setReference(ADC_REFERENCE::REF_3V3);
  adc->adc0->setAveraging(0); 
  adc->adc0->setResolution(8); 
  adc->adc0->disableCompare();
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED);
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);      

  // Initialize dma
  dma0.source((volatile uint16_t&)ADC0_RA);
  dma0.destinationBuffer(buf, sizeof(buf));
  dma0.triggerAtHardwareEvent(DMAMUX_SOURCE_ADC0);
  dma0.interruptAtCompletion();
  //dma0.disableOnCompletion();                    // Depends on what you want to do after DMA buffer is full
  dma0.attachInterrupt(&dma0_isr);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
  pinMode(PIN_ADC, INPUT);
  delay(1000);
}

float         delta, expected;
uint32_t      t, cnta=0, cntl=0;
elapsedMicros sinceStart_micros;

void loop() {
  adc_init();
  memset((void*)buf, 0, sizeof(buf));
  for (long Fadc = 130000; Fadc < 2000000; Fadc = Fadc+5000) {
    Serial.printf("Freq: %d", Fadc); 
    adc0_busy = true;
    adc->adc0->startExtTrigLPTMR(true);              // enable external LPTMR trigger and its interrupt
    adc->adc0->startSingleRead(PIN_ADC);             // call this to setup everything before the lptmr starts, differential is also possible
    adc->adc0->enableInterrupts(adc0_isr);
    adc->adc0->enableDMA();                          // set ADC_SC2_DMAEN
    dma0.enable();                                   //
    // Set External clock for ADC
    // connect pin 13 to pin 23 for this to work
    analogWriteResolution(4);
    analogWriteFrequency(23,long(Fadc*2));           // Create clock on pin13
    analogWrite(23, 8);                              // 50% dutycycle clock
    sinceStart_micros = 0; 
    adcticks = 0;
    lptmrticks = 0;
    while (adc0_busy) {    
      // Serial.printf("%d %d\n", adcticks, lptmrticks); 
    }
    t = sinceStart_micros;
    cnta = adcticks; 
    cntl = lptmrticks;
    // Stop ADC
    dma0.disable();
    adc->adc0->disableDMA();
    adc->adc0->stopExtTrigLPTMR(true);
    expected = t*1.e-6*Fadc;
    delta = Fadc - (BUFFERSIZE / (t*1.e-6));
    Serial.printf(" ADC %d", cnta); 
    Serial.printf(" LPT %d", cntl); 
    Serial.printf(" off by %+.3f Hz\n", delta);
    delay(100);
  }
}
