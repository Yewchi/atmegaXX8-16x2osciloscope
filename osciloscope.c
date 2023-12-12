// github.com/yewchi zyewchi@gmail.com

#define SAMPLES_STORED 780
#define PIXEL_PADDING_SETTING 0
#define VERTICAL_CHAR_PIXELS 8
#define HORIZONTAL_CHAR_PIXELS 5
#define UPDATE_DRAW_CHAR_PAIR_COUNT 4
#define PIXELS_PER_CHAR 5
// PIXELS_PER_CHAR + 1 padding character
#define SPACE_PER_CHAR 5
#define LCD_CHARACTER_LIMIT 8
#define HORIZONTAL_WAVEFORM_CHARS 11
#define VERTICAL_WAVEFORM_CHARS 2
#define HORIZONTAL_CHAR_SPACE 5
// 11*SPACE_PER_CHAR // with one extra pixel for simplicity
#define HORIZONTAL_WAVEFORM_SPACE 55
// 8*2 + 1
#define VERTICAL_WAVEFORM_SPACE 16
#define VERTICAL_FINAL_INDEX_ROW 15
#define VERTICAL_SECOND_CHAR_START 8
#define ANALOG_READ_HIGH 1024
#define ANALOG_READ_DIV 60

// ms to freeze wave for 2 times in a row every 16 or so updates.
#define FREEZE_WAVE_DURATION 40

#define ZOOM_PITCH_FAST 453
#define ZOOM_PITCH_NORMAL 226
#define ZOOM_PITCH_SLOW 113
#define ZOOM_PITCH_VERY_SLOW 56.7
#define ZOOM_PITCH_LFO_3 20
#define ZOOM_PITCH_LFO_2 4.12
#define ZOOM_PITCH_LFO_1 2.06

#define WATERFALL_SLOPE_SPREAD 3
#define WATERFALL_TRIGGER_PIXELS 2
#define WATERFALL_DOWN_PIXEL_FLAG 64
#define WATERFALL_UP_PIXEL_FLAG 128
#define WATERFALL_LIMIT_PROGRESS_TIME 310

#define REDRAW_NO_UPDATE 1

#define micros_LAG_TIME 4

#define READ_AC_SIGNAL_CENTERED
#define STRETCH_LOW_AMP_SIGNAL_SIMPLE

//#define FIND_START_OF_WAVE_SIMPLE

// the analog Arduino read pin used, A0.
#define READ_PIN 0

#include <LiquidCrystal.h>
#include <math.h>

/* https://www.arduino.cc/reference/en/libraries/liquidcrystal/ */
#define LCD_RS 12
#define LCD_EN 11
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 3
#define LCD_D7 2

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

unsigned long curr_time = micros();
unsigned long sampling_period = 0;

unsigned short samples[SAMPLES_STORED];

const unsigned short min_analog_read_div = (int)ANALOG_READ_HIGH / VERTICAL_WAVEFORM_SPACE;
short analog_read_div = min_analog_read_div;

unsigned short zoom_div = 1;

//char print_str[6];

void boardDependantCheckMemory(byte flag) {
}

short expected_low_peak_limit = 500;
short expected_middle_of_range = 512;
uint8_t low_peak_reliability = 1;
uint8_t last_pitch_update = 0;
float last_pitch = 440.0f;
/* take_samples()
 *  limitations: starts reading after finding over-512, after finding a less-than 512 that is lower than
 *   an expected low peak, or after 6000 samples have been taken, i.e. some noticable fraction of a second. The
 *   sample rate might be something close to 90k, it was less than 96k.
 */
