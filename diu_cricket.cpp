#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Windows' bundled gl.h (via MinGW) is OpenGL 1.1 and doesn't define this
// OpenGL 1.2+ constant. Hardcode its standard value so it compiles everywhere.
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// Background music (Windows only — PlaySound is a Windows API. On other
// platforms these calls just compile out to nothing, so the file still
// builds fine everywhere; only Windows actually plays sound.)
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

bool musicOn = true;

void setMusic(bool on)
{
#ifdef _WIN32
    if (on)
        PlaySound(TEXT("theme.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
    else
        PlaySound(NULL, 0, 0);
#else
    (void)on;
#endif
}

using namespace std;

//========================
// Window Size
//========================
const int WIDTH  = 1200;
const int HEIGHT = 700;

//========================
// Building texture (the actual reference image, textured onto a quad,
// instead of hand-drawn primitives). Falls back to the primitive-drawn
// building further below if building_texture.png can't be loaded.
//========================
GLuint buildingTexture  = 0;
bool   textureLoaded    = false;
int    texW = 0, texH = 0;
const float TEXTURE_BASE_Y = 260.0f; // matches the footpath line

//========================
// Field geometry (kept strictly BELOW the road so grass never
// overlaps the road/footpath — road starts at y = 170)
//========================
const float FIELD_CX = 600.0f;
const float FIELD_CY = 50.0f;
const float FIELD_RX = 540.0f;
const float FIELD_RY = 110.0f;  // top of field = 160, road bottom = 170 -> 10px gap

//========================
// Entities: players & pedestrians (fully positionable + controllable)
//========================
const int NUM_PLAYERS  = 11;
const int NUM_STUDENTS = 5;

const char* playerLabel[NUM_PLAYERS] = {
    "Batsman","Bowler","Keeper","Slip","Point",
    "Cover","Mid Off","Mid On","Square Leg","Fine Leg","Third Man"
};

float playerX[NUM_PLAYERS] = { 600, 685, 705, 740, 790, 850, 760, 460, 380, 300, 900 };
float playerY[NUM_PLAYERS] = {  34,  22,  34,  50,  68,  90, 118, 118,  88,  55, 118 };

float studentX[NUM_STUDENTS] = { 120, 200, 280, 360, 440 };
float studentY[NUM_STUDENTS] = { 245, 245, 245, 245, 245 };

enum ControlMode { PLAYER_MODE, STUDENT_MODE, CAR_MODE };
ControlMode controlMode   = PLAYER_MODE;
int         selectedPlayer  = 0;
int         selectedStudent = 0;

float carX = 300.0f;
float carY = 195.0f; // sits on the road (170-240)

//========================
// Global State
//========================
bool  dayMode  = true;   // 'd' toggles day / night
bool  rainMode = false;  // 'r' toggles rain
bool  bowl     = false;  // 'b' bowls the ball
bool  batting  = false;  // 'h' hits the ball while it's approaching the bat

float ballX       = 0.0f; // bowler -> batsman ball animation
float hitProgress  = 0.0f; // batsman -> outfield animation, 0..1
float hitTargetX   = 0.0f;
float hitTargetY   = 0.0f;
float cloudX       = 0.0f; // drifting clouds
float birdOffset    = 0.0f; // flying birds
float rainOffset    = 0.0f; // falling rain

// A handful of realistic shot directions to pick from when the batsman hits
const int NUM_SHOTS = 6;
const float shotTargetX[NUM_SHOTS] = { 820, 350, 600, 900, 300, 950 };
const float shotTargetY[NUM_SHOTS] = { 110, 110, 150,  70,  60,  55 };

float cameraZoom = 1.0f; // '+' / '-'
float cameraX    = 0.0f; // 'a' / 'f' pan

//========================
// Forward Declarations
//========================
void drawCircle(float x, float y, float radius);
void drawEllipse(float cx, float cy, float rx, float ry, bool filled);
void bitmapText(float x, float y, void* font, const char* text);
void drawWindow(float x, float y, float w, float h);
void drawCar(float x, float y);
float clampf(float v, float lo, float hi);
bool loadBuildingTexture();
void drawBuildingTexture();

void drawSky();
void drawGround();
void drawRoad();
void drawFootpath();
void drawLeftBuilding();
void drawCornerTower();
void drawAtrium();
void drawTower();
void drawMidBuilding();
void drawRightBuilding();
void drawRotunda();
void drawPalm(float x, float y);
void drawSignboard();

void drawTree(float x, float y);
void drawBush(float x, float y);
void drawStreetLight(float x);
void drawStreetLights();

void drawShadow(float x, float y);
void drawSelectionMarker(float x, float y);

void drawCricketGround();
void drawPitch();
void drawStumps(float x, float y);
void drawBall();
void drawPlayer(float x, float y, bool isBatsman);
void drawPlayers();

void drawSunMoon();
void drawCloud(float x, float y);
void drawClouds();
void drawStudent(float x, float y);
void drawStudents();
void drawBird(float x, float y);
void drawBirds();
void drawRain();
void drawHUD();

void display();
void keyboard(unsigned char key, int x, int y);
void specialKeys(int key, int x, int y);
void update(int value);
void init();

//========================
// Small Helpers
//========================
void drawCircle(float x, float y, float radius)
{
    glBegin(GL_POLYGON);
    for (int i = 0; i < 100; i++)
    {
        float angle = 2.0f * 3.1415926f * i / 100;
        glVertex2f(x + cos(angle) * radius, y + sin(angle) * radius);
    }
    glEnd();
}

void drawEllipse(float cx, float cy, float rx, float ry, bool filled)
{
    glBegin(filled ? GL_POLYGON : GL_LINE_LOOP);
    for (int i = 0; i < 100; i++)
    {
        float angle = 2.0f * 3.1415926f * i / 100;
        glVertex2f(cx + cos(angle) * rx, cy + sin(angle) * ry);
    }
    glEnd();
}

void bitmapText(float x, float y, void* font, const char* text)
{
    glRasterPos2f(x, y);
    while (*text)
    {
        glutBitmapCharacter(font, *text);
        text++;
    }
}

float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void drawWindow(float x, float y, float w, float h)
{
    glColor3f(0.40f, 0.46f, 0.52f);
    glBegin(GL_QUADS);
    glVertex2f(x - 1, y - 1);
    glVertex2f(x + w + 1, y - 1);
    glVertex2f(x + w + 1, y + h + 1);
    glVertex2f(x - 1, y + h + 1);
    glEnd();

    glColor3f(0.55f, 0.82f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x + 1, y + 1);
    glVertex2f(x + w - 1, y + 1);
    glVertex2f(x + w - 1, y + h - 1);
    glVertex2f(x + 1, y + h - 1);
    glEnd();
}

//========================
// Building texture — loads building_texture.png (the reference image, with
// its sky chroma-keyed to transparent) and draws it as a single textured
// quad spanning the building's footprint. Falls back to the hand-drawn
// primitive building below if the file can't be found.
//========================
bool loadBuildingTexture()
{
    stbi_set_flip_vertically_on_load(1);

    int channels = 0;
    unsigned char* data = stbi_load("building_texture.png", &texW, &texH, &channels, 4);
    if (!data)
    {
        fprintf(stderr, "Note: building_texture.png not found next to the executable "
                         "- using the primitive-drawn building instead.\n");
        return false;
    }

    glGenTextures(1, &buildingTexture);
    glBindTexture(GL_TEXTURE_2D, buildingTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return true;
}

void drawBuildingTexture()
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, buildingTexture);
    glColor3f(1.0f, 1.0f, 1.0f); // no tint - show the texture's own colors

    // Fit the texture within the available vertical space (base line up to the
    // top of the screen) so the antenna never gets clipped, then center it.
    float maxDispH = (float)HEIGHT - TEXTURE_BASE_Y;
    float dispH = maxDispH;
    float dispW = dispH * ((float)texW / (float)texH);
    if (dispW > (float)WIDTH)
    {
        dispW = (float)WIDTH;
        dispH = dispW * ((float)texH / (float)texW);
    }
    float startX = ((float)WIDTH - dispW) / 2.0f;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(startX, TEXTURE_BASE_Y);
    glTexCoord2f(1, 0); glVertex2f(startX + dispW, TEXTURE_BASE_Y);
    glTexCoord2f(1, 1); glVertex2f(startX + dispW, TEXTURE_BASE_Y + dispH);
    glTexCoord2f(0, 1); glVertex2f(startX, TEXTURE_BASE_Y + dispH);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

//========================
// Sky
//========================
void drawSky()
{
    if (dayMode)
        glColor3f(0.53f, 0.81f, 0.98f);
    else
        glColor3f(0.05f, 0.05f, 0.18f);

    glBegin(GL_QUADS);
    glVertex2f(0, 350);
    glVertex2f(WIDTH, 350);
    glVertex2f(WIDTH, HEIGHT);
    glVertex2f(0, HEIGHT);
    glEnd();
}

//========================
// Ground
//========================
void drawGround()
{
    glColor3f(0.25f, 0.65f, 0.20f);

    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(WIDTH, 0);
    glVertex2f(WIDTH, 350);
    glVertex2f(0, 350);
    glEnd();
}

//========================
// Road  (y 170-240 — the cricket field below is capped at 158, so it can
// never paint over this strip again)
//========================
void drawRoad()
{
    glColor3f(0.20f, 0.20f, 0.20f);

    glBegin(GL_QUADS);
    glVertex2f(0, 170);
    glVertex2f(WIDTH, 170);
    glVertex2f(WIDTH, 240);
    glVertex2f(0, 240);
    glEnd();

    glColor3f(1, 1, 1);
    for (int i = 0; i < WIDTH; i += 80)
    {
        glBegin(GL_QUADS);
        glVertex2f(i, 203);
        glVertex2f(i + 40, 203);
        glVertex2f(i + 40, 207);
        glVertex2f(i, 207);
        glEnd();
    }
}

//========================
// Footpath
//========================
void drawFootpath()
{
    glColor3f(0.75f, 0.75f, 0.75f);

    glBegin(GL_QUADS);
    glVertex2f(0, 240);
    glVertex2f(WIDTH, 240);
    glVertex2f(WIDTH, 260);
    glVertex2f(0, 260);
    glEnd();
}

//========================
// Left Building (shorter block, left of the tower)
//========================
void drawLeftBuilding()
{
    glColor3f(0.92f, 0.92f, 0.92f);
    glBegin(GL_QUADS);
    glVertex2f(200, 260);
    glVertex2f(430, 260);
    glVertex2f(430, 480);
    glVertex2f(200, 480);
    glEnd();

    glColor3f(0.82f, 0.82f, 0.82f);
    for (int x = 200; x <= 430; x += 15)
    {
        glBegin(GL_QUADS);
        glVertex2f(x, 260);
        glVertex2f(x + 3, 260);
        glVertex2f(x + 3, 480);
        glVertex2f(x, 480);
        glEnd();
    }

    glColor3f(0.55f, 0.80f, 1.0f);
    for (int y = 300; y <= 450; y += 30)
        for (int x = 220; x <= 400; x += 30)
            drawWindow(x, y, 18, 20);
}

//========================
// Corner Tower (the rounded turret on the far left, with horizontal window
// bands and a domed cap, as in the reference image)
//========================
void drawCornerTower()
{
    // Lower drum
    glColor3f(0.90f, 0.90f, 0.90f);
    glBegin(GL_QUADS);
    glVertex2f(214, 260);
    glVertex2f(286, 260);
    glVertex2f(286, 480);
    glVertex2f(214, 480);
    glEnd();

    glColor3f(0.55f, 0.80f, 1.0f);
    for (int y = 288; y <= 452; y += 24)
    {
        glBegin(GL_QUADS);
        glVertex2f(222, (float)y);
        glVertex2f(278, (float)y);
        glVertex2f(278, y + 14.0f);
        glVertex2f(222, y + 14.0f);
        glEnd();
    }

    // Belt ring
    glColor3f(0.15f, 0.35f, 0.80f);
    glBegin(GL_QUADS);
    glVertex2f(214, 480);
    glVertex2f(286, 480);
    glVertex2f(286, 494);
    glVertex2f(214, 494);
    glEnd();

    // Upper turret — narrower second tier, as in the reference
    glColor3f(0.94f, 0.94f, 0.94f);
    glBegin(GL_QUADS);
    glVertex2f(228, 494);
    glVertex2f(272, 494);
    glVertex2f(272, 548);
    glVertex2f(228, 548);
    glEnd();

    glColor3f(0.55f, 0.80f, 1.0f);
    for (int y = 500; y <= 536; y += 18)
    {
        glBegin(GL_QUADS);
        glVertex2f(233.0f, (float)y);
        glVertex2f(267.0f, (float)y);
        glVertex2f(267.0f, y + 10.0f);
        glVertex2f(233.0f, y + 10.0f);
        glEnd();
    }

    // Domed cap — anchored at the turret's top edge, no floating gap
    glColor3f(0.85f, 0.85f, 0.85f);
    drawEllipse(250, 548, 26, 16, true);

    // Thin spire, angled up-left as in the reference
    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_LINES);
    glVertex2f(252, 560); glVertex2f(236, 600);
    glEnd();
}

//========================
// Atrium (the low glass connector piece between the corner tower and the
// main tower, visible in the reference)
//========================
void drawAtrium()
{
    glColor3f(0.90f, 0.90f, 0.90f);
    glBegin(GL_QUADS);
    glVertex2f(286, 260);
    glVertex2f(430, 260);
    glVertex2f(430, 385);
    glVertex2f(286, 385);
    glEnd();

    glColor3f(0.55f, 0.82f, 1.0f);
    for (int y = 292; y <= 356; y += 22)
        for (int x = 296; x <= 410; x += 22)
            drawWindow((float)x, (float)y, 14, 14);

    glColor3f(0.75f, 0.75f, 0.75f);
    glBegin(GL_QUADS);
    glVertex2f(286, 379);
    glVertex2f(430, 379);
    glVertex2f(430, 388);
    glVertex2f(286, 388);
    glEnd();
}

//========================
// Tower (the tall block behind the corner tower, matches the sketch's
// tallest section, now with a rooftop penthouse + antenna mast)
//========================
void drawTower()
{
    glColor3f(0.94f, 0.94f, 0.94f);
    glBegin(GL_QUADS);
    glVertex2f(430, 260);
    glVertex2f(620, 260);
    glVertex2f(620, 615);
    glVertex2f(430, 615);
    glEnd();

    // Vertical facade fins (the striped texture on the tower in the sketch)
    glColor3f(0.82f, 0.82f, 0.82f);
    for (int x = 430; x <= 620; x += 15)
    {
        glBegin(GL_QUADS);
        glVertex2f(x, 260);
        glVertex2f(x + 3, 260);
        glVertex2f(x + 3, 615);
        glVertex2f(x, 615);
        glEnd();
    }

    // Windows (stop well short of the signboard band at y=560)
    glColor3f(0.55f, 0.82f, 1.0f);
    for (int y = 300; y <= 550; y += 28)
        for (int x = 445; x <= 595; x += 30)
            drawWindow(x, y, 20, 20);

    // Roof cap
    glColor3f(0.15f, 0.35f, 0.80f);
    glBegin(GL_QUADS);
    glVertex2f(430, 605);
    glVertex2f(620, 605);
    glVertex2f(620, 615);
    glVertex2f(430, 615);
    glEnd();

    // Rooftop penthouse (small box with a colonnade, matching the reference)
    glColor3f(0.95f, 0.95f, 0.95f);
    glBegin(GL_QUADS);
    glVertex2f(478, 615);
    glVertex2f(572, 615);
    glVertex2f(572, 660);
    glVertex2f(478, 660);
    glEnd();

    glColor3f(0.15f, 0.35f, 0.80f);
    for (int x = 486; x <= 564; x += 14)
    {
        glBegin(GL_QUADS);
        glVertex2f(x, 615);
        glVertex2f(x + 4, 615);
        glVertex2f(x + 4, 655);
        glVertex2f(x, 655);
        glEnd();
    }

    glColor3f(0.15f, 0.35f, 0.80f);
    glBegin(GL_QUADS);
    glVertex2f(478, 660);
    glVertex2f(572, 660);
    glVertex2f(572, 670);
    glVertex2f(478, 670);
    glEnd();

    // Antenna mast + dish
    glColor3f(0.45f, 0.45f, 0.45f);
    glBegin(GL_QUADS);
    glVertex2f(521, 670);
    glVertex2f(527, 670);
    glVertex2f(527, 718);
    glVertex2f(521, 718);
    glEnd();

    glColor3f(0.55f, 0.55f, 0.55f);
    glBegin(GL_LINES);
    glVertex2f(510, 700); glVertex2f(538, 700);
    glEnd();
    drawEllipse(524, 720, 9, 4, false);

    glColor3f(1, 0, 0);
    drawCircle(524, 722, 3);
}

//========================
// Mid Building (large block right of the tower — carries the signboard
// and sits behind the rotunda entrance)
//========================
void drawMidBuilding()
{
    glColor3f(0.94f, 0.94f, 0.94f);
    glBegin(GL_QUADS);
    glVertex2f(620, 260);
    glVertex2f(900, 260);
    glVertex2f(900, 560);
    glVertex2f(620, 560);
    glEnd();

    glColor3f(0.82f, 0.82f, 0.82f);
    for (int x = 620; x <= 900; x += 15)
    {
        glBegin(GL_QUADS);
        glVertex2f(x, 260);
        glVertex2f(x + 3, 260);
        glVertex2f(x + 3, 560);
        glVertex2f(x, 560);
        glEnd();
    }

    glColor3f(0.55f, 0.82f, 1.0f);
    for (int y = 300; y <= 535; y += 25)
        for (int x = 636; x <= 880; x += 26)
            drawWindow(x, y, 18, 18);
}

//========================
// Right Building (small sliver of building peeking out on the far right)
//========================
void drawRightBuilding()
{
    // Sloped wall — the roofline recedes down-right, hinting at perspective
    glColor3f(0.92f, 0.92f, 0.92f);
    glBegin(GL_QUADS);
    glVertex2f(900, 260);
    glVertex2f(1100, 260);
    glVertex2f(1100, 495);
    glVertex2f(900, 560);
    glEnd();

    glColor3f(0.80f, 0.80f, 0.80f);
    for (int x = 900; x <= 1100; x += 15)
    {
        float slantTop = 560.0f - (x - 900) * (65.0f / 200.0f);
        glBegin(GL_QUADS);
        glVertex2f((float)x, 260);
        glVertex2f(x + 3.0f, 260);
        glVertex2f(x + 3.0f, slantTop);
        glVertex2f((float)x, slantTop);
        glEnd();
    }

    glColor3f(0.55f, 0.80f, 1.0f);
    for (int y = 300; y <= 535; y += 25)
        for (int x = 918; x <= 1080; x += 26)
        {
            float slantTop = 560.0f - (x - 900) * (65.0f / 200.0f);
            if (y + 18 > slantTop) continue; // stay under the sloped roofline

            if (x >= 1044) // rightmost columns read as open balconies, as in the reference
            {
                glColor3f(0.70f, 0.70f, 0.70f);
                glBegin(GL_QUADS);
                glVertex2f((float)x, (float)y);
                glVertex2f(x + 18.0f, (float)y);
                glVertex2f(x + 18.0f, y + 18.0f);
                glVertex2f((float)x, y + 18.0f);
                glEnd();

                glColor3f(0.35f, 0.35f, 0.35f);
                glBegin(GL_LINES);
                for (float bx = x + 2.0f; bx <= x + 16.0f; bx += 4.0f)
                {
                    glVertex2f(bx, (float)y);
                    glVertex2f(bx, y + 6.0f);
                }
                glVertex2f((float)x, y + 6.0f);
                glVertex2f(x + 18.0f, y + 6.0f);
                glEnd();
                glColor3f(0.55f, 0.80f, 1.0f);
            }
            else
            {
                drawWindow((float)x, (float)y, 18, 18);
            }
        }

    // Blue roof cap tracing the slanted edge
    glColor3f(0.15f, 0.35f, 0.80f);
    glBegin(GL_QUADS);
    glVertex2f(900, 552);
    glVertex2f(1100, 487);
    glVertex2f(1100, 495);
    glVertex2f(900, 560);
    glEnd();
}

//========================
// Rotunda (the rounded glass entrance drum in front of the mid building,
// with window bands, a roof overhang, an entrance door and steps)
//========================
void drawRotunda()
{
    float cx = 760;
    float drumTop = 375, drumBottom = 260;
    float drumLeft = cx - 120, drumRight = cx + 120;

    // Glass drum (a tall wall, not a flat dome — the roof sits on THIS)
    glColor3f(0.75f, 0.85f, 0.92f);
    glBegin(GL_QUADS);
    glVertex2f(drumLeft, drumBottom);
    glVertex2f(drumRight, drumBottom);
    glVertex2f(drumRight, drumTop);
    glVertex2f(drumLeft, drumTop);
    glEnd();

    // Vertical mullions across the drum face
    glColor3f(0.92f, 0.95f, 0.97f);
    for (int x = (int)drumLeft + 6; x <= (int)drumRight; x += 18)
    {
        glBegin(GL_LINES);
        glVertex2f((float)x, drumBottom);
        glVertex2f((float)x, drumTop);
        glEnd();
    }

    // Dome roof — ellipse centered AT the drum's top edge, so its lower half
    // fuses seamlessly into the drum and only the upper half rises as the dome
    glColor3f(0.15f, 0.35f, 0.75f);
    drawEllipse(cx, drumTop, 138, 44, true);

    // Highlight patch for a hint of curvature on the dome
    glColor3f(0.35f, 0.55f, 0.85f);
    drawEllipse(cx - 30, drumTop + 12, 60, 20, true);

    // Canopy overhang right above the entrance
    glColor3f(0.15f, 0.20f, 0.30f);
    drawEllipse(cx, 273, 105, 10, true);

    // Entrance doors (paneled glass)
    glColor3f(0.08f, 0.10f, 0.15f);
    for (int i = 0; i < 4; i++)
    {
        float x0 = cx - 32 + i * 16;
        glBegin(GL_QUADS);
        glVertex2f(x0, 260);
        glVertex2f(x0 + 13, 260);
        glVertex2f(x0 + 13, 300);
        glVertex2f(x0, 300);
        glEnd();
    }

    // Entrance steps
    glColor3f(0.80f, 0.80f, 0.78f);
    for (int i = 0; i < 3; i++)
    {
        float w  = 60 + i * 18;
        float yb = 254 - i * 7;
        glBegin(GL_QUADS);
        glVertex2f(cx - w / 2, yb);
        glVertex2f(cx + w / 2, yb);
        glVertex2f(cx + w / 2, yb + 6);
        glVertex2f(cx - w / 2, yb + 6);
        glEnd();
    }

    drawPalm(cx - 70, 254);
    drawPalm(cx + 70, 254);
}

//========================
// Palm Tree (flanks the rotunda entrance, matching the sketch)
//========================
void drawPalm(float x, float y)
{
    glColor3f(0.45f, 0.30f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x - 2, y);
    glVertex2f(x + 2, y);
    glVertex2f(x + 2, y + 26);
    glVertex2f(x - 2, y + 26);
    glEnd();

    glColor3f(0.0f, 0.55f, 0.15f);
    glBegin(GL_LINES);
    for (int i = 0; i < 5; i++)
    {
        float angle = 0.3f + i * 0.55f;
        glVertex2f(x, y + 26);
        glVertex2f(x + cos(angle) * 16, y + 26 + sin(angle) * 11);
    }
    glEnd();
}

//========================
// Sign Board ("DAFFODIL INTERNATIONAL UNIVERSITY" — spans across the
// tower/mid junction, near the top, as in the sketch)
//========================
void drawSignboard()
{
    glColor3f(0.15f, 0.35f, 0.80f);
    glBegin(GL_QUADS);
    glVertex2f(420, 560);
    glVertex2f(1000, 560);
    glVertex2f(1000, 595);
    glVertex2f(420, 595);
    glEnd();

    glColor3f(1, 1, 1);
    bitmapText(470, 573, GLUT_BITMAP_HELVETICA_18, "DAFFODIL INTERNATIONAL UNIVERSITY");
}

//========================
// Tree
//========================
void drawTree(float x, float y)
{
    glColor3f(0.45f, 0.25f, 0.05f);
    glBegin(GL_QUADS);
    glVertex2f(x - 4, y);
    glVertex2f(x + 4, y);
    glVertex2f(x + 4, y + 30);
    glVertex2f(x - 4, y + 30);
    glEnd();

    glColor3f(0.0f, 0.60f, 0.0f);
    drawCircle(x, y + 45, 18);
    drawCircle(x - 12, y + 40, 14);
    drawCircle(x + 12, y + 40, 14);
}

//========================
// Bush
//========================
void drawBush(float x, float y)
{
    glColor3f(0.10f, 0.55f, 0.10f);
    drawCircle(x, y, 10);
    drawCircle(x + 10, y + 5, 10);
    drawCircle(x - 10, y + 5, 10);
}

//========================
// Street Light
//========================
void drawStreetLight(float x)
{
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(x, 240);
    glVertex2f(x + 4, 240);
    glVertex2f(x + 4, 330);
    glVertex2f(x, 330);
    glEnd();

    glColor3f(1, 1, 0.2f);
    drawCircle(x + 2, 336, 5);
}

void drawStreetLights()
{
    drawStreetLight(100);
    drawStreetLight(300);
    drawStreetLight(500);
    drawStreetLight(700);
    drawStreetLight(900);
    drawStreetLight(1100);
}

//========================
// Shadow + Selection Marker (grounding & control feedback)
//========================
void drawShadow(float x, float y)
{
    glColor3f(0.10f, 0.32f, 0.10f);
    drawEllipse(x, y - 6, 9, 3, true);
}

void drawSelectionMarker(float x, float y)
{
    glColor3f(1.0f, 1.0f, 0.0f);
    glLineWidth(2);
    drawEllipse(x, y - 6, 12, 4, false);
    glLineWidth(1);
}

//========================
// Cricket Ground — mowed oval, capped strictly under the road (FIELD_RY=158
// vs road starting at 170), with a boundary rope line
//========================
void drawCricketGround()
{
    const int rings = 6;
    for (int i = 0; i < rings; i++)
    {
        float t = 1.0f - (float)i / rings;
        if (i % 2 == 0) glColor3f(0.20f, 0.62f, 0.20f);
        else            glColor3f(0.16f, 0.55f, 0.16f);
        drawEllipse(FIELD_CX, FIELD_CY, FIELD_RX * t, FIELD_RY * t, true);
    }

    glColor3f(0.95f, 0.95f, 0.85f);
    glLineWidth(2);
    drawEllipse(FIELD_CX, FIELD_CY, FIELD_RX, FIELD_RY, false);
    glLineWidth(1);
}

//========================
// Pitch
//========================
void drawPitch()
{
    glColor3f(0.80f, 0.68f, 0.45f);
    glBegin(GL_QUADS);
    glVertex2f(520, 15);
    glVertex2f(680, 15);
    glVertex2f(680, 40);
    glVertex2f(520, 40);
    glEnd();
}

//========================
// Stumps — 3 stumps with 2 bails resting on top, wood-toned
//========================
void drawStumps(float x, float y)
{
    const float stumpH = 24.0f;

    glColor3f(0.85f, 0.72f, 0.45f);
    for (int i = 0; i < 3; i++)
    {
        glBegin(GL_QUADS);
        glVertex2f(x + i * 6.0f, y);
        glVertex2f(x + i * 6.0f + 2.5f, y);
        glVertex2f(x + i * 6.0f + 2.5f, y + stumpH);
        glVertex2f(x + i * 6.0f, y + stumpH);
        glEnd();
    }

    // Bails
    glColor3f(0.62f, 0.47f, 0.27f);
    glBegin(GL_QUADS);
    glVertex2f(x + 1.5f, y + stumpH);
    glVertex2f(x + 6.5f, y + stumpH);
    glVertex2f(x + 6.5f, y + stumpH + 2.5f);
    glVertex2f(x + 1.5f, y + stumpH + 2.5f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x + 7.5f, y + stumpH);
    glVertex2f(x + 12.5f, y + stumpH);
    glVertex2f(x + 12.5f, y + stumpH + 2.5f);
    glVertex2f(x + 7.5f, y + stumpH + 2.5f);
    glEnd();
}

//========================
// Ball — released from the Bowler's raised hand (follows the bowler if
// moved), travels to the striker's stumps, and — if hit — flies out to
// wherever the batsman placed the shot
//========================
void drawBall()
{
    float bx, by;

    if (batting)
    {
        float startX = playerX[0] + 9.0f; // from the bat
        float startY = playerY[0] + 14.0f;
        bx = startX + (hitTargetX - startX) * hitProgress;
        by = startY + (hitTargetY - startY) * hitProgress;
    }
    else
    {
        float t = ballX / 145.0f;
        if (t > 1.0f) t = 1.0f;

        float startX = playerX[1] + 10.0f; // Bowler's hand, mid-delivery
        float startY = playerY[1] + 32.0f;
        float endX = 525.0f;               // striker's stumps
        float endY = 26.0f;

        bx = startX + (endX - startX) * t;
        by = startY + (endY - startY) * t;
    }

    glColor3f(1, 0, 0);
    drawCircle(bx, by, 4);
}

//========================
// Player  (index 0 = Batsman gets whites + a bat, rest are fielding blue)
//========================
void drawPlayer(float x, float y, bool isBatsman)
{
    drawShadow(x, y);

    // Head
    glColor3f(1.0f, 0.82f, 0.65f);
    drawCircle(x, y + 28, 5);

    // Body
    if (isBatsman) glColor3f(0.90f, 0.90f, 0.85f);
    else           glColor3f(0.15f, 0.25f, 0.85f);

    glBegin(GL_QUADS);
    glVertex2f(x - 4, y + 8);
    glVertex2f(x + 4, y + 8);
    glVertex2f(x + 4, y + 22);
    glVertex2f(x - 4, y + 22);
    glEnd();

    // Hands
    glColor3f(0, 0, 0);
    glBegin(GL_LINES);
    glVertex2f(x - 4, y + 18);
    glVertex2f(x - 10, y + 10);
    glVertex2f(x + 4, y + 18);
    glVertex2f(x + 10, y + 10);
    glEnd();

    // Legs
    glBegin(GL_LINES);
    glVertex2f(x - 2, y + 8);
    glVertex2f(x - 6, y - 5);
    glVertex2f(x + 2, y + 8);
    glVertex2f(x + 6, y - 5);
    glEnd();

    // Bat (batsman only)
    if (isBatsman)
    {
        glColor3f(0.55f, 0.35f, 0.10f);
        glBegin(GL_QUADS);
        glVertex2f(x + 9, y + 4);
        glVertex2f(x + 13, y + 4);
        glVertex2f(x + 13, y + 24);
        glVertex2f(x + 9, y + 24);
        glEnd();
    }
}

//========================
// Players (positions are individually controllable — see keyboard/specialKeys)
//========================
void drawPlayers()
{
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        drawPlayer(playerX[i], playerY[i], i == 0);
        if (controlMode == PLAYER_MODE && i == selectedPlayer)
            drawSelectionMarker(playerX[i], playerY[i]);
    }
}

