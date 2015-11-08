#include "FastLED.h"
FASTLED_USING_NAMESPACE;

#include "GradientPalettes.h"

#include "application.h"
#include "math.h"

SYSTEM_MODE(SEMI_AUTOMATIC);

int offlinePin = D7;

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
typedef struct { SimplePattern drawFrame;  String name; } PatternAndName;
typedef PatternAndName PatternAndNameList[];

// List of patterns to cycle through.  Each is defined as a separate function below.

const PatternAndNameList patterns = {
  { pride,                  "Pride" },
  { colorWaves,             "Color Waves" },
  { rainbowTwinkles,        "Rainbow Twinkles" },
  { snowTwinkles,           "Snow Twinkles" },
  { cloudTwinkles,          "Cloud Twinkles" },
  { incandescentTwinkles,   "Incandescent Twinkles" },
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
  { rainbowSolid,           "Solid Rainbow" },
  { confetti,               "Confetti" },
  { sinelon,                "Sinelon" },
  { bpm,                    "Beat" },
  { juggle,                 "Juggle" },
  { fire,                   "Fire" },
  { water,                  "Water" },
  { showSolidColor,         "Solid Color" }
};

int patternCount = ARRAY_SIZE(patterns);

// variables exposed via Particle cloud API (Spark Core is limited to 10)
int brightness = 32;
int patternIndex = 0;
String patternNames = "";
int power = 1;
int r = 0;
int g = 0;
int b = 255;

// variables exposed via the variableValue variable, via Particle Cloud API
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

CRGBPalette16 IceColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

uint8_t paletteIndex = 0;

