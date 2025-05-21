// Improved Touch sensing for Teensy 4.0 using analog input with DMX control
// Optimized for reliability and responsiveness

#include <TeensyDMX.h>
#include <RunningMedian.h>  // Include for better noise filtering

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
#define SAMPLE_SIZE 9          // Samples for median filter (odd number is better)
#define DEBOUNCE_TIME 50       // Milliseconds to debounce touch events
#define BASELINE_UPDATE_RATE 10000  // How often to update baseline (10 seconds)

// Variables
RunningMedian touchSamples = RunningMedian(SAMPLE_SIZE);
int baseline = 0;              // Baseline reading
int touchThreshold = 60;       // Initial detection threshold
int releaseThreshold = 40;     // Lower threshold for release detection (hysteresis)
bool touched = false;          // Current touch state
unsigned long lastTouchTime = 0;       // For debouncing
unsigned long lastBaselineUpdate = 0;  // For adaptive baseline

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWriteFast(ledPin, HIGH);
  Serial.begin(9600);
  delay(1000);
  
  dmxTx.begin();
  allOff();

  Serial.println("Teensy 4.0 Touch Detection - Optimized");
  Serial.println("-------------------------------------");
  
  // Initial calibration
  calibrateBaseline();
  
  Serial.println("System ready!");
}

void calibrateBaseline() {
  Serial.println("Calibrating - don't touch the sensor for 3 seconds");
  
  // Clear any old samples
  touchSamples.clear();
  
  // Take many samples for accurate baseline
  const int numCalibrationSamples = 100;
  long total = 0;
  
  // First discard some readings to let the ADC settle
  for (int i = 0; i < 10; i++) {
    analogRead(TOUCH_PIN);
    delay(5);
  }
  
  // Now collect actual calibration samples
  for (int i = 0; i < numCalibrationSamples; i++) {
    int sample = analogRead(TOUCH_PIN);
    total += sample;
    
    // Blink LED to show calibration in progress
    if (i % 10 == 0) {
      digitalWriteFast(ledPin, !digitalRead(ledPin));
    }
    
    delay(30);
  }
  
  // Set baseline and thresholds based on calibration
  baseline = total / numCalibrationSamples;
  
  // Calculate threshold based on noise level in calibration
  long sumDifferences = 0;
  for (int i = 0; i < 20; i++) {
    int sample = analogRead(TOUCH_PIN);
    sumDifferences += abs(sample - baseline);
    delay(10);
  }
  
  int noiseLevel = sumDifferences / 20;
  touchThreshold = max(noiseLevel * 3, 50);  // At least 3x noise or 50, whichever is higher
  releaseThreshold = touchThreshold * 2/3;   // Release at 2/3 of touch threshold
  
  digitalWriteFast(ledPin, HIGH);  // Ensure LED is in correct state
  
  Serial.println("Calibration complete!");
  Serial.println("Baseline: " + String(baseline));
  Serial.println("Touch threshold: " + String(touchThreshold));
  Serial.println("Release threshold: " + String(releaseThreshold));
  
  // Initialize median filter with baseline values
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    touchSamples.add(baseline);
  }
  
  lastBaselineUpdate = millis();
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

void updateAdaptiveBaseline() {
  // Only update if not touched and enough time has passed
  if (!touched && (millis() - lastBaselineUpdate > BASELINE_UPDATE_RATE)) {
    // Collect some samples when not touched
    long total = 0;
    for (int i = 0; i < 10; i++) {
      total += analogRead(TOUCH_PIN);
      delay(5);
    }
    
    // Slowly adjust baseline (10% new, 90% old)
    int newReading = total / 10;
    baseline = (baseline * 9 + newReading) / 10;
    lastBaselineUpdate = millis();
    
    Serial.println("Updated baseline: " + String(baseline));
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
  // Add new reading to median filter
  touchSamples.add(analogRead(TOUCH_PIN));
  
  // Get filtered reading
  int filteredValue = touchSamples.getMedian();
  int difference = filteredValue - baseline;
  
  // Update baseline over time to adapt to environment
  updateAdaptiveBaseline();
  
  // Touch detection with hysteresis and debouncing
  unsigned long currentTime = millis();
  
  if (!touched && difference > touchThreshold && 
      currentTime - lastTouchTime > DEBOUNCE_TIME) {
    // Touch detected
    Serial.println("TOUCHED! Value: " + String(filteredValue) + 
                  ", Baseline: " + String(baseline) + 
                  ", Diff: " + String(difference));
    
    touched = true;
    lastTouchTime = currentTime;
    digitalWriteFast(ledPin, LOW);
    
    // Play the light sequence
    playLightSequence();
    
    digitalWriteFast(ledPin, HIGH);
  }
  else if (touched && difference < releaseThreshold && 
           currentTime - lastTouchTime > DEBOUNCE_TIME) {
    // Touch released
    Serial.println("Released");
    touched = false;
    lastTouchTime = currentTime;
    allOff();
  }
  
  // Small delay for stability
  delay(10);  // Much more responsive than 50ms
}