//========================
// Sun / Moon
//========================
void drawSunMoon()
{
    if (dayMode)
    {
        glColor3f(1.0f, 0.9f, 0.0f);
        drawCircle(1080, 620, 35);
    }
    else
    {
        glColor3f(0.95f, 0.95f, 0.95f);
        drawCircle(1080, 620, 28);
    }
}

//========================
// Cloud
//========================
void drawCloud(float x, float y)
{
    glColor3f(1, 1, 1);
    drawCircle(x, y, 18);
    drawCircle(x + 18, y + 6, 18);
    drawCircle(x + 36, y, 18);
    drawCircle(x + 18, y - 8, 18);
}

void drawClouds()
{
    drawCloud(100 + cloudX, 600);
    drawCloud(350 + cloudX, 560);
    drawCloud(650 + cloudX, 620);
    drawCloud(950 + cloudX, 580);
}

//========================
// Student  (people on the footpath — individually controllable)
//========================
void drawStudent(float x, float y)
{
    drawShadow(x, y);

    glColor3f(1.0f, 0.82f, 0.65f);
    drawCircle(x, y + 20, 5);

    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x - 4, y + 5);
    glVertex2f(x + 4, y + 5);
    glVertex2f(x + 4, y + 18);
    glVertex2f(x - 4, y + 18);
    glEnd();

    glColor3f(0, 0, 0);
    glBegin(GL_LINES);
    glVertex2f(x - 2, y + 5);
    glVertex2f(x - 6, y - 8);
    glVertex2f(x + 2, y + 5);
    glVertex2f(x + 6, y - 8);
    glEnd();

    glBegin(GL_LINES);
    glVertex2f(x - 4, y + 14);
    glVertex2f(x - 10, y + 8);
    glVertex2f(x + 4, y + 14);
    glVertex2f(x + 10, y + 8);
    glEnd();
}

