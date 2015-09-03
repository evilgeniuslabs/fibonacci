#include "FastLED.h"
FASTLED_USING_NAMESPACE;

#include "application.h"
#include "math.h"

// allow us to use itoa() in this scope
extern char* itoa(int a, char* buffer, unsigned char radix);

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
 
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)

#define DATA_PIN    A5
#define CLK_PIN     A3
// #define COLOR_ORDER RGB
#define LED_TYPE    WS2801
#define NUM_LEDS    100

#define NUM_VIRTUAL_LEDS 101
#define LAST_VISIBLE_LED 99

CRGB leds[NUM_VIRTUAL_LEDS];

typedef uint8_t (*SimplePattern)();
typedef SimplePattern SimplePatternList[];
typedef struct { SimplePattern drawFrame;  char name[32]; } PatternAndName;
typedef PatternAndName PatternAndNameList[];
 
// List of patterns to cycle through.  Each is defined as a separate function below.

const PatternAndNameList patterns = { 
  { pride,                  "Pride" },
  { colorWaves,             "Color Waves" },
  { horizontalRainbow,      "Horizontal Rainbow" },
  { verticalRainbow,        "Vertical Rainbow" },
  { diagonalRainbow,        "Diagonal Rainbow" },
  { noise,                  "Noise 1" },
  { yinYang,                "Yin & Yang" },
  { radialPaletteShift,     "Radial Palette Shift" },
  { verticalPaletteBlend,   "Vertical Palette Shift" },
  { horizontalPaletteBlend, "Horizontal Palette Shift" },
  { spiral1,                "Spiral 1" },
  { spiral2,                "Spiral 2" },
  { spiralPath1,            "Spiral Path 1" },
  { wave,                   "Wave" },
  { life,                   "Life" },
  { pulse,                  "Pulse" },
  { rainbow,                "Rainbow" },
  { rainbowWithGlitter,     "Rainbow With Glitter" },
  { confetti,               "Confetti" },
  { sinelon,                "Sinelon" },
  { bpm,                    "Beat" },
  { juggle,                 "Juggle" },
  { fire,                   "Fire" },
  { water,                  "Water" },
  { analogClock,            "Analog Clock" },
  { analogClockColor,       "Analog Clock Color" },
  { fastAnalogClock,        "Fast Analog Clock Test" },
  { showSolidColor,         "Solid Color" }
};

// variables exposed via Particle cloud API (Spark Core is limited to 10)
int brightness = 32;
int patternCount = ARRAY_SIZE(patterns);
int patternIndex = 0;
char patternName[32] = "Pride";
int power = 1;
int flipClock = 0;
int timezone = -6;
char variableValue[32] = "";
// ----------------------------------------------------------------------

// variables exposed via the variableValue variable, via Particle Cloud API
int r = 0;
int g = 0;
int b = 255;

int noiseSpeedX = 0; // 1 for a very slow moving effect, or 60 for something that ends up looking like water.
int noiseSpeedY = 0;
int noiseSpeedZ = 1;

int noiseScale = 1; // 1 will be so zoomed in, you'll mostly see solid colors.

// ------------------------------------------------------------------------

static uint16_t noiseX;
static uint16_t noiseY;
static uint16_t noiseZ;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGB solidColor = CRGB(r, g, b);

unsigned long lastTimeSync = millis();

int offlinePin = D7;

SYSTEM_MODE(SEMI_AUTOMATIC);

CRGBPalette16 IceColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

uint8_t paletteIndex = 0;

// List of palettes to cycle through.

CRGBPalette16 palettes[] = {
  RainbowColors_p,
  RainbowStripeColors_p,
  CloudColors_p,
  OceanColors_p,
  ForestColors_p,
  HeatColors_p,
  LavaColors_p,
  PartyColors_p,
  IceColors_p,
};

uint8_t paletteCount = ARRAY_SIZE(palettes);

CRGBPalette16 currentPalette(CRGB::Black);
CRGBPalette16 targetPalette = palettes[paletteIndex];

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
#define SECONDS_PER_PALETTE 20

void setup() {
    FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(brightness);
    FastLED.setDither(false);
    fill_solid(leds, NUM_LEDS, solidColor);
    FastLED.show();
    
    pinMode(offlinePin, INPUT_PULLUP);
    
    if(digitalRead(offlinePin) == HIGH) {
        Spark.connect();
    }
    
    // Serial.begin(9600);
    
    // load settings from EEPROM
    brightness = EEPROM.read(0);
    if(brightness < 1)
        brightness = 1;
    else if(brightness > 255)
        brightness = 255;
    
    FastLED.setBrightness(brightness);
    // FastLED.setDither(brightness < 255);
    
    uint8_t timezoneSign = EEPROM.read(1);
    if(timezoneSign < 1)
        timezone = -EEPROM.read(2);
    else
        timezone = EEPROM.read(2);
    
    if(timezone < -12)
        timezone = -12;
    else if (timezone > 14)
        timezone = 14;
    
    patternIndex = EEPROM.read(3);
    if(patternIndex < 0)
        patternIndex = 0;
    else if (patternIndex >= patternCount)
        patternIndex = patternCount - 1;
    
    flipClock = EEPROM.read(4);
    if(flipClock < 0)
        flipClock = 0;
    else if (flipClock > 1)
        flipClock = 1;
        
    r = EEPROM.read(5);
    g = EEPROM.read(6);
    b = EEPROM.read(7);
    
    if(r == 0 && g == 0 && b == 0) {
        r = 0;
        g = 0;
        b = 255;
    }
    
    solidColor = CRGB(r, b, g);
    
    Spark.function("patternIndex", setPatternIndex); // sets the current pattern index, changes to the pattern with the specified index
    Spark.function("pNameCursor", movePatternNameCursor); // moves the pattern name cursor to the specified index, allows getting a list of pattern names
    Spark.function("variable", setVariable); // sets the value of a variable, args are name:value
    Spark.function("varCursor", moveVariableCursor);
    
    Spark.variable("power", &power, INT);
    Spark.variable("brightness", &brightness, INT);
    Spark.variable("timezone", &timezone, INT);
    Spark.variable("flipClock", &flipClock, INT);
    Spark.variable("patternCount", &patternCount, INT);
    Spark.variable("patternIndex", &patternIndex, INT);
    Spark.variable("patternName", patternName, STRING);
    Spark.variable("variable", variableValue, STRING);
    
    Time.zone(timezone);
    
    noiseX = random16();
    noiseY = random16();
    noiseZ = random16();
}