void take_samples() {
  short readVal = analogRead(READ_PIN);
  short peak_high = 0;
  short peak_low = 1024;
  
  unsigned long end_sampling_time;
  
#if defined(FIND_START_OF_WAVE_SIMPLE)
  byte waveform_is_high = readVal > 505 ? 1 : 0; // 0 - 1024 analog in limits assumed in all code
  for (short i=0; i<=6000; i++) {
    readVal = analogRead(READ_PIN);
    if (readVal > peak_high) {
      peak_high = readVal;
    } else if (readVal < peak_low) {
      peak_low = readVal;
    }
    if (waveform_is_high) {
      if (readVal < 505)
        waveform_is_high = 0;
    } else if (readVal > 512) {
      break;
    }
  }u
#else
  byte waveform_is_high = readVal > expected_low_peak_limit ? 1 : 0; // 0 - 1024 analog in limits assumed in all code
  byte high_low_cross_limit = 5;
  byte seen_below_middle = 0;
  for (short i=0; i<=6000; i++) {
    readVal = analogRead(READ_PIN);
    if (readVal > peak_high) {
      peak_high = readVal;
    } else if (readVal < peak_low) {
      peak_low = readVal;
    }
    if (readVal < expected_middle_of_range) {
      if (!seen_below_middle) {
        seen_below_middle = 1;
        if (--high_low_cross_limit <= 0)
          break;
      }
      if (waveform_is_high) {
        if (readVal < expected_low_peak_limit)
          waveform_is_high = 0;
      }
    } else if (!waveform_is_high) {
      break;
    } else {
      seen_below_middle = 0;
    }
  }
#endif

  // SAMPLING
  // If this looks silly I thought about unrolling 650 iterations
  if (last_pitch > ZOOM_PITCH_VERY_SLOW) {
    curr_time = micros();
    // get all samples at highest rate possible
    for (short i=0; i<SAMPLES_STORED; i++) {
      samples[i] = analogRead(READ_PIN);
    }
    end_sampling_time = micros() - micros_LAG_TIME;
  } else if (last_pitch > ZOOM_PITCH_LFO_3) {
    curr_time = micros();
    // get all samples at a reduced rate for sub rates
    for (short i=0; i<SAMPLES_STORED*2; i++) {
      samples[i/2] = analogRead(READ_PIN);
    }
    end_sampling_time = micros() - micros_LAG_TIME;
  } else if (last_pitch > ZOOM_PITCH_LFO_2) {
    curr_time = micros();
    // get all samples at a very reduced rate for LFOs
    for (short i=0; i<SAMPLES_STORED*6; i++) {
      samples[i/6] = analogRead(READ_PIN);
    }
    end_sampling_time = micros() - micros_LAG_TIME;
  } else {
    curr_time = micros();
    // get all samples at a the slowest rate for slow LFOs
    // this delays the update greatly but hotfixes finding the low hertz pitch
    for (short i=0; i<SAMPLES_STORED*32; i++) {
      samples[i/32] = analogRead(READ_PIN);
    }
    end_sampling_time = micros() - micros_LAG_TIME;
  }
  sampling_period = end_sampling_time - curr_time;

  // Find sample peaks
  for (short i=0; i<SAMPLES_STORED; i++) {
    readVal = samples[i];
    if (readVal > peak_high) {
      peak_high = readVal;
    } else if (readVal < peak_low) {
      peak_low = readVal;
    }
  }
  
  // Set next low-found-trigger for an over-avg read wave
#if !defined(FIND_START_OF_WAVE_SIMPLE)
  byte target_reliability = high_low_cross_limit^2;
  if (low_peak_reliability > target_reliability+1)
    low_peak_reliability--;
  else if (low_peak_reliability < target_reliability-1)
    low_peak_reliability++;

  expected_middle_of_range = (peak_high + peak_low) / 2;
  expected_low_peak_limit = peak_low + (peak_high - peak_low)/low_peak_reliability;
  ////lcd.setCursor(11,0);
  ////lcd.print(low_peak_reliability);
  
  // Simple find signal Hz.
  uint16_t times_low = -1;
  uint16_t start_Hz_index = 0;
  uint16_t end_Hz_index = SAMPLES_STORED;
  seen_below_middle = 0;
  for (short i = 0; i<SAMPLES_STORED; i++) {
    if (!seen_below_middle) {
      if (samples[i] < (expected_low_peak_limit + expected_middle_of_range)/2) {
        seen_below_middle = 1;
        times_low++;
        if (start_Hz_index == 0)
          start_Hz_index = i;
        end_Hz_index = i;
      }
    } else if (samples[i] > (2*expected_middle_of_range - expected_low_peak_limit)) {
      seen_below_middle = 0;
    }
  }
  float pitch = (float) (times_low / (
        0.000001*(sampling_period * end_Hz_index - sampling_period * start_Hz_index)
          / SAMPLES_STORED
      )
    );
  pitch = pitch < 0.6 ? 0.0 : pitch;
  pitch = pitch > 9999.0 ? 9999.0 : pitch;

  // Process pitch display
  // / important: no SAMPLE_STORED nor per analogRead time math, only total sampling_period
  uint8_t update_check = (uint8_t)((curr_time / 100000) % 256);

  if ( abs(pitch - last_pitch) < pow(ceil(last_pitch), 0.5)
      || abs(update_check - last_pitch_update) > 8) { // p%256, may flip update_check to 0.
    //sprintf(print_str, "%d.%d", max(1, (uint16_t)floor(pitch)), ((uint16_t)floor(pitch*10))%10);
    lcd.setCursor(11,0);
    //lcd.print(print_str);
    uint16_t pitch_int = (uint16_t)floor(pitch);
    lcd.print(pitch_int);
    if (pitch_int >= 1000 && pitch_int < 10000) {
      lcd.print(' ');
    } else { 
      lcd.print('.');
      lcd.print(((uint16_t)floor(pitch*10))%10);
      if (pitch_int < 100)
        lcd.print(' ');
      if (pitch_int < 10) 
        lcd.print(' ');
    }
    last_pitch_update = update_check;
    last_pitch = pitch;

    // update the horizontal zooming to display the entire cycle
    if (zoom_div > 1 && pitch > ZOOM_PITCH_FAST) { // ZOOM_PITCH_FAST
      zoom_div = 1;
      lcd.setCursor(13, 1);
      lcd.print('>'); lcd.print('>'); lcd.print('>');
    } else if (zoom_div == 3 && pitch > ZOOM_PITCH_NORMAL*1.15 // ZOOM_PITCH_NORMAL
        || zoom_div < 2 && pitch < ZOOM_PITCH_FAST) {
      zoom_div = 2;
      lcd.setCursor(13, 1);
      lcd.print('>'); lcd.print('>'); lcd.print(' ');
    } else if (zoom_div == 4 && pitch > ZOOM_PITCH_SLOW*1.15 // ZOOM_PITCH_SLOW
        || zoom_div < 3 && pitch < ZOOM_PITCH_NORMAL) {
      zoom_div = 3;
      lcd.setCursor(13, 1);
      lcd.print('>'); lcd.print(' '); lcd.print(' ');
    } else if (zoom_div == 7 && pitch > ZOOM_PITCH_VERY_SLOW*1.15 // ZOOM_PITCH_VERY_SLOW
        || zoom_div < 4 && pitch < ZOOM_PITCH_SLOW){
      zoom_div = 4;
      lcd.setCursor(13, 1);
      lcd.print('L'); lcd.print('F'); lcd.print(4);
    } else if (zoom_div == 11 && pitch > ZOOM_PITCH_LFO_3*1.15 // ZOOM_PITCH_LFO_FAST
        || zoom_div < 7 && pitch < ZOOM_PITCH_VERY_SLOW){
      zoom_div = 7;
      lcd.setCursor(13, 1);
      lcd.print('L'); lcd.print('F'); lcd.print(3);
    } else if (zoom_div == 12 && pitch > ZOOM_PITCH_LFO_2*1.15 // ZOOM_PITCH_LFO_NORMAL
        || zoom_div < 11 && pitch < ZOOM_PITCH_LFO_3){
      zoom_div = 11;
      lcd.setCursor(13, 1);
      lcd.print('L'); lcd.print('F'); lcd.print(2);
    } else if (zoom_div < 12 && pitch < ZOOM_PITCH_LFO_2) { // ZOOM_PITCH_LFO_SLOW
      zoom_div = 12;
      lcd.setCursor(13, 1);
      lcd.print('L'); lcd.print('F'); lcd.print('1');
    }
  }
#endif

  // Transform data / set variables for viewing
#ifdef STRETCH_LOW_AMP_SIGNAL_SIMPLE
  analog_read_div = (short)20*ceil(((peak_high - peak_low)/VERTICAL_WAVEFORM_SPACE)/20.0);
#endif
#ifdef READ_AC_SIGNAL_CENTERED
  //samples[i] -= peak_low;  80 to 65, 0 to 100 limits, 10 max div with 10 pixels, scales at 3, so use 15 / 3 = 5,
  // answer is take values down by 22.5 to base 42.5 max 57.5, but that's to center, which doesn't help immediately due to divisor.
  // space is 3*10 = 30, so we want centered 15 in 30. centered div / range in div * space.
  // (space - range) / 2 == new minimum sample value when centered == 7.5
  // reduction for samples == 65 - 7.5; giving 7.5 to 22.5 range in 30.
  int32_t adjust = (int32_t)floor((analog_read_div * VERTICAL_WAVEFORM_SPACE - peak_high + peak_low) / 2.0) - peak_low;
  /*
  lcd.setCursor(11,0);
  lcd.print("     ");
  lcd.setCursor(11,0);
  char thing[20];
  sprintf(thing, "%d", adjust);
  lcd.print(thing);
  */
#endif
#if defined(READ_AC_SIGNAL_CENTERED) || defined(STRETCH_LOW_AMP_SIGNAL_SIMPLE)
  for (int i=0; i<SAMPLES_STORED; i++) {
  #ifdef READ_AC_SIGNAL_CENTERED
    samples[i] += adjust;
  #elif defined(STRETCH_LOW_AMP_SIGNAL_SIMPLE)
    samples[i] -= peak_low;
  #endif
  }
#endif
  boardDependantCheckMemory(1);
} // </ take_samples()