// List of palettes to cycle through.
CRGBPalette16 palettes[] =
{
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

void setup()
{
    FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN>(leds, NUM_LEDS);
    FastLED.setCorrection(Typical8mmPixel);
    FastLED.setBrightness(brightness);
    FastLED.setDither(false);
    fill_solid(leds, NUM_LEDS, solidColor);
    FastLED.show();

    pinMode(offlinePin, INPUT_PULLUP);

    if(digitalRead(offlinePin) == HIGH) {
        Particle.connect();
    }

    // Serial.begin(9600);

    // load settings from EEPROM
    brightness = EEPROM.read(0);
    if(brightness < 1)
        brightness = 1;
    else if(brightness > 255)
        brightness = 255;

    FastLED.setBrightness(brightness);
    FastLED.setDither(brightness < 255);

    patternIndex = EEPROM.read(1);
    if(patternIndex < 0)
        patternIndex = 0;
    else if (patternIndex >= patternCount)
        patternIndex = patternCount - 1;

    r = EEPROM.read(2);
    g = EEPROM.read(3);
    b = EEPROM.read(4);

    if(r == 0 && g == 0 && b == 0) {
        r = 0;
        g = 0;
        b = 255;
    }

    solidColor = CRGB(r, b, g);

    Particle.function("patternIndex", setPatternIndex); // sets the current pattern index, changes to the pattern with the specified index
    Particle.function("variable", setVariable); // sets the value of a variable, args are name:value
    Particle.function("varCursor", moveVariableCursor);

    Particle.variable("power", power);
    Particle.variable("brightness", brightness);
    Particle.variable("patternIndex", patternIndex);
    Particle.variable("r", r);
    Particle.variable("g", g);
    Particle.variable("b", b);
    Particle.variable("variable", variableValue);

    patternNames = "[";
    for(uint8_t i = 0; i < patternCount; i++)
    {
      patternNames.concat("\"");
      patternNames.concat(patterns[i].name);
      patternNames.concat("\"");
      if(i < patternCount - 1)
        patternNames.concat(",");
    }
    patternNames.concat("]");
    Particle.variable("patternNames", patternNames);

    noiseX = random16();
    noiseY = random16();
    noiseZ = random16();
}

void loop()
{
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

    EVERY_N_MILLISECONDS( 40 ) { gHue++; } // slowly cycle the "base color" through the rainbow

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
    else if (args.startsWith("r:")) {
        r = parseByte(args.substring(2));
        solidColor.r = r;
        EEPROM.write(2, r);
        patternIndex = patternCount - 1;
        return r;
    }
    else if (args.startsWith("g:")) {
        g = parseByte(args.substring(2));
        solidColor.g = g;
        EEPROM.write(3, g);
        patternIndex = patternCount - 1;
        return g;
    }
    else if (args.startsWith("b:")) {
        b = parseByte(args.substring(2));
        solidColor.b = b;
        EEPROM.write(4, b);
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

int setPower(String args)
{
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
    FastLED.setDither(brightness < 255);

    EEPROM.write(0, brightness);

    return brightness;
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

    EEPROM.write(1, patternIndex);

    return patternIndex;
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

uint8_t rainbowSolid()
{
  fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
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

uint8_t juggle()
{
  static uint8_t    numdots =   4; // Number of dots in use.
  static uint8_t   faderate =   2; // How long should the trails be. Very low value = longer trails.
  static uint8_t     hueinc =  255 / numdots - 1; // Incremental change in hue between each dot.
  static uint8_t    thishue =   0; // Starting hue.
  static uint8_t     curhue =   0; // The current hue
  static uint8_t    thissat = 255; // Saturation of the colour.
  static uint8_t thisbright = 255; // How bright should the LED/display be.
  static uint8_t   basebeat =   5; // Higher = faster movement.

  static uint8_t lastSecond =  99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
  uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

  if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch(secondHand) {
      case  0: numdots = 1; basebeat = 20; hueinc = 16; faderate = 2; thishue = 0; break; // You can change values here, one at a time , or altogether.
      case 10: numdots = 4; basebeat = 10; hueinc = 16; faderate = 8; thishue = 128; break;
      case 20: numdots = 8; basebeat =  3; hueinc =  0; faderate = 8; thishue=random8(); break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 30: break;
    }
  }

  // Several colored dots, weaving in and out of sync with each other
  curhue = thishue; // Reset the hue values.
  fadeToBlackBy(leds, NUM_LEDS, faderate);
  for( int i = 0; i < numdots; i++) {
    //beat16 is a FastLED 3.1 function
    leds[beatsin16(basebeat+i+numdots,0,NUM_LEDS)] += CHSV(gHue + curhue, thissat, thisbright);
    curhue += hueinc;
  }

  return 0;
}

uint8_t fire()
{
    heatMap(HeatColors_p, true);

    return 30;
}

uint8_t water()
{
    heatMap(IceColors_p, false);

    return 30;
}

uint8_t showSolidColor()
{
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

uint8_t spiral1Arms[spiral1Count][spiral1Length] =
{
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

uint8_t spiral2Arms[spiral2Count][spiral2Length] =
{
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
void fillSpiral(uint8_t (&spiral)[armCount][armLength], bool reverse, CRGBPalette16 palette)
{
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

uint8_t spiral1()
{
    fillSpiral(spiral1Arms, false, currentPalette);

    return 0;
}

uint8_t spiral2()
{
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

const uint8_t coordsX[NUM_LEDS] =
{
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

const uint8_t coordsY[NUM_LEDS] =
{
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

CRGB scrollingHorizontalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis)
{
    return CHSV( x + (timeInMillis / 100), 255, 255);
}

uint8_t horizontalRainbow()
{
    unsigned long t = millis();

    for(uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = scrollingHorizontalWashColor(coordsX[i], coordsY[i], t);
    }

    return 0;
}

CRGB scrollingVerticalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis)
{
    return CHSV( y + (timeInMillis / 100), 255, 255);
}

uint8_t verticalRainbow()
{
    unsigned long t = millis();

    for(uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = scrollingVerticalWashColor(coordsX[i], coordsY[i], t);
    }

    return 0;
}

CRGB scrollingDiagonalWashColor( uint8_t x, uint8_t y, unsigned long timeInMillis)
{
    return CHSV( x + y + (timeInMillis / 100), 255, 255);
}

uint8_t diagonalRainbow()
{
    unsigned long t = millis();

    for(uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = scrollingDiagonalWashColor(coordsX[i], coordsY[i], t);
    }

    return 0;
}

uint8_t noise()
{
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
    byte alive = 1;
    byte prev = 1;
    byte hue = 6;
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

void heatMap(CRGBPalette16 palette, bool up)
{
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(256));

    uint8_t cooling = 55;
    uint8_t sparking = 120;

    // Array of temperature readings at each simulation cell
    static byte heat[kMatrixWidth + 3][kMatrixHeight + 3];

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

  // uint8_t sat8 = beatsin88( 87, 220, 250);
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

uint8_t cloudTwinkles()
{
  gCurrentPalette = CloudColors_p; // Blues and whites!
  colortwinkles();
  return 20;
}

uint8_t rainbowTwinkles()
{
  gCurrentPalette = RainbowColors_p; // Blues and whites!
  colortwinkles();
  return 20;
}

uint8_t snowTwinkles()
{
  CRGB w(85,85,85), W(CRGB::White);

  gCurrentPalette = CRGBPalette16( W,W,W,W, w,w,w,w, w,w,w,w, w,w,w,w );
  colortwinkles();
  return 20;
}

uint8_t incandescentTwinkles()
{
  CRGB l(0xE1A024);

  gCurrentPalette = CRGBPalette16( l,l,l,l, l,l,l,l, l,l,l,l, l,l,l,l );
  colortwinkles();
  return 20;
}

#define STARTING_BRIGHTNESS 64
#define FADE_IN_SPEED       32
#define FADE_OUT_SPEED      20
#define DENSITY            255

enum { GETTING_DARKER = 0, GETTING_BRIGHTER = 1 };

void colortwinkles()
{
  // Make each pixel brighter or darker, depending on
  // its 'direction' flag.
  brightenOrDarkenEachPixel( FADE_IN_SPEED, FADE_OUT_SPEED);

  // Now consider adding a new random twinkle
  if( random8() < DENSITY ) {
    int pos = random16(NUM_LEDS);
    if( !leds[pos]) {
      leds[pos] = ColorFromPalette( gCurrentPalette, random8(), STARTING_BRIGHTNESS, NOBLEND);
      setPixelDirection(pos, GETTING_BRIGHTER);
    }
  }
}

void brightenOrDarkenEachPixel( fract8 fadeUpAmount, fract8 fadeDownAmount)
{
 for( uint16_t i = 0; i < NUM_LEDS; i++) {
    if( getPixelDirection(i) == GETTING_DARKER) {
      // This pixel is getting darker
      leds[i] = makeDarker( leds[i], fadeDownAmount);
    } else {
      // This pixel is getting brighter
      leds[i] = makeBrighter( leds[i], fadeUpAmount);
      // now check to see if we've maxxed out the brightness
      if( leds[i].r == 255 || leds[i].g == 255 || leds[i].b == 255) {
        // if so, turn around and start getting darker
        setPixelDirection(i, GETTING_DARKER);
      }
    }
  }
}

CRGB makeBrighter( const CRGB& color, fract8 howMuchBrighter)
{
  CRGB incrementalColor = color;
  incrementalColor.nscale8( howMuchBrighter);
  return color + incrementalColor;
}

CRGB makeDarker( const CRGB& color, fract8 howMuchDarker)
{
  CRGB newcolor = color;
  newcolor.nscale8( 255 - howMuchDarker);
  return newcolor;
}

// Compact implementation of
// the directionFlags array, using just one BIT of RAM
// per pixel.  This requires a bunch of bit wrangling,
// but conserves precious RAM.  The cost is a few
// cycles and about 100 bytes of flash program memory.
uint8_t  directionFlags[ (NUM_LEDS+7) / 8];

bool getPixelDirection( uint16_t i)
{
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;

  uint8_t  andMask = 1 << bitNum;
  return (directionFlags[index] & andMask) != 0;
}

void setPixelDirection( uint16_t i, bool dir)
{
  uint16_t index = i / 8;
  uint8_t  bitNum = i & 0x07;

  uint8_t  orMask = 1 << bitNum;
  uint8_t andMask = 255 - orMask;
  uint8_t value = directionFlags[index] & andMask;
  if( dir ) {
    value += orMask;
  }
  directionFlags[index] = value;
}