void loop() {
    if (Spark.connected() && millis() - lastTimeSync > ONE_DAY_MILLIS) {
        // Request time synchronization from the Spark Cloud
        Spark.syncTime();
        lastTimeSync = millis();
    }
    
    if(power < 1) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        FastLED.delay(15);
        return;
    }
    
    uint8_t delay = patterns[patternIndex].drawFrame();
    
    // send the 'leds' array out to the actual LED strip
    FastLED.show();
    
    // insert a delay to keep the framerate modest
    FastLED.delay(delay); 
    
    // blend the current palette to the next
    EVERY_N_MILLISECONDS(40) {
        nblendPaletteTowardPalette(currentPalette, targetPalette, 16);
    }
    
    EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
    
    // slowly change to a new palette
    EVERY_N_SECONDS(SECONDS_PER_PALETTE) {
        paletteIndex++;
        if (paletteIndex >= paletteCount) paletteIndex = 0;
        targetPalette = palettes[paletteIndex];
    };
}

uint8_t variableCursor = 0;

int moveVariableCursor(String args)
{
    if(args.startsWith("pwr")) {
        itoa(power, variableValue, 10);
        return power;
    }
    else if (args.startsWith("brt")) {
        itoa(brightness, variableValue, 10);
        return brightness;
    }
    else if (args.startsWith("tz")) {
        itoa(timezone, variableValue, 10);
        return timezone;
    }
    else if (args.startsWith("flpclk")) {
        itoa(timezone, variableValue, 10);
        return flipClock;
    }
    else if (args.startsWith("r")) {
        itoa(r, variableValue, 10);
        return r;
    }
    else if (args.startsWith("g")) {
        itoa(g, variableValue, 10);
        return g;
    }
    else if (args.startsWith("b")) {
        itoa(b, variableValue, 10);
        return b;
    }
    else if (args.startsWith("nsx")) {
        itoa(noiseSpeedX, variableValue, 10);
        return noiseSpeedX;
    }
    else if (args.startsWith("nsy")) {
        itoa(noiseSpeedY, variableValue, 10);
        return noiseSpeedY;
    }
    else if (args.startsWith("nsz")) {
        itoa(noiseSpeedZ, variableValue, 10);
        return noiseSpeedZ;
    }
    else if (args.startsWith("nsc")) {
        itoa(b, variableValue, 10);
        return noiseScale;
    }
    
    return 0;
}

int setVariable(String args) {
    if(args.startsWith("pwr:")) {
        return setPower(args.substring(4));
    }
    else if (args.startsWith("brt:")) {
        return setBrightness(args.substring(4));
    }
    else if (args.startsWith("tz:")) {
        return setTimezone(args.substring(3));
    }
    else if (args.startsWith("flpclk:")) {
        return setFlipClock(args.substring(7));
    }
    else if (args.startsWith("r:")) {
        r = parseByte(args.substring(2));
        solidColor.r = r;
        EEPROM.write(5, r);
        patternIndex = patternCount - 1;
        return r;
    }
    else if (args.startsWith("g:")) {
        g = parseByte(args.substring(2));
        solidColor.g = g;
        EEPROM.write(6, g);
        patternIndex = patternCount - 1;
        return g;
    }
    else if (args.startsWith("b:")) {
        b = parseByte(args.substring(2));
        solidColor.b = b;
        EEPROM.write(7, b);
        patternIndex = patternCount - 1;
        return b;
    }
    else if (args.startsWith("nsx:")) {
        noiseSpeedX = args.substring(4).toInt();
        if(noiseSpeedX < 0)
            noiseSpeedX = 0;
        else if (noiseSpeedX > 65535)
            noiseSpeedX = 65535;
        return noiseSpeedX;
    }
    else if (args.startsWith("nsy:")) {
        noiseSpeedY = args.substring(4).toInt();
        if(noiseSpeedY < 0)
            noiseSpeedY = 0;
        else if (noiseSpeedY > 65535)
            noiseSpeedY = 65535;
        return noiseSpeedY;
    }
    else if (args.startsWith("nsz:")) {
        noiseSpeedZ = args.substring(4).toInt();
        if(noiseSpeedZ < 0)
            noiseSpeedZ = 0;
        else if (noiseSpeedZ > 65535)
            noiseSpeedZ = 65535;
        return noiseSpeedZ;
    }
    else if (args.startsWith("nsc:")) {
        noiseScale = args.substring(4).toInt();
        if(noiseScale < 0)
            noiseScale = 0;
        else if (noiseScale > 65535)
            noiseScale = 65535;
        return noiseScale;
    }
    
    return -1;
}

int setPower(String args) {
    power = args.toInt();
    if(power < 0)
        power = 0;
    else if (power > 1)
        power = 1;
    
    return power;
}

int setBrightness(String args)
{
    brightness = args.toInt();
    if(brightness < 1)
        brightness = 1;
    else if(brightness > 255)
        brightness = 255;
        
    FastLED.setBrightness(brightness);
    // FastLED.setDither(brightness < 255);
    
    EEPROM.write(0, brightness);
    
    return brightness;
}

int setTimezone(String args) {
    timezone = args.toInt();
    if(timezone < -12)
        power = -12;
    else if (power > 13)
        power = 13;
    
    Time.zone(timezone);
    
    if(timezone < 0)
        EEPROM.write(1, 0);
    else
        EEPROM.write(1, 1);
    
    EEPROM.write(2, abs(timezone));
    
    return power;
}

int setFlipClock(String args) {
    flipClock = args.toInt();
    if(flipClock < 0)
        flipClock = 0;
    else if (flipClock > 1)
        flipClock = 1;
    
    EEPROM.write(4, flipClock);
    
    return flipClock;
}

byte parseByte(String args) {
    int c = args.toInt();
    if(c < 0)
        c = 0;
    else if (c > 255)
        c = 255;
    
    return c;
}

int setPatternIndex(String args)
{
    patternIndex = args.toInt();
    if(patternIndex < 0)
        patternIndex = 0;
    else if (patternIndex >= patternCount)
        patternIndex = patternCount - 1;
        
    EEPROM.write(3, patternIndex);
    
    return patternIndex;
}

int movePatternNameCursor(String args)
{
    int index = args.toInt();
    if(index < 0)
        index = 0;
    else if (index >= patternCount)
        index = patternCount - 1;
    
    strcpy(patternName, patterns[index].name);
    
    return index;
}

uint8_t rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
  return 8;
}

uint8_t rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
  return 8;
}

uint8_t confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  return 8;
}

uint8_t sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
  return 8;
}

uint8_t bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(currentPalette, gHue+(i*2), beat-gHue+(i*10));
  }
  
  return 8;
}

uint8_t juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  
  return 8;
}

uint8_t fire() {
    heatMap(HeatColors_p, true);
    
    return 30;
}