/* IMPORTANT */
/* QC1602A LCD display only allows the storage of 8 characters into DRAM at a time.
 *  writing a new character will update the 5x8 char location that referenced that char,
 *  and so the characters need to be deleted and coordinate cleared to prevent an incorrect waveform.
 *  This is why the funky character handling code and why only 4x2 are shown in full
 *  brightness, the other chars should faintly show a ghost of the lit up previous
 *  characters pixels.
 *  
 */
byte custom_char_rows[VERTICAL_WAVEFORM_SPACE]; // IMPORTANT see above
byte block_pixels[HORIZONTAL_WAVEFORM_SPACE]; // values of each column that represent the vertical pixel index
int n_pixel = 0;
int this_char_column = 0;
int bit_tbl[] = {16, 8, 4, 2, 1}; // bits of a byte's index that represent a lit pixel left to right
int shift_index = 11;
byte count_fast_displays = 0;
byte count_fast_out_of_range = 0;
byte waterfall_step = 1;
unsigned long waterfall_last_progress = curr_time;
byte character_was_missed = 0;
int display_chars() {
  
  lcd.setCursor(0,0);
  lcd.print(' '); lcd.print(' '); lcd.print(' ');
  lcd.print(' '); lcd.print(' '); lcd.print(' ');
  lcd.print(' '); lcd.print(' '); lcd.print(' ');
  lcd.print(' '); lcd.print(' ');
  lcd.setCursor(0,1);
  lcd.print(' '); lcd.print(' '); lcd.print(' ');
  lcd.print(' '); lcd.print(' '); lcd.print(' ');
  lcd.print(' '); lcd.print(' '); lcd.print(' ');
  lcd.print(' '); lcd.print(' ');

  short char_id = LCD_CHARACTER_LIMIT-1;

  // 0-start like horizontal 0th coordinate tiles.
  // This flag says to draw deeper into the waveform and expend the custom character ram per draw.
  // This means that the start of the waveform can be bright for more samples before faint/pulse drawing the end.
  byte is_a_0_start = shift_index == HORIZONTAL_WAVEFORM_CHARS-1 ? 1 : 0;

  if (curr_time - waterfall_last_progress > WATERFALL_LIMIT_PROGRESS_TIME) {
    waterfall_step = waterfall_step >= WATERFALL_SLOPE_SPREAD ? 1 : waterfall_step + 1;
    waterfall_last_progress = curr_time;
  }
  byte waterfall_on;

  // check to draw any missed tiles when character limit was hit.
  if (character_was_missed && !is_a_0_start) {
    // i.e. a quirk is that the 11,1 coordinate tile will be skipped if undrawn, very rare.
    shift_index = shift_index - 1;
  }

  
  // Create and set custom waveform chars
  for (int ch=0; ch<HORIZONTAL_WAVEFORM_CHARS; ch++) {
    shift_index = (shift_index + 1) % HORIZONTAL_WAVEFORM_CHARS;
    int pixel_offset= shift_index * HORIZONTAL_CHAR_SPACE;
    memset(custom_char_rows, 0x0, VERTICAL_WAVEFORM_SPACE);

    // process char-width pixels
    for (n_pixel=0; n_pixel<HORIZONTAL_CHAR_PIXELS; n_pixel++) {

      byte pixel_index = pixel_offset+n_pixel;
      int vertical_pixel_index = block_pixels[pixel_index];

      // check and flag for waterfalls
      if (vertical_pixel_index >= WATERFALL_DOWN_PIXEL_FLAG
          && pixel_index < HORIZONTAL_WAVEFORM_SPACE-1) 
      {
        waterfall_on = vertical_pixel_index >= WATERFALL_UP_PIXEL_FLAG ?
            WATERFALL_UP_PIXEL_FLAG : WATERFALL_DOWN_PIXEL_FLAG;
        vertical_pixel_index %= WATERFALL_DOWN_PIXEL_FLAG;
      } else {
        waterfall_on = 0;
      }

      // pixels at row for this column
      if (vertical_pixel_index >= VERTICAL_CHAR_PIXELS) {
        custom_char_rows[VERTICAL_FINAL_INDEX_ROW - vertical_pixel_index] += bit_tbl[n_pixel];
      } else /*if (vertical_pixel_index < VERTICAL_CHAR_PIXELS)*/ {
        custom_char_rows[VERTICAL_FINAL_INDEX_ROW - vertical_pixel_index] += bit_tbl[n_pixel];
      }

      // waterfall add pixels
      if (waterfall_on) {
        short limit_pixel = block_pixels[pixel_index+1] % WATERFALL_DOWN_PIXEL_FLAG;
        if (waterfall_on == WATERFALL_UP_PIXEL_FLAG) {
          for (short index = vertical_pixel_index-waterfall_step; index>=limit_pixel; index-=WATERFALL_SLOPE_SPREAD) {
            custom_char_rows[VERTICAL_FINAL_INDEX_ROW - index] = bit_tbl[n_pixel];
          }
        } else {
          for (short index = vertical_pixel_index+waterfall_step; index<=limit_pixel; index+=WATERFALL_SLOPE_SPREAD) {
            custom_char_rows[VERTICAL_FINAL_INDEX_ROW - index] = bit_tbl[n_pixel];
          }
        }
      }
    } // </ process char-width pixels

    // store and allocate tiles to custom chars  -  (when the custom char would not be empty)
    if (char_id >= 0 && *((uint64_t*)custom_char_rows) > 0 && !character_was_missed) {
      lcd.createChar(char_id, custom_char_rows);
      lcd.setCursor(shift_index,0);
      lcd.write(byte(char_id));
      char_id--;
      // if below true, then next draw we will set the skip_index back one, and (faintly) draw the
      //  tile below this char, it would not be a 0-start draw
      character_was_missed = char_id <= -1 ? 1 : 0;
    }
    if (char_id >= 0 && *((uint64_t*)custom_char_rows+1) > 0) {
      lcd.createChar(char_id, (byte*)((uint64_t*)custom_char_rows+1));
      lcd.setCursor(shift_index,1);
      lcd.write(byte(char_id));
      char_id--;
    }

    // exit once all chars used
    if (char_id < 0 || shift_index >= 10)
      break;
        
    character_was_missed = 0;
  }
  boardDependantCheckMemory(2);
  return !is_a_0_start;
}

