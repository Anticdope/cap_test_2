// Simplified Touch sensing for Teensy 4.0 using analog input with DMX control
// Focused on reliable touch detection with minimal complexity

#include <TeensyDMX.h>

// Use the TeensyDMX "Sender" on Serial1 (DMX TX on pin 1)
namespace teensydmx = ::qindesign::teensydmx;
teensydmx::Sender dmxTx{Serial1};

// Constants
const uint8_t ledPin           = 13;   // Built-in LED
const uint8_t NUM_RGB_FIX      = 7;    // Seven RGB fixtures
const uint8_t CH_PER_RGB       = 3;    // Each uses R,G,B
const uint8_t CH_LAST_FIX      = 1;    // One single-channel fixture at the end
const uint16_t TOTAL_DMX_CH    = NUM_RGB_FIX * CH_PER_RGB + CH_LAST_FIX;

#define TOUCH_PIN A7           // Analog input for touch sensing
#define DEBOUNCE_TIME 50       // Milliseconds to debounce touch events
#define SAMPLE_COUNT 5         // Number of samples to average for each reading

// Variables
int baseline = 0;              // Baseline reading
int touchThreshold = 100;      // Detection threshold - increased for reliability
bool touched = false;          // Current touch state
unsigned long lastTouchTime = 0;       // For debouncing

// Debug mode - set to true for detailed serial output
const bool DEBUG_MODE = true;

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWriteFast(ledPin, HIGH);
  Serial.begin(9600);
  delay(1000);
  
  dmxTx.begin();
  allOff();

  Serial.println("Teensy 4.0 Touch Detection - Simplified");
  Serial.println("-------------------------------------");
  
  // Initial calibration
  calibrateBaseline();
  
  Serial.println("System ready!");
}

// Get a reliable reading by averaging multiple samples
int getReading() {
  long sum = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    sum += analogRead(TOUCH_PIN);
    delay(1);
  }
  return sum / SAMPLE_COUNT;
}

void calibrateBaseline() {
  Serial.println("Calibrating - don't touch the sensor for 3 seconds");
  
  // Take many samples for accurate baseline
  const int numCalibrationSamples = 50;
  long total = 0;
  
  // First discard some readings to let the ADC settle
  for (int i = 0; i < 5; i++) {
    analogRead(TOUCH_PIN);
    delay(5);
  }
  
  // Now collect actual calibration samples
  for (int i = 0; i < numCalibrationSamples; i++) {
    total += analogRead(TOUCH_PIN);
    
    // Blink LED to show calibration in progress
    if (i % 10 == 0) {
      digitalWriteFast(ledPin, !digitalRead(ledPin));
    }
    
    delay(30);
  }
  
  // Set baseline
  baseline = total / numCalibrationSamples;
  
  digitalWriteFast(ledPin, HIGH);  // Ensure LED is in correct state
  
  Serial.println("Calibration complete!");
  Serial.println("Baseline: " + String(baseline));
  Serial.println("Touch threshold: " + String(touchThreshold));
  
  // Initial debug reading
  if (DEBUG_MODE) {
    int currentValue = getReading();
    Serial.println("Current reading: " + String(currentValue) + 
                  ", Difference: " + String(currentValue - baseline));
  }
}

// Set RGB values for a fixture
void setRGB(uint8_t fixtureIndex, uint8_t val) {
  uint16_t base = fixtureIndex * CH_PER_RGB + 1;
  dmxTx.set(base    , val);
  dmxTx.set(base + 1, val);
  dmxTx.set(base + 2, val);
}

// Turn off all DMX channels
void allOff() {
  for (uint16_t ch = 1; ch <= TOTAL_DMX_CH; ch++) {
    dmxTx.set(ch, 0);
  }
}

// Light sequence on touch
void playLightSequence() {
  setRGB(0, 255);
  setRGB(5, 255);
  delay(500);

  setRGB(1, 255);
  setRGB(4, 255);
  delay(250);

  setRGB(2, 255);
  setRGB(3, 255);
  delay(2000);

  dmxTx.set(20, 255);
  delay(7000);

  dmxTx.set(20, 0);
  allOff();
}

void loop() {
  // Get current reading (averaged)
  int currentValue = getReading();
  int difference = currentValue - baseline;
  
  // Debug output periodically
  if (DEBUG_MODE && millis() % 1000 < 10) {
    Serial.println("Reading: " + String(currentValue) + 
                  ", Baseline: " + String(baseline) + 
                  ", Diff: " + String(difference));
  }
  
  // Touch detection with simple debouncing
  unsigned long currentTime = millis();
  
  if (!touched && difference > touchThreshold && 
      currentTime - lastTouchTime > DEBOUNCE_TIME) {
    // Touch detected
    Serial.println("TOUCHED! Value: " + String(currentValue) + 
                  ", Baseline: " + String(baseline) + 
                  ", Diff: " + String(difference));
    
    touched = true;
    lastTouchTime = currentTime;
    digitalWriteFast(ledPin, LOW);
    
    // Play the light sequence
    playLightSequence();
    
    digitalWriteFast(ledPin, HIGH);
  }
  else if (touched && difference < (touchThreshold/2) && 
           currentTime - lastTouchTime > DEBOUNCE_TIME) {
    // Touch released (using hysteresis - release at half the touch threshold)
    Serial.println("Released");
    touched = false;
    lastTouchTime = currentTime;
    allOff();
  }
  
  // Small delay for stability
  delay(10);
}