uint8_t water() {
    heatMap(IceColors_p, false);
    
    return 30;
}

uint8_t analogClock() {
    dimAll(220);
    
    drawAnalogClock(Time.second(), Time.minute(), Time.hourFormat12(), true, true);
    
    return 8;
}

uint8_t analogClockColor() {
    fill_solid(leds, NUM_LEDS, solidColor);
    
    drawAnalogClock(Time.second(), Time.minute(), Time.hourFormat12(), false, true);
    
    return 8;
}

byte fastSecond = 0;
byte fastMinute = 0;
byte fastHour = 1;

uint8_t fastAnalogClock() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    drawAnalogClock(fastSecond, fastMinute, fastHour, false, false);
        
    fastMinute++;
    
    // fastSecond++;
    
    // if(fastSecond >= 60) {
    //     fastSecond = 0;
    //     fastMinute++;
    // }
     
    if(fastMinute >= 60) {
        fastMinute = 0;
        fastHour++;
    }
    
    if(fastHour >= 13) {
        fastHour = 1;
    }
        
    return 125;
}

uint8_t showSolidColor() {
    fill_solid(leds, NUM_LEDS, solidColor);
    
    return 30;
}

// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
uint8_t pride() 
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);
  
  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;
  
  for( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);
    
    CRGB newcolor = CHSV( hue8, sat8, bri8);
    
    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS-1) - pixelnumber;
    
    nblend( leds[pixelnumber], newcolor, 64);
  }
  
  return 15;
}

uint8_t radialPaletteShift()
{
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    // leds[i] = ColorFromPalette( currentPalette, gHue + sin8(i*16), brightness);
    leds[i] = ColorFromPalette(currentPalette, i + gHue, 255, LINEARBLEND);
  }

  return 8;
}

const uint8_t spiral1Count = 13;
const uint8_t spiral1Length = 7;

uint8_t spiral1Arms[spiral1Count][spiral1Length] = {
    { 0, 14, 27, 40, 53, 66, 84 },
    { 1, 15, 28, 41, 54, 67, 76 },
    { 2, 16, 29, 42, 55, 68, 85 },
    { 3, 17, 30, 43, 56, 77, 86 },
    { 4, 18, 31, 44, 57, 69, 78 },
    { 5, 19, 32, 45, 58, 70, 87 },
    { 6, 20, 33, 46, 59, 79, 94 },
    { 7, 21, 34, 47, 60, 71, 88 },
    { 8, 22, 35, 48, 61, 80, 89 },
    { 9, 23, 36, 49, 62, 72, 81 },
    { 10, 24, 37, 50, 63, 73, 82 },
    { 11, 25, 38, 51, 64, 74, 83 },
    { 12, 13, 26, 39, 52, 65, 75 }
};

const uint8_t spiral2Count = 21;
const uint8_t spiral2Length = 4;

uint8_t spiral2Arms[spiral2Count][spiral2Length] = {
    { 0, 26, 51, 73 },
    { 1, 14, 39, 64 },
    { 15, 27, 52, 74 },
    { 2, 28, 40, 65 },
    { 16, 41, 53, 75 },
    { 3, 29, 54, 66 },
    { 4, 17, 42, 67 },
    { 18, 30, 55, 76 },
    { 5, 31, 43, 68 },
    { 19, 44, 56, 85 },
    { 6, 32, 57, 77 },
    { 7, 20, 45, 69 },
    { 21, 33, 58, 78 },
    { 8, 34, 46, 70 },
    { 9, 22, 47, 59 },
    { 23, 35, 60, 79 },
    { 10, 36, 48, 71 },
    { 24, 49, 61, 88 },
    { 11, 37, 62, 80 },
    { 12, 25, 50, 72 },
    { 13, 38, 63, 81 }
};

template <size_t armCount, size_t armLength>
void fillSpiral(uint8_t (&spiral)[armCount][armLength], bool reverse, CRGBPalette16 palette) {
    fill_solid(leds, NUM_LEDS, ColorFromPalette(palette, 0, 255, LINEARBLEND));
    
    byte offset = 255 / armCount;
    static byte hue = 0;
    
    for(uint8_t i = 0; i < armCount; i++) {
        CRGB color;
        
        if(reverse)
            color = ColorFromPalette(palette, hue + offset, 255, LINEARBLEND);
        else
            color = ColorFromPalette(palette, hue - offset, 255, LINEARBLEND);
        
        for(uint8_t j = 0; j < armLength; j++) {
            leds[spiral[i][j]] = color;
        }
        
        if(reverse)
            offset += 255 / armCount;
        else
            offset -= 255 / armCount;
    }
    
    EVERY_N_MILLISECONDS( 20 )
    { 
        if(reverse)
            hue++;
        else
            hue--;
    }
}

uint8_t spiral1() {
    fillSpiral(spiral1Arms, false, currentPalette);
    
    return 0;
}

uint8_t spiral2() {
    fillSpiral(spiral2Arms, true, currentPalette);
    
    return 0;
}

CRGBPalette16 FireVsWaterColors_p = CRGBPalette16(
    CRGB::White, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
    CRGB::White, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue);

uint8_t yinYang()
{
    fillSpiral(spiral1Arms, false, FireVsWaterColors_p);
    
    return 0;
}

void spiralPath(uint8_t points[], uint8_t length)
{
    static uint8_t currentIndex = 0;
    
    dimAll(255);
    
    leds[points[currentIndex]] = ColorFromPalette(currentPalette, gHue, 255, LINEARBLEND);
    
    EVERY_N_MILLIS(20)
    {
        currentIndex++;
        
        if(currentIndex >= length)
            currentIndex = 0;
    };
}

uint8_t spiralPath1Arms[] = 
{
    11, 25, 38, 51, 64, 74, 83, 65, 40, 28, 2,
    16, 29, 42, 55, 68, 85, 56, 44, 19, 32, 45,
    58, 70, 87, 59, 47, 22, 9, 23, 36, 49, 62,
    72, 91, 63, 38, 13, 26, 39, 52, 65, 75, 91,
    66, 54, 29, 3, 17, 30, 43, 56, 77, 86, 69,
    45, 20, 7, 21, 34, 47, 60, 71, 88, 61, 49,
    24, 37, 50, 63, 73, 82, 64, 39, 14, 1, 15,
    28, 41, 54, 67, 76, 55, 30, 18, 31, 44, 57,
    69, 78, 93, 70, 46, 34, 8, 22, 35, 48, 61,
    80, 89, 90, 73, 51, 26, 0, 14, 27, 40, 53,
    66, 84, 97, 98, 77, 57, 32, 6, 20, 33, 46,
    59, 79, 94, 95, 80, 62, 37
};

const uint8_t spiralPath1Length = ARRAY_SIZE(spiralPath1Arms);

uint8_t spiralPath1() 
{
    spiralPath(spiralPath1Arms, spiralPath1Length);
    return 0;
}

// Params for width and height
const uint8_t kMatrixWidth = 10;
const uint8_t kMatrixHeight = 10;

uint8_t xyMap[kMatrixHeight][kMatrixWidth][2] = {
    { { 100, 100 }, { 100, 100 }, { 100, 100 }, { 21, 100 }, { 7, 20 }, { 6, 100 }, { 32, 100 }, { 19, 100 }, { 100, 100 }, { 100, 100 } },
    { { 100, 100 }, { 8, 100 }, { 34, 100 }, { 33, 46 }, { 58, 100 }, { 45, 100 }, { 57, 100 }, { 44, 100 }, { 5, 31 }, { 122, 100 } },
    { { 9, 100 }, { 22, 100 }, { 47, 100 }, { 59, 100 }, { 70, 100 }, { 69, 78 }, { 77, 100 }, { 56, 100 }, { 43, 100 }, { 18, 100 } },
    { { 133, 100 }, { 35, 100 }, { 60, 100 }, { 79, 100 }, { 87, 93 }, { 86, 100 }, { 85, 100 }, { 68, 100 }, { 141, 100 }, { 30, 4 } },
    { { 23, 100 }, { 48, 100 }, { 71, 100 }, { 94, 100 }, { 99, 100 }, { 98, 100 }, { 92, 100 }, { 76, 100 }, { 55, 100 }, { 17, 42 } },
    { { 10, 36 }, { 61, 100 }, { 80, 100 }, { 88, 100 }, { 95, 100 }, { 96, 97 }, { 91, 100 }, { 84, 100 }, { 67, 100 }, { 29, 3 } },
    { { 24, 100 }, { 49, 62 }, { 162, 100 }, { 89, 100 }, { 90, 100 }, { 166, 100 }, { 75, 83 }, { 66, 100 }, { 54, 100 }, { 16, 100 } },
    { { 173, 100 }, { 37, 100 }, { 50, 72 }, { 81, 100 }, { 73, 100 }, { 74, 82 }, { 65, 100 }, { 179, 100 }, { 41, 53 }, { 181, 100 } },
    { { 183, 100 }, { 11, 100 }, { 25, 100 }, { 63, 100 }, { 51, 100 }, { 64, 100 }, { 52, 100 }, { 40, 27 }, { 28, 100 }, { 2, 100 } },
    { { 193, 100 }, { 195, 100 }, { 12, 100 }, { 13, 38 }, { 26, 100 }, { 0, 39 }, { 14, 100 }, { 1, 27 }, { 15, 100 }, { 201, 100 } }
};

void setPixelXY(uint8_t x, uint8_t y, CRGB color)
{
    if((x >= kMatrixWidth) || (y >= kMatrixHeight)) {
        return;
    }
    
    for(uint8_t z = 0; z < 2; z++) {
        uint8_t i = xyMap[y][x][z];
        
        leds[i] = color;
    }
}

uint8_t horizontalPaletteBlend()
{
    uint8_t offset = 0;
    
    for(uint8_t x = 0; x <= kMatrixWidth; x++)
    {
        CRGB color = ColorFromPalette(currentPalette, gHue + offset, 255, LINEARBLEND);
        
        for(uint8_t y = 0; y <= kMatrixHeight; y++)
        {
            setPixelXY(x, y, color);
        }
        
        offset++;
    }
    
    return 15;
}

uint8_t verticalPaletteBlend()
{
    uint8_t offset = 0;
    
    for(uint8_t y = 0; y <= kMatrixHeight; y++)
    {
        CRGB color = ColorFromPalette(currentPalette, gHue + offset, 255, LINEARBLEND);
        
        for(uint8_t x = 0; x <= kMatrixWidth; x++)
        {
            setPixelXY(x, y, color);
        }
        
        offset++;
    }
    
    return 15;
}

const uint8_t coordsX[NUM_LEDS] = {
    135, 180, 236, 255, 253, 210, 143, 98, 37, 7,
    0, 32, 64, 93, 160, 200, 240, 243, 234, 182,
    116, 75, 28, 6, 15, 58, 120, 179, 214, 237,
    226, 210, 155, 95, 59, 27, 14, 36, 85, 142,
    191, 219, 227, 206, 186, 132, 81, 50, 32, 27,
    60, 109, 159, 195, 217, 212, 183, 162, 113, 74,
    50, 46, 46, 83, 128, 169, 191, 207, 193, 141,
    101, 57, 67, 104, 142, 169, 190, 161, 126, 76,
    64, 89, 122, 146, 176, 170, 142, 99, 73, 87,
    112, 155, 165, 119, 90, 100, 130, 137, 142, 116
};

const uint8_t coordsY[NUM_LEDS] = {
    255, 250, 201, 132, 86, 28, 3, 0, 37, 73,
    146, 209, 242, 246, 242, 229, 174, 106, 64, 21,
    12, 18, 65, 102, 171, 220, 239, 224, 204, 145,
    86, 50, 22, 28, 39, 92, 128, 187, 224, 228,
    202, 179, 122, 72, 43, 30, 47, 63, 115, 151,
    198, 220, 212, 178, 154, 104, 66, 43, 44, 69,
    87, 135, 167, 201, 208, 191, 156, 133, 94, 52,
    62, 109, 175, 195, 191, 169, 120, 70, 68, 91,
    148, 174, 180, 166, 138, 93, 84, 85, 125, 151,
    160, 144, 113, 94, 112, 132, 148, 127, 107, 116
};

CRGB scrollingHorizontalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis) {
    return CHSV( x + (timeInMillis / 100), 255, 255);
}

uint8_t horizontalRainbow() {
    unsigned long t = millis();
    
    for(uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = scrollingHorizontalWashColor(coordsX[i], coordsY[i], t);
    }
    
    return 0;
}

CRGB scrollingVerticalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis) {
    return CHSV( y + (timeInMillis / 100), 255, 255);
}

uint8_t verticalRainbow() {
    unsigned long t = millis();
    
    for(uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = scrollingVerticalWashColor(coordsX[i], coordsY[i], t);
    }
    
    return 0;
}

CRGB scrollingDiagonalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis) {
    return CHSV( x + y + (timeInMillis / 100), 255, 255);
}

uint8_t diagonalRainbow() {
    unsigned long t = millis();
    
    for(uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = scrollingDiagonalWashColor(coordsX[i], coordsY[i], t);
    }
    
    return 0;
}