void setup()
{
  //Serial.begin(9600);
  //Serial.print("github.com/yewchi/atmegaXX8-16x2osciloscope");
  lcd.begin(16,2);
}

byte no_update_pixels_count = 0;
void loop() {
  take_samples();

  // pixels-distance between pixels to do a waterfall  - (the analog read div is scaled to the pixels)
  short trigger_waterfall = analog_read_div * WATERFALL_TRIGGER_PIXELS;

  // Check update pixel skip and update pixels
  if (++no_update_pixels_count > REDRAW_NO_UPDATE) { 
    no_update_pixels_count = 0;
    
    memset(block_pixels, 0, HORIZONTAL_WAVEFORM_SPACE*sizeof(byte));
    
    // convert samples to pixels acording to scale dividors (zoom_div skips samples; analog_read_div stretchs vertically)
    for (int i=0; i<HORIZONTAL_WAVEFORM_SPACE*zoom_div; i+=zoom_div) {
      // Set pixel height for sample
      block_pixels[i/zoom_div] = max(0,
          min(VERTICAL_FINAL_INDEX_ROW, 
            (byte)(samples[i]/analog_read_div)
           )
          );
  
      // Set waterfall up or down for steep slopes
      if (i>0 && abs(samples[i] - samples[i+zoom_div]) > trigger_waterfall) {
        block_pixels[i/zoom_div] += samples[i] > samples[i+zoom_div]
            ? WATERFALL_UP_PIXEL_FLAG
            : WATERFALL_DOWN_PIXEL_FLAG;
      }
    }
  }

  byte brightness_as_delay_out_of_range =
      count_fast_out_of_range == 0 || count_fast_displays >= 24 ? 75 : 0;
  count_fast_out_of_range = (count_fast_out_of_range + 1) % 4;
  while(display_chars()) {
    delay(brightness_as_delay_out_of_range);
  }
  lcd.setCursor(11, 1);
  lcd.print(analog_read_div);
  delay(count_fast_displays >= 0 ? 0 : FREEZE_WAVE_DURATION);
  if (++count_fast_displays >= 26) {
    count_fast_displays = 0;
    //delay(50);
  }
  boardDependantCheckMemory(3);
}