void drawStudents()
{
    for (int i = 0; i < NUM_STUDENTS; i++)
    {
        drawStudent(studentX[i], studentY[i]);
        if (controlMode == STUDENT_MODE && i == selectedStudent)
            drawSelectionMarker(studentX[i], studentY[i]);
    }
}

//========================
// Car (controllable — drives on the road)
//========================
void drawCar(float x, float y)
{
    glColor3f(0.10f, 0.28f, 0.10f);
    drawEllipse(x, y - 11, 22, 4, true);

    // Body
    glColor3f(0.85f, 0.15f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(x - 22, y - 7);
    glVertex2f(x + 22, y - 7);
    glVertex2f(x + 22, y + 3);
    glVertex2f(x - 22, y + 3);
    glEnd();

    // Cabin
    glBegin(GL_QUADS);
    glVertex2f(x - 12, y + 3);
    glVertex2f(x + 12, y + 3);
    glVertex2f(x + 9, y + 13);
    glVertex2f(x - 9, y + 13);
    glEnd();

    // Windows
    glColor3f(0.65f, 0.85f, 0.95f);
    glBegin(GL_QUADS);
    glVertex2f(x - 9, y + 4);
    glVertex2f(x - 1, y + 4);
    glVertex2f(x - 2, y + 12);
    glVertex2f(x - 8, y + 12);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x + 1, y + 4);
    glVertex2f(x + 9, y + 4);
    glVertex2f(x + 8, y + 12);
    glVertex2f(x + 2, y + 12);
    glEnd();

    // Headlights
    glColor3f(1.0f, 1.0f, 0.6f);
    drawCircle(x + 22, y - 2, 2.5f);
    glColor3f(0.9f, 0.1f, 0.1f);
    drawCircle(x - 22, y - 2, 2.5f);

    // Wheels
    glColor3f(0.05f, 0.05f, 0.05f);
    drawCircle(x - 13, y - 8, 5);
    drawCircle(x + 13, y - 8, 5);
    glColor3f(0.55f, 0.55f, 0.55f);
    drawCircle(x - 13, y - 8, 2);
    drawCircle(x + 13, y - 8, 2);
}

//========================
// Bird
//========================
void drawBird(float x, float y)
{
    glColor3f(0, 0, 0);

    glBegin(GL_LINE_STRIP);
    glVertex2f(x, y);
    glVertex2f(x + 8, y + 6);
    glVertex2f(x + 16, y);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex2f(x + 16, y);
    glVertex2f(x + 24, y + 6);
    glVertex2f(x + 32, y);
    glEnd();
}

void drawBirds()
{
    drawBird(120 + birdOffset, 620);
    drawBird(260 + birdOffset, 650);
    drawBird(520 + birdOffset, 610);
    drawBird(820 + birdOffset, 640);
}

//========================
// Rain
//========================
void drawRain()
{
    if (!rainMode) return;

    glColor3f(0.75f, 0.85f, 1.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < WIDTH; i += 20)
        for (int j = 0; j < HEIGHT; j += 35)
        {
            glVertex2f(i, j - rainOffset);
            glVertex2f(i + 5, j - 12 - rainOffset);
        }
    glEnd();
}

//========================
// HUD (fixed to the screen — drawn after resetting the camera transform)
//========================
void drawHUD()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 0.0f);
    bitmapText(15, HEIGHT - 22, GLUT_BITMAP_HELVETICA_12,
        "P=player  S=student  C=car  1-9,0=jump to entity  TAB=next  Arrows=move  B=bowl  H=hit");

    char buf[96];
    if (controlMode == PLAYER_MODE)
        snprintf(buf, sizeof(buf), "Controlling PLAYER: %s (%d/%d)",
                 playerLabel[selectedPlayer], selectedPlayer + 1, NUM_PLAYERS);
    else if (controlMode == STUDENT_MODE)
        snprintf(buf, sizeof(buf), "Controlling STUDENT #%d of %d",
                 selectedStudent + 1, NUM_STUDENTS);
    else
        snprintf(buf, sizeof(buf), "Controlling CAR");

    bitmapText(15, HEIGHT - 40, GLUT_BITMAP_HELVETICA_12, buf);

    char musicBuf[48];
    snprintf(musicBuf, sizeof(musicBuf), "Music: %s (M to toggle)", musicOn ? "ON" : "OFF");
    bitmapText(15, HEIGHT - 58, GLUT_BITMAP_HELVETICA_12, musicBuf);
}

//========================
// Display
//========================
void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(cameraX, 0, 0);
    glScalef(cameraZoom, cameraZoom, 1);

    drawSky();
    if (!textureLoaded)
    {
        drawSunMoon();
        drawClouds();
    }

    drawGround();
    drawRoad();
    drawFootpath();

    if (textureLoaded)
    {
        drawBuildingTexture();
    }
    else
    {
        drawLeftBuilding();
        drawCornerTower();
        drawAtrium();
        drawTower();
        drawMidBuilding();
        drawRightBuilding();
        drawSignboard();
        drawRotunda();
    }

    drawTree(60, 250);
    drawTree(170, 250);
    drawTree(300, 250);
    drawTree(890, 250);
    drawTree(1110, 250);

    drawBush(350, 250);
    drawBush(420, 250);
    drawBush(780, 250);
    drawBush(850, 250);

    drawStreetLights();
    drawStudents();

    drawCar(carX, carY);
    if (controlMode == CAR_MODE)
        drawSelectionMarker(carX, carY - 5);

    drawCricketGround();
    drawPitch();
    drawStumps(520, 15);
    drawStumps(670, 15);
    drawPlayers();
    drawBall();

    drawBirds();
    drawRain();

    drawHUD();

    glutSwapBuffers();
}

//========================
// Keyboard (ASCII keys)
//========================
void keyboard(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'd': case 'D': dayMode = !dayMode; break;
    case 'b': case 'B':
        if (!batting) { bowl = true; ballX = 0.0f; }
        break;

    case 'h': case 'H':
        if (bowl && !batting && ballX >= 95.0f)
        {
            bowl = false;
            batting = true;
            hitProgress = 0.0f;
            int idx = rand() % NUM_SHOTS;
            hitTargetX = shotTargetX[idx];
            hitTargetY = shotTargetY[idx];
        }
        break;
    case 'r': case 'R': rainMode = !rainMode; break;
    case 'm': case 'M':
        musicOn = !musicOn;
        setMusic(musicOn);
        break;

    case '+': cameraZoom += 0.05f; break;
    case '-':
        cameraZoom -= 0.05f;
        if (cameraZoom < 0.6f) cameraZoom = 0.6f;
        break;

    case 'a': case 'A': cameraX += 20; break;
    case 'f': case 'F': cameraX -= 20; break;

    case 'p': case 'P': controlMode = PLAYER_MODE; break;
    case 's': case 'S': controlMode = STUDENT_MODE; break;
    case 'c': case 'C': controlMode = CAR_MODE; break;

    case 9:   // Tab
    case ']':
        if (controlMode == PLAYER_MODE) selectedPlayer  = (selectedPlayer + 1) % NUM_PLAYERS;
        else if (controlMode == STUDENT_MODE) selectedStudent = (selectedStudent + 1) % NUM_STUDENTS;
        break;

    case 8:   // Backspace
    case '[':
        if (controlMode == PLAYER_MODE) selectedPlayer  = (selectedPlayer  - 1 + NUM_PLAYERS)  % NUM_PLAYERS;
        else if (controlMode == STUDENT_MODE) selectedStudent = (selectedStudent - 1 + NUM_STUDENTS) % NUM_STUDENTS;
        break;

    // Direct jump: '1'..'9' -> index 0..8, '0' -> index 9.
    // Every single player and every single student is reachable this way.
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '0':
    {
        int idx = (key == '0') ? 9 : (key - '1');
        if (controlMode == PLAYER_MODE)
            selectedPlayer = (idx < NUM_PLAYERS) ? idx : NUM_PLAYERS - 1;
        else if (controlMode == STUDENT_MODE)
            selectedStudent = (idx < NUM_STUDENTS) ? idx : NUM_STUDENTS - 1;
        break;
    }

    case 27: exit(0); // ESC
    }

    glutPostRedisplay();
}

