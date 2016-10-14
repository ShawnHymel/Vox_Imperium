#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputAnalog         adc1;           //xy=255,182
AudioFilterBiquad        biquad1;        //xy=394,182
AudioPlaySerialflashRaw  playFlashRaw1;  //xy=508,319
AudioFilterStateVariable filter1;        //xy=531,189
AudioMixer4              mixer1;         //xy=710,196
AudioAnalyzePeak         peak1;          //xy=709,266
AudioOutputAnalog        dac1;           //xy=844,196
AudioConnection          patchCord1(adc1, biquad1);
AudioConnection          patchCord2(biquad1, 0, filter1, 0);
AudioConnection          patchCord3(playFlashRaw1, 0, mixer1, 2);
AudioConnection          patchCord4(filter1, 0, mixer1, 0);
AudioConnection          patchCord5(filter1, 1, peak1, 0);
AudioConnection          patchCord6(filter1, 2, mixer1, 1);
AudioConnection          patchCord7(mixer1, dac1);
// GUItool: end automatically generated code

// Parameters
const bool DEBUG = false;
const bool ADD_BREAK = true;
const bool ADD_CLICK = true;
const bool FEEDBACK_SUPPRESSION = false;  // Enables input filter
const bool LOWPASS_CUTOFF = 2800;         // Hz
const unsigned int CROSSOVER_FREQ = 2000; // Filter center freq
const float BASS_GAIN_ON = 0.01;
const float BASS_GAIN_OFF = 0.0;
const float TREBLE_GAIN_ON = 0.25;    // Voice volume
const float TREBLE_GAIN_OFF = 0.0;
const float SFX_GAIN = 0.5;           // Sound clip volume
const float SQUELCH_CUTOFF = 0.10;    // Volume level
const int HYSTERESIS_TIME_ON = 20;    // Milliseconds
const int HYSTERESIS_TIME_OFF = 400;  // Milliseconds

// Pins
const int FLASH_CS = 6;               // Serial flash chip select
const int AMP_ENABLE = 5;             // Amplifier enable pin

// On/Off state machine states
typedef enum volState {
  QUIET,
  QUIET_TO_LOUD,
  LOUD,
  LOUD_TO_QUIET,
} VolState;

// Global variables
elapsedMillis fps; // Sample peak only if we have available cycles
VolState state = QUIET;
unsigned long timer;

void setup() {
  
  if ( DEBUG ) {
    Serial.begin( 9600 );
  }

  // Initialize amplifier
  AudioMemory(20);
  dac1.analogReference(EXTERNAL); // much louder!
  delay(50);                      // time for DAC voltage stable
  pinMode(AMP_ENABLE, OUTPUT);

  // wait up to 10 seconds for Arduino Serial Monitor
  unsigned long startMillis = millis();
  while ( !Serial && ( millis() - startMillis < 10000 ) );

  // Butterworth lowpass filter (reduces audio feedback)
  if ( FEEDBACK_SUPPRESSION ) {
    biquad1.setLowpass(0, LOWPASS_CUTOFF, 0.707);
  } else {
    biquad1.setLowpass(0, 8000, 0.707);
  }

  // Adjust gain
  mixer1.gain(0, BASS_GAIN_OFF);
  mixer1.gain(1, TREBLE_GAIN_OFF);
  mixer1.gain(2, SFX_GAIN);

  // Initialize serial flash
  if ( !SerialFlash.begin(FLASH_CS) ) {
    if ( DEBUG ) {
      Serial.println( "Unable to access SPI Flash chip" );
    }
  }
  
  if ( DEBUG ) {
    Serial.println("Finished init");
  }
}

void loop() {

  if ( (fps > 24) && peak1.available() ) {

    // State machine
    switch ( state ) {

      // Wait until the mic picks up some sound
      case QUIET:
        if ( peak1.read() > SQUELCH_CUTOFF ) {
          timer = millis();
          state = QUIET_TO_LOUD;
        }
        break;

      // If sound continues, play break
      case QUIET_TO_LOUD:
        if ( peak1.read() <= SQUELCH_CUTOFF ) {
          state = QUIET;
        } else {
          if ( millis() > timer + HYSTERESIS_TIME_ON ) {
            
            if ( DEBUG ) {
              Serial.println("ON");
            }

            // Turn on amp, play sound, turn on mic
            digitalWrite(AMP_ENABLE, HIGH);
            if ( ADD_BREAK ) {
              playFile("break.raw");
            }
            mixer1.gain(0, BASS_GAIN_ON);
            mixer1.gain(1, TREBLE_GAIN_ON);

            // Go to next state
            state = LOUD;
          }
        }
        break;

      // Filter mic input and play it through speakers
      case LOUD:
        if ( peak1.read() <= SQUELCH_CUTOFF ) {
          timer = millis();
          state = LOUD_TO_QUIET;
        }
        break;

      // If no sound for a time, play click
      case LOUD_TO_QUIET:
        if ( peak1.read() > SQUELCH_CUTOFF ) {
          state = LOUD;
        } else {
          if ( millis() > timer + HYSTERESIS_TIME_OFF ) {

            if ( DEBUG ) {
              Serial.println("OFF");
            }

            // Play sound, turn off amp and mic
            if ( ADD_CLICK ) {
              playFile("click.raw");
            }
            digitalWrite(AMP_ENABLE, LOW);
            mixer1.gain(0, BASS_GAIN_OFF);
            mixer1.gain(1, TREBLE_GAIN_OFF);
            state = QUIET;
          }
        }
        break;

      // You really shouldn't get here
      default:
        break;
    }
  }
}

// Play a sound clip from serial flash
void playFile( const char* filename ) {

  if ( DEBUG ) {
    Serial.print("Playing file: ");
    Serial.print(filename);
  }

  // Start playing the file
  playFlashRaw1.play(filename);

  // A brief delay for the library read info
  delay(5);

  // Wait for the file to finish playing
  while ( playFlashRaw1.isPlaying() );

  if ( DEBUG ) {
    Serial.println("...done");
  }
}