uint8_t noise() {
    for(uint8_t i = 0; i < NUM_LEDS; i++) {
        uint8_t x = coordsX[i];
        uint8_t y = coordsY[i];
        
        int xoffset = noiseScale * x;
        int yoffset = noiseScale * y;
        
        uint8_t data = inoise8(x + xoffset + noiseX, y + yoffset + noiseY, noiseZ);
        
        // The range of the inoise8 function is roughly 16-238.
        // These two operations expand those values out to roughly 0..255
        // You can comment them out if you want the raw noise data.
        data = qsub8(data,16);
        data = qadd8(data,scale8(data,39));
        
        leds[i] = ColorFromPalette(currentPalette, data, 255, LINEARBLEND);
    }
    
    noiseX += noiseSpeedX;
    noiseY += noiseSpeedY;
    noiseZ += noiseSpeedZ;
  
    return 0;
}

const uint8_t maxX = kMatrixWidth - 1;
const uint8_t maxY = kMatrixHeight - 1;

uint8_t wave()
{
    const uint8_t scale = 256 / kMatrixWidth;

    static uint8_t rotation = 0;
    static uint8_t theta = 0;
    static uint8_t waveCount = 1;
    
    uint8_t n = 0;

    switch (rotation) {
        case 0:
            for (int x = 0; x < kMatrixWidth; x++) {
                n = quadwave8(x * 2 + theta) / scale;
                setPixelXY(x, n, ColorFromPalette(currentPalette, x + gHue, 255, LINEARBLEND));
                if (waveCount == 2)
                    setPixelXY(x, maxY - n, ColorFromPalette(currentPalette, x + gHue, 255, LINEARBLEND));
            }
            break;

        case 1:
            for (int y = 0; y < kMatrixHeight; y++) {
                n = quadwave8(y * 2 + theta) / scale;
                setPixelXY(n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
                if (waveCount == 2)
                    setPixelXY(maxX - n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
            }
            break;

        case 2:
            for (int x = 0; x < kMatrixWidth; x++) {
                n = quadwave8(x * 2 - theta) / scale;
                setPixelXY(x, n, ColorFromPalette(currentPalette, x + gHue));
                if (waveCount == 2)
                    setPixelXY(x, maxY - n, ColorFromPalette(currentPalette, x + gHue, 255, LINEARBLEND));
            }
            break;

        case 3:
            for (int y = 0; y < kMatrixHeight; y++) {
                n = quadwave8(y * 2 - theta) / scale;
                setPixelXY(n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
                if (waveCount == 2)
                    setPixelXY(maxX - n, y, ColorFromPalette(currentPalette, y + gHue, 255, LINEARBLEND));
            }
            break;
    }
    
    dimAll(254);

    EVERY_N_SECONDS(10) 
    {  
        rotation = random(0, 4);
        // waveCount = random(1, 3);
    };
    
    theta++;
    
    return 0;
}

uint8_t pulse()
{
    dimAll(200);

    uint8_t maxSteps = 16;
    static uint8_t step = maxSteps;
    static uint8_t centerX = 0;
    static uint8_t centerY = 0;
    float fadeRate = 0.8;

    if (step >= maxSteps)
    {
        centerX = random(kMatrixWidth);
        centerY = random(kMatrixWidth);
        step = 0;
    }
    
    if (step == 0)
    {
        drawCircle(centerX, centerY, step, ColorFromPalette(currentPalette, gHue, 255, LINEARBLEND));
        step++;
    }
    else
    {
        if (step < maxSteps)
        {
            // initial pulse
            drawCircle(centerX, centerY, step, ColorFromPalette(currentPalette, gHue, pow(fadeRate, step - 2) * 255, LINEARBLEND));
            
            // secondary pulse
            if (step > 3) {
                drawCircle(centerX, centerY, step - 3, ColorFromPalette(currentPalette, gHue, pow(fadeRate, step - 2) * 255, LINEARBLEND));
            }
        
            step++;
        }
        else
        {
            step = -1;
        }
    }
    
    return 30;
}

// algorithm from http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
void drawCircle(uint8_t x0, uint8_t y0, uint8_t radius, const CRGB color)
{
    int a = radius, b = 0;
    int radiusError = 1 - a;

    if (radius == 0) {
        setPixelXY(x0, y0, color);
        return;
    }

    while (a >= b)
    {
        setPixelXY(a + x0, b + y0, color);
        setPixelXY(b + x0, a + y0, color);
        setPixelXY(-a + x0, b + y0, color);
        setPixelXY(-b + x0, a + y0, color);
        setPixelXY(-a + x0, -b + y0, color);
        setPixelXY(-b + x0, -a + y0, color);
        setPixelXY(a + x0, -b + y0, color);
        setPixelXY(b + x0, -a + y0, color);

        b++;
        if (radiusError < 0)
            radiusError += 2 * b + 1;
        else
        {
            a--;
            radiusError += 2 * (b - a + 1);
        }
    }
}

class Cell
{
    public:
    byte alive : 1;
    byte prev : 1;
    byte hue: 6;  
    byte brightness;
};

Cell world[kMatrixWidth][kMatrixHeight];

uint8_t neighbors(uint8_t x, uint8_t y)
{
    return (world[(x + 1) % kMatrixWidth][y].prev) +
        (world[x][(y + 1) % kMatrixHeight].prev) +
        (world[(x + kMatrixWidth - 1) % kMatrixWidth][y].prev) +
        (world[x][(y + kMatrixHeight - 1) % kMatrixHeight].prev) +
        (world[(x + 1) % kMatrixWidth][(y + 1) % kMatrixHeight].prev) +
        (world[(x + kMatrixWidth - 1) % kMatrixWidth][(y + 1) % kMatrixHeight].prev) +
        (world[(x + kMatrixWidth - 1) % kMatrixWidth][(y + kMatrixHeight - 1) % kMatrixHeight].prev) +
        (world[(x + 1) % kMatrixWidth][(y + kMatrixHeight - 1) % kMatrixHeight].prev);
}
    
void randomFillWorld()
{
    static uint8_t lifeDensity = 10;

    for (uint8_t i = 0; i < kMatrixWidth; i++) {
        for (uint8_t j = 0; j < kMatrixHeight; j++) {
            if (random(100) < lifeDensity) {
                world[i][j].alive = 1;
                world[i][j].brightness = 255;
            }
            else {
                world[i][j].alive = 0;
                world[i][j].brightness = 0;
            }
            world[i][j].prev = world[i][j].alive;
            world[i][j].hue = 0;
        }
    }
}

uint8_t life()
{
    static uint8_t generation = 0;
    
    // Display current generation
    for (uint8_t i = 0; i < kMatrixWidth; i++)
    {
        for (uint8_t j = 0; j < kMatrixHeight; j++)
        {
            setPixelXY(i, j, ColorFromPalette(currentPalette, world[i][j].hue * 4, world[i][j].brightness, LINEARBLEND));
        }
    }
    
    uint8_t liveCells = 0;

    // Birth and death cycle
    for (uint8_t x = 0; x < kMatrixWidth; x++)
    {
        for (uint8_t y = 0; y < kMatrixHeight; y++)
        {
            // Default is for cell to stay the same
            if (world[x][y].brightness > 0 && world[x][y].prev == 0)
              world[x][y].brightness *= 0.5;
              
            uint8_t count = neighbors(x, y);
            
            if (count == 3 && world[x][y].prev == 0)
            {
                // A new cell is born
                world[x][y].alive = 1;
                world[x][y].hue += 2;
                world[x][y].brightness = 255;
            }
            else if ((count < 2 || count > 3) && world[x][y].prev == 1)
            {
                // Cell dies
                world[x][y].alive = 0;
            }
            
            if(world[x][y].alive)
                liveCells++;
        }
    }
    
    // Copy next generation into place
    for (uint8_t x = 0; x < kMatrixWidth; x++)
    {
        for (uint8_t y = 0; y < kMatrixHeight; y++)
        {
            world[x][y].prev = world[x][y].alive;
        }
    }

    if (liveCells < 4 || generation >= 128)
    {
        fill_solid(leds, NUM_LEDS, CRGB::Black);

        randomFillWorld();
        
        generation = 0;
    }
    else
    {
        generation++;
    }

    return 60;
}

void heatMap(CRGBPalette16 palette, bool up) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(256));
    
    uint8_t cooling = 55;
    uint8_t sparking = 120;
    
    // Array of temperature readings at each simulation cell
    static byte heat[kMatrixWidth][kMatrixHeight];
    
    for (int x = 0; x < kMatrixWidth; x++)
    {
        // Step 1.  Cool down every cell a little
        for (int y = 0; y < kMatrixHeight; y++)
        {
            heat[x][y] = qsub8(heat[x][y], random8(0, ((cooling * 10) / kMatrixHeight) + 2));
        }
        
        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int y = 0; y < kMatrixHeight; y++)
        {
            heat[x][y] = (heat[x][y + 1] + heat[x][y + 2] + heat[x][y + 2]) / 3;
        }
        
        // Step 2.  Randomly ignite new 'sparks' of heat
        if (random8() < sparking)
        {
            heat[x][kMatrixHeight - 1] = qadd8(heat[x][kMatrixHeight - 1], random8(160, 255));
        }
        
        // Step 4.  Map from heat cells to LED colors
        for (int y = 0; y < kMatrixHeight; y++)
        {
            uint8_t colorIndex = heat[x][y];
            
            // Recommend that you use values 0-240 rather than
            // the usual 0-255, as the last 15 colors will be
            // 'wrapping around' from the hot end to the cold end,
            // which looks wrong.
            colorIndex = scale8(colorIndex, 240);
            
            // override color 0 to ensure a black background
            if (colorIndex != 0)
            {
                setPixelXY(x, y, ColorFromPalette(palette, colorIndex, 255, LINEARBLEND));
            }
        }
    }
}