//========================
// Special Keys (arrows move the currently selected player/student)
//========================
void specialKeys(int key, int x, int y)
{
    const float step = 6.0f;

    if (controlMode == PLAYER_MODE)
    {
        float &px = playerX[selectedPlayer];
        float &py = playerY[selectedPlayer];

        if (key == GLUT_KEY_LEFT)  px -= step;
        if (key == GLUT_KEY_RIGHT) px += step;
        if (key == GLUT_KEY_UP)    py += step;
        if (key == GLUT_KEY_DOWN)  py -= step;

        px = clampf(px, 30, 1170);
        py = clampf(py, 8, 340); // stays on the visible ground, below the sky
    }
    else if (controlMode == STUDENT_MODE)
    {
        float &sx = studentX[selectedStudent];
        float &sy = studentY[selectedStudent];

        if (key == GLUT_KEY_LEFT)  sx -= step;
        if (key == GLUT_KEY_RIGHT) sx += step;
        if (key == GLUT_KEY_UP)    sy += step;
        if (key == GLUT_KEY_DOWN)  sy -= step;

        sx = clampf(sx, 20, 1180);
        sy = clampf(sy, 220, 292); // keep pedestrians on the footpath/road strip
    }
    else // CAR_MODE
    {
        const float carStep = 9.0f;
        if (key == GLUT_KEY_LEFT)  carX -= carStep;
        if (key == GLUT_KEY_RIGHT) carX += carStep;
        if (key == GLUT_KEY_UP)    carY += 4.0f;
        if (key == GLUT_KEY_DOWN)  carY -= 4.0f;

        carX = clampf(carX, 10, 1190);
        carY = clampf(carY, 178, 232); // keep the car on the road
    }

    glutPostRedisplay();
}

//========================
// Animation / Timer
//========================
void update(int value)
{
    if (bowl)
    {
        ballX += 3.0f;
        if (ballX > 145)
        {
            ballX = 0;
            bowl = false;
        }
    }

    if (batting)
    {
        hitProgress += 0.025f;
        if (hitProgress >= 1.0f)
        {
            batting = false;
            hitProgress = 0.0f;
            ballX = 0.0f;
        }
    }

    cloudX += 0.2f;
    if (cloudX > 1200) cloudX = -300;

    for (int i = 0; i < NUM_STUDENTS; i++)
    {
        bool beingControlled = (controlMode == STUDENT_MODE && i == selectedStudent);
        if (!beingControlled)
        {
            studentX[i] += 0.4f;
            if (studentX[i] > 1250) studentX[i] = -60;
        }
    }

    birdOffset += 0.4f;
    if (birdOffset > 1200) birdOffset = -300;

    rainOffset += 3;
    if (rainOffset > 35) rainOffset = 0;

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

//========================
// Init
//========================
void init()
{
    glClearColor(1, 1, 1, 1);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

//========================
// Main
//========================
int main(int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("DIU Cricket Ground");

    init();
    textureLoaded = loadBuildingTexture();
    setMusic(musicOn);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