int oldSecTime = 0;
int oldSec = 0;

void drawAnalogClock(byte second, byte minute, byte hour, boolean drawMillis, boolean drawSecond) {
    if(Time.second() != oldSec){
        oldSecTime = millis();
        oldSec = Time.second();
    }
    
    int millisecond = millis() - oldSecTime;
    
    int secondIndex = map(second, 0, 59, 0, NUM_LEDS);
    int minuteIndex = map(minute, 0, 59, 0, NUM_LEDS);
    int hourIndex = map(hour * 5, 5, 60, 0, NUM_LEDS);
    int millisecondIndex = map(secondIndex + millisecond * .06, 0, 60, 0, NUM_LEDS);
    
    if(millisecondIndex >= NUM_LEDS)
        millisecondIndex -= NUM_LEDS;
    
    hourIndex += minuteIndex / 12;
    
    if(hourIndex >= NUM_LEDS)
        hourIndex -= NUM_LEDS;
    
    // see if we need to reverse the order of the LEDS
    if(flipClock == 1) {
        int max = NUM_LEDS - 1;
        secondIndex = max - secondIndex;
        minuteIndex = max - minuteIndex;
        hourIndex = max - hourIndex;
        millisecondIndex = max - millisecondIndex;
    }
    
    if(secondIndex >= NUM_LEDS)
        secondIndex = NUM_LEDS - 1;
    else if(secondIndex < 0)
        secondIndex = 0;
    
    if(minuteIndex >= NUM_LEDS)
        minuteIndex = NUM_LEDS - 1;
    else if(minuteIndex < 0)
        minuteIndex = 0;
        
    if(hourIndex >= NUM_LEDS)
        hourIndex = NUM_LEDS - 1;
    else if(hourIndex < 0)
        hourIndex = 0;
        
    if(millisecondIndex >= NUM_LEDS)
        millisecondIndex = NUM_LEDS - 1;
    else if(millisecondIndex < 0)
        millisecondIndex = 0;
    
    if(drawMillis)
        leds[millisecondIndex] += CRGB(0, 0, 127); // Blue
        
    if(drawSecond)
        leds[secondIndex] += CRGB(0, 0, 127); // Blue
        
    leds[minuteIndex] += CRGB::Green;
    leds[hourIndex] += CRGB::Red;
}

// scale the brightness of all pixels down
void dimAll(byte value)
{
  for (int i = 0; i < NUM_LEDS; i++){
    leds[i].nscale8(value);
  }
}

void addGlitter( uint8_t chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and 
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Current palette number from the 'playlist' of color palettes
uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );


uint8_t colorWaves()
{
  EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
    gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
    gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
  }

  EVERY_N_MILLISECONDS(40) {
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16);
  }
  
  colorwaves( leds, NUM_LEDS, gCurrentPalette);

  return 20;
}


// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette) 
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);
  
  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;
  
  for( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);
    
    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (numleds-1) - pixelnumber;
    
    nblend( ledarray[pixelnumber], newcolor, 128);
  }
}

// Alternate rendering function just scrolls the current palette 
// across the defined LED strip.
void palettetest( CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}





// Gradient Color Palette definitions for 33 different cpt-city color palettes.
//    956 bytes of PROGMEM for all of the palettes together,
//   +618 bytes of PROGMEM for gradient palette code (AVR).
//  1,494 bytes total for all 34 color palettes and associated code.

// Gradient palette "ib_jul01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ing/xmas/tn/ib_jul01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

DEFINE_GRADIENT_PALETTE( ib_jul01_gp ) {
    0, 194,  1,  1,
   94,   1, 29, 18,
  132,  57,131, 28,
  255, 113,  1,  1};

// Gradient palette "es_vintage_57_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/vintage/tn/es_vintage_57.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_vintage_57_gp ) {
    0,   2,  1,  1,
   53,  18,  1,  0,
  104,  69, 29,  1,
  153, 167,135, 10,
  255,  46, 56,  4};

// Gradient palette "es_vintage_01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/vintage/tn/es_vintage_01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_vintage_01_gp ) {
    0,   4,  1,  1,
   51,  16,  0,  1,
   76,  97,104,  3,
  101, 255,131, 19,
  127,  67,  9,  4,
  153,  16,  0,  1,
  229,   4,  1,  1,
  255,   4,  1,  1};

// Gradient palette "es_rivendell_15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/rivendell/tn/es_rivendell_15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_rivendell_15_gp ) {
    0,   1, 14,  5,
  101,  16, 36, 14,
  165,  56, 68, 30,
  242, 150,156, 99,
  255, 150,156, 99};

// Gradient palette "rgi_15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ds/rgi/tn/rgi_15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_GRADIENT_PALETTE( rgi_15_gp ) {
    0,   4,  1, 31,
   31,  55,  1, 16,
   63, 197,  3,  7,
   95,  59,  2, 17,
  127,   6,  2, 34,
  159,  39,  6, 33,
  191, 112, 13, 32,
  223,  56,  9, 35,
  255,  22,  6, 38};

// Gradient palette "retro2_16_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ma/retro2/tn/retro2_16.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 8 bytes of program space.

DEFINE_GRADIENT_PALETTE( retro2_16_gp ) {
    0, 188,135,  1,
  255,  46,  7,  1};

// Gradient palette "Analogous_1_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/red/tn/Analogous_1.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_GRADIENT_PALETTE( Analogous_1_gp ) {
    0,   3,  0,255,
   63,  23,  0,255,
  127,  67,  0,255,
  191, 142,  0, 45,
  255, 255,  0,  0};

// Gradient palette "es_pinksplash_08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/pink_splash/tn/es_pinksplash_08.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_pinksplash_08_gp ) {
    0, 126, 11,255,
  127, 197,  1, 22,
  175, 210,157,172,
  221, 157,  3,112,
  255, 157,  3,112};

// Gradient palette "es_pinksplash_07_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/pink_splash/tn/es_pinksplash_07.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_pinksplash_07_gp ) {
    0, 229,  1,  1,
   61, 242,  4, 63,
  101, 255, 12,255,
  127, 249, 81,252,
  153, 255, 11,235,
  193, 244,  5, 68,
  255, 232,  1,  5};

// Gradient palette "Coral_reef_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/other/tn/Coral_reef.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_GRADIENT_PALETTE( Coral_reef_gp ) {
    0,  40,199,197,
   50,  10,152,155,
   96,   1,111,120,
   96,  43,127,162,
  139,  10, 73,111,
  255,   1, 34, 71};

// Gradient palette "es_ocean_breeze_068_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_068.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_ocean_breeze_068_gp ) {
    0, 100,156,153,
   51,   1, 99,137,
  101,   1, 68, 84,
  104,  35,142,168,
  178,   0, 63,117,
  255,   1, 10, 10};

// Gradient palette "es_ocean_breeze_036_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_036.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_ocean_breeze_036_gp ) {
    0,   1,  6,  7,
   89,   1, 99,111,
  153, 144,209,255,
  255,   0, 73, 82};

// Gradient palette "departure_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/mjf/tn/departure.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 88 bytes of program space.

DEFINE_GRADIENT_PALETTE( departure_gp ) {
    0,   8,  3,  0,
   42,  23,  7,  0,
   63,  75, 38,  6,
   84, 169, 99, 38,
  106, 213,169,119,
  116, 255,255,255,
  138, 135,255,138,
  148,  22,255, 24,
  170,   0,255,  0,
  191,   0,136,  0,
  212,   0, 55,  0,
  255,   0, 55,  0};

// Gradient palette "es_landscape_64_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_64.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 36 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_landscape_64_gp ) {
    0,   0,  0,  0,
   37,   2, 25,  1,
   76,  15,115,  5,
  127,  79,213,  1,
  128, 126,211, 47,
  130, 188,209,247,
  153, 144,182,205,
  204,  59,117,250,
  255,   1, 37,192};

// Gradient palette "es_landscape_33_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_33.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_landscape_33_gp ) {
    0,   1,  5,  0,
   19,  32, 23,  1,
   38, 161, 55,  1,
   63, 229,144,  1,
   66,  39,142, 74,
  255,   1,  4,  1};

// Gradient palette "rainbowsherbet_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ma/icecream/tn/rainbowsherbet.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( rainbowsherbet_gp ) {
    0, 255, 33,  4,
   43, 255, 68, 25,
   86, 255,  7, 25,
  127, 255, 82,103,
  170, 255,255,242,
  209,  42,255, 22,
  255,  87,255, 65};

// Gradient palette "gr65_hult_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/hult/tn/gr65_hult.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_GRADIENT_PALETTE( gr65_hult_gp ) {
    0, 247,176,247,
   48, 255,136,255,
   89, 220, 29,226,
  160,   7, 82,178,
  216,   1,124,109,
  255,   1,124,109};

// Gradient palette "gr64_hult_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/hult/tn/gr64_hult.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_GRADIENT_PALETTE( gr64_hult_gp ) {
    0,   1,124,109,
   66,   1, 93, 79,
  104,  52, 65,  1,
  130, 115,127,  1,
  150,  52, 65,  1,
  201,   1, 86, 72,
  239,   0, 55, 45,
  255,   0, 55, 45};

// Gradient palette "GMT_drywet_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gmt/tn/GMT_drywet.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( GMT_drywet_gp ) {
    0,  47, 30,  2,
   42, 213,147, 24,
   84, 103,219, 52,
  127,   3,219,207,
  170,   1, 48,214,
  212,   1,  1,111,
  255,   1,  7, 33};

// Gradient palette "ib15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ing/general/tn/ib15.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 24 bytes of program space.

DEFINE_GRADIENT_PALETTE( ib15_gp ) {
    0, 113, 91,147,
   72, 157, 88, 78,
   89, 208, 85, 33,
  107, 255, 29, 11,
  141, 137, 31, 39,
  255,  59, 33, 89};

// Gradient palette "Fuschia_7_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ds/fuschia/tn/Fuschia-7.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_GRADIENT_PALETTE( Fuschia_7_gp ) {
    0,  43,  3,153,
   63, 100,  4,103,
  127, 188,  5, 66,
  191, 161, 11,115,
  255, 135, 20,182};

// Gradient palette "es_emerald_dragon_08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/emerald_dragon/tn/es_emerald_dragon_08.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_emerald_dragon_08_gp ) {
    0,  97,255,  1,
  101,  47,133,  1,
  178,  13, 43,  1,
  255,   2, 10,  1};

// Gradient palette "lava_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/lava.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_GRADIENT_PALETTE( lava_gp ) {
    0,   0,  0,  0,
   46,  18,  0,  0,
   96, 113,  0,  0,
  108, 142,  3,  1,
  119, 175, 17,  1,
  146, 213, 44,  2,
  174, 255, 82,  4,
  188, 255,115,  4,
  202, 255,156,  4,
  218, 255,203,  4,
  234, 255,255,  4,
  244, 255,255, 71,
  255, 255,255,255};

// Gradient palette "fire_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/fire.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( fire_gp ) {
    0,   1,  1,  0,
   76,  32,  5,  0,
  146, 192, 24,  0,
  197, 220,105,  5,
  240, 252,255, 31,
  250, 252,255,111,
  255, 255,255,255};

// Gradient palette "Colorfull_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Colorfull.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_GRADIENT_PALETTE( Colorfull_gp ) {
    0,  10, 85,  5,
   25,  29,109, 18,
   60,  59,138, 42,
   93,  83, 99, 52,
  106, 110, 66, 64,
  109, 123, 49, 65,
  113, 139, 35, 66,
  116, 192,117, 98,
  124, 255,255,137,
  168, 100,180,155,
  255,  22,121,174};

// Gradient palette "Magenta_Evening_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Magenta_Evening.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( Magenta_Evening_gp ) {
    0,  71, 27, 39,
   31, 130, 11, 51,
   63, 213,  2, 64,
   70, 232,  1, 66,
   76, 252,  1, 69,
  108, 123,  2, 51,
  255,  46,  9, 35};

// Gradient palette "Pink_Purple_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Pink_Purple.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 44 bytes of program space.

DEFINE_GRADIENT_PALETTE( Pink_Purple_gp ) {
    0,  19,  2, 39,
   25,  26,  4, 45,
   51,  33,  6, 52,
   76,  68, 62,125,
  102, 118,187,240,
  109, 163,215,247,
  114, 217,244,255,
  122, 159,149,221,
  149, 113, 78,188,
  183, 128, 57,155,
  255, 146, 40,123};

// Gradient palette "Sunset_Real_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Sunset_Real.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( Sunset_Real_gp ) {
    0, 120,  0,  0,
   22, 179, 22,  0,
   51, 255,104,  0,
   85, 167, 22, 18,
  135, 100,  0,103,
  198,  16,  0,130,
  255,   0,  0,160};

// Gradient palette "es_autumn_19_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/autumn/tn/es_autumn_19.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_GRADIENT_PALETTE( es_autumn_19_gp ) {
    0,  26,  1,  1,
   51,  67,  4,  1,
   84, 118, 14,  1,
  104, 137,152, 52,
  112, 113, 65,  1,
  122, 133,149, 59,
  124, 137,152, 52,
  135, 113, 65,  1,
  142, 139,154, 46,
  163, 113, 13,  1,
  204,  55,  3,  1,
  249,  17,  1,  1,
  255,  17,  1,  1};

// Gradient palette "BlacK_Blue_Magenta_White_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/BlacK_Blue_Magenta_White.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( BlacK_Blue_Magenta_White_gp ) {
    0,   0,  0,  0,
   42,   0,  0, 45,
   84,   0,  0,255,
  127,  42,  0,255,
  170, 255,  0,255,
  212, 255, 55,255,
  255, 255,255,255};

// Gradient palette "BlacK_Magenta_Red_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/BlacK_Magenta_Red.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_GRADIENT_PALETTE( BlacK_Magenta_Red_gp ) {
    0,   0,  0,  0,
   63,  42,  0, 45,
  127, 255,  0,255,
  191, 255,  0, 45,
  255, 255,  0,  0};

// Gradient palette "BlacK_Red_Magenta_Yellow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/BlacK_Red_Magenta_Yellow.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( BlacK_Red_Magenta_Yellow_gp ) {
    0,   0,  0,  0,
   42,  42,  0,  0,
   84, 255,  0,  0,
  127, 255,  0, 45,
  170, 255,  0,255,
  212, 255, 55, 45,
  255, 255,255,  0};

// Gradient palette "Blue_Cyan_Yellow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/Blue_Cyan_Yellow.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 20 bytes of program space.

DEFINE_GRADIENT_PALETTE( Blue_Cyan_Yellow_gp ) {
    0,   0,  0,255,
   63,   0, 55,255,
  127,   0,255,255,
  191,  42,255, 45,
  255, 255,255,  0};


// Single array of defined cpt-city color palettes.
// This will let us programmatically choose one based on
// a number, rather than having to activate each explicitly 
// by name every time.
// Since it is const, this array could also be moved 
// into PROGMEM to save SRAM, but for simplicity of illustration
// we'll keep it in a regular SRAM array.
//
// This list of color palettes acts as a "playlist"; you can
// add or delete, or re-arrange as you wish.
const TProgmemRGBGradientPalettePtr gGradientPalettes[] = {
  Sunset_Real_gp,
  es_rivendell_15_gp,
  es_ocean_breeze_036_gp,
  rgi_15_gp,
  retro2_16_gp,
  Analogous_1_gp,
  es_pinksplash_08_gp,
  Coral_reef_gp,
  es_ocean_breeze_068_gp,
  es_pinksplash_07_gp,
  es_vintage_01_gp,
  departure_gp,
  es_landscape_64_gp,
  es_landscape_33_gp,
  rainbowsherbet_gp,
  gr65_hult_gp,
  gr64_hult_gp,
  GMT_drywet_gp,
  ib_jul01_gp,
  es_vintage_57_gp,
  ib15_gp,
  Fuschia_7_gp,
  es_emerald_dragon_08_gp,
  lava_gp,
  fire_gp,
  Colorfull_gp,
  Magenta_Evening_gp,
  Pink_Purple_gp,
  es_autumn_19_gp,
  BlacK_Blue_Magenta_White_gp,
  BlacK_Magenta_Red_gp,
  BlacK_Red_Magenta_Yellow_gp,
  Blue_Cyan_Yellow_gp };


// Count of how many cpt-city gradients are defined:
const uint8_t gGradientPaletteCount = 
  sizeof( gGradientPalettes) / sizeof( TProgmemRGBGradientPalettePtr );