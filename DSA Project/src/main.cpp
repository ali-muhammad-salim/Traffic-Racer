#include "raylib.h"
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <iostream>

using namespace std;

// -------------------- Constants --------------------
const int SCREEN_WIDTH = 1000;
const int SCREEN_HEIGHT = 650;
const int ROAD_WIDTH = 600;
const int ROAD_X = (SCREEN_WIDTH - ROAD_WIDTH) / 2;
const int LANE_WIDTH = 120;
const int NUM_LANES = 5;
const int MAX_LEVEL = 100;
const int TOP_K_SCORES = 10;
const int FRAME_RATE = 60;
const int FRAMES_PER_SEC = FRAME_RATE;

// Score points required to gain a level (smaller = faster level ups)
const int LEVEL_SCORE_INTERVAL = 150;

// -------------------- Enums & Structs --------------------
enum GameState { MENU, PLAYING, PAUSED, GAME_OVER, SCORES };
enum SceneType { CITY, HIGHWAY, DESERT, NIGHT, FOREST, SNOW, SUNSET, RAIN };
enum PowerUpType { SHIELD, SLOW_MOTION, SCORE_MULTIPLIER, EXTRA_LIFE };

struct Position { float x, y; Position(float X=0, float Y=0): x(X), y(Y) {} };
struct CollisionBox {
    float x,y,w,h;
    bool checkCollision(const CollisionBox &o) const {
        return x < o.x + o.w && x + w > o.x && y < o.y + o.h && y + h > o.y;
    }
};

// -------------------- Forward declarations --------------------
class EnemyManager;
class PowerUpManager;
class EventScheduler;
class JobQueue;
class ScoreManager;

// -------------------- Quadtree (simple) --------------------
struct QTItem {
    CollisionBox box;
    void* ref; // pointer to entity (Car* or PowerUp*)
    int type;  // 1 = enemy, 2 = powerup, 3 = particle (unused for collisions)
};

class Quadtree {
    Rectangle bounds;
    int capacity;
    vector<QTItem> items;
    bool divided;
    Quadtree *nw, *ne, *sw, *se;
public:
    Quadtree(Rectangle b, int cap = 6)
        : bounds(b), capacity(cap), divided(false), nw(nullptr), ne(nullptr), sw(nullptr), se(nullptr) {}
    ~Quadtree() {
        clear();
    }
    void clear() {
        items.clear();
        if (divided) {
            delete nw; nw = nullptr;
            delete ne; ne = nullptr;
            delete sw; sw = nullptr;
            delete se; se = nullptr;
            divided = false;
        }
    }
    bool contains(const Rectangle &r, const CollisionBox &b) const {
        return !(b.x + b.w < r.x || b.x > r.x + r.width || b.y + b.h < r.y || b.y > r.y + r.height);
    }
    void subdivide() {
        float x = bounds.x, y = bounds.y, w = bounds.width/2, h = bounds.height/2;
        nw = new Quadtree({x, y, w, h}, capacity);
        ne = new Quadtree({x + w, y, w, h}, capacity);
        sw = new Quadtree({x, y + h, w, h}, capacity);
        se = new Quadtree({x + w, y + h, w, h}, capacity);
        divided = true;
    }
    void insert(const QTItem &it) {
        if (!contains(bounds, it.box)) return;
        if ((int)items.size() < capacity) {
            items.push_back(it);
            return;
        }
        if (!divided) subdivide();
        nw->insert(it); ne->insert(it); sw->insert(it); se->insert(it);
    }
    void query(const CollisionBox &area, vector<QTItem> &found) {
        if (!contains(bounds, area)) return;
        for (auto &it : items) {
            if (area.checkCollision(it.box)) found.push_back(it);
        }
        if (divided) {
            nw->query(area, found);
            ne->query(area, found);
            sw->query(area, found);
            se->query(area, found);
        }
    }
};

// -------------------- SceneManager --------------------
class SceneManager {
public:
    struct Building {
        int x, width, height;
        int cols, rows;
        vector<vector<bool>> windows;
    };

private:
    SceneType currentScene;
    int sceneTimer;
    float transitionAlpha;
    bool transitioning;
    vector<Building> buildings;
    bool buildingsInitialized;
    
    // Background textures
    Texture2D cityBg, highwayBg, desertBg, nightBg, forestBg, snowBg, sunsetBg, rainBg;
    bool texturesLoaded;

public:
    SceneManager()
        : currentScene(CITY), sceneTimer(0), transitionAlpha(0),
          transitioning(false), buildingsInitialized(false), texturesLoaded(false) {}
    
    ~SceneManager() {
        unloadTextures();
    }
    
    void loadTextures() {
        if (texturesLoaded) return;
        
        // Try to load background images from URLs or local files
        // For production, download these images and place them in assets folder
        // Using placeholder approach with Image generation for now
        
        cityBg = loadTextureFromURL("https://images.unsplash.com/photo-1480714378408-67cf0d13bc1b?w=1000&h=650&fit=crop");
        highwayBg = loadTextureFromURL("https://images.unsplash.com/photo-1449824913935-59a10b8d2000?w=1000&h=650&fit=crop");
        desertBg = loadTextureFromURL("https://images.unsplash.com/photo-1509316785289-025f5b846b35?w=1000&h=650&fit=crop");
        nightBg = loadTextureFromURL("https://images.unsplash.com/photo-1519681393784-d120267933ba?w=1000&h=650&fit=crop");
        forestBg = loadTextureFromURL("https://images.unsplash.com/photo-1441974231531-c6227db76b6e?w=1000&h=650&fit=crop");
        snowBg = loadTextureFromURL("https://images.unsplash.com/photo-1491002052546-bf38f186af56?w=1000&h=650&fit=crop");
        sunsetBg = loadTextureFromURL("https://images.unsplash.com/photo-1495567720989-cebdbdd97913?w=1000&h=650&fit=crop");
        rainBg = loadTextureFromURL("https://images.unsplash.com/photo-1428908728789-d2de25dbd4e2?w=1000&h=650&fit=crop");
        
        texturesLoaded = true;
    }
    
    Texture2D loadTextureFromURL(const char* url) {
        // In a real implementation, you would download the image
        // For now, create a simple gradient texture as fallback
        Image img = GenImageGradientLinear(SCREEN_WIDTH, SCREEN_HEIGHT, 0, SKYBLUE, DARKBLUE);
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        return tex;
    }
    
    void unloadTextures() {
        if (!texturesLoaded) return;
        UnloadTexture(cityBg);
        UnloadTexture(highwayBg);
        UnloadTexture(desertBg);
        UnloadTexture(nightBg);
        UnloadTexture(forestBg);
        UnloadTexture(snowBg);
        UnloadTexture(sunsetBg);
        UnloadTexture(rainBg);
        texturesLoaded = false;
    }

    SceneType getCurrentScene() const { return currentScene; }

    void update() {
        sceneTimer++;
        if (sceneTimer > 1200) {
            if (!transitioning) { transitioning = true; transitionAlpha = 0; }
        }
        if (transitioning) {
            transitionAlpha += 0.02f;
            if (transitionAlpha >= 1.0f) {
                currentScene = (SceneType)(((int)currentScene + 1) % 8);
                sceneTimer = 0;
                transitioning = false;
                transitionAlpha = 0;
                buildingsInitialized = false;
            }
        }
    }

    Color getRoadColor() const {
        switch(currentScene) {
            case CITY: return DARKGRAY;
            case HIGHWAY: return (Color){50,50,50,255};
            case DESERT: return (Color){139,90,43,255};
            case NIGHT: return (Color){30,30,40,255};
            case FOREST: return (Color){60,70,50,255};
            case SNOW: return (Color){200,200,220,255};
            case SUNSET: return (Color){80,60,50,255};
            case RAIN: return (Color){40,40,45,255};
            default: return DARKGRAY;
        }
    }

    Color getSkyColor() const {
        switch(currentScene) {
            case CITY: return SKYBLUE;
            case HIGHWAY: return (Color){135,206,235,255};
            case DESERT: return (Color){255,200,124,255};
            case NIGHT: return (Color){25,25,50,255};
            case FOREST: return (Color){100,180,100,255};
            case SNOW: return (Color){220,230,240,255};
            case SUNSET: return (Color){255,140,90,255};
            case RAIN: return (Color){80,90,100,255};
            default: return SKYBLUE;
        }
    }

    Color getLineColor() const {
        switch(currentScene) {
            case CITY: return YELLOW;
            case HIGHWAY: return WHITE;
            case DESERT: return (Color){255,255,150,255};
            case NIGHT: return (Color){255,255,100,255};
            case FOREST: return (Color){255,255,200,255};
            case SNOW: return (Color){255,200,0,255};
            case SUNSET: return (Color){255,220,150,255};
            case RAIN: return (Color){200,200,255,255};
            default: return YELLOW;
        }
    }

    const char* getSceneName() const {
        switch(currentScene) {
            case CITY: return "CITY";
            case HIGHWAY: return "HIGHWAY";
            case DESERT: return "DESERT";
            case NIGHT: return "NIGHT";
            case FOREST: return "FOREST";
            case SNOW: return "SNOW";
            case SUNSET: return "SUNSET";
            case RAIN: return "RAIN";
            default: return "UNKNOWN";
        }
    }

    void generateCityBuildings() {
        buildings.clear();
        int num = 6;
        int spacing = SCREEN_WIDTH / num;
        unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)sceneTimer;
        for (int i = 0; i < num; ++i) {
            int x = i * spacing + (seed % 15) - 7 + (i*3);
            int height = 120 + ((seed + i*37) % 200);
            int width = 140;
            Building b;
            b.x = x;
            b.width = width;
            b.height = height;
            b.cols = 5 + ((seed + i) % 3);
            b.rows = max(4, b.height / 30);
            b.windows.assign(b.rows, vector<bool>(b.cols, false));
            unsigned int s2 = seed + i*123;
            for (int r=0; r<b.rows; ++r) {
                for (int c=0; c<b.cols; ++c) {
                    unsigned int v = (s2 * (r+3) * (c+7) + 73) % 10;
                    b.windows[r][c] = (v >= 3);
                }
            }
            buildings.push_back(b);
        }
        buildingsInitialized = true;
    }

    void drawBackground() {
        if (getCurrentScene() == CITY && !buildingsInitialized) generateCityBuildings();

        Color sky = getSkyColor();
        
        // Draw gradient sky background
        for (int i = 0; i < SCREEN_HEIGHT/2; ++i) {
            float a = (float)i / (SCREEN_HEIGHT/2);
            Color grad = ColorAlpha(sky, 1.0f - a*0.3f);
            DrawRectangle(0, i, SCREEN_WIDTH, 1, grad);
        }

        DrawRectangle(0, (int)(SCREEN_HEIGHT * 0.5f), SCREEN_WIDTH, (int)(SCREEN_HEIGHT * 0.5f), Fade(getRoadColor(), 0.5f));

        // Scene-specific elements
        if (getCurrentScene() == CITY) {
            for (const Building &b : buildings) {
                DrawRectangle(b.x, (int)(SCREEN_HEIGHT * 0.5f) - b.height, b.width, b.height, GRAY);
                int wx = b.x + 10;
                int wy = (int)(SCREEN_HEIGHT * 0.5f) - b.height + 10;
                int cellW = (b.width - 20) / b.cols;
                int cellH = (b.height - 20) / b.rows;
                for (int r=0; r<b.rows; ++r) {
                    for (int c=0; c<b.cols; ++c) {
                        if (b.windows[r][c]) {
                            DrawRectangle(wx + c * cellW + 2, wy + r * cellH + 2, cellW - 4, cellH - 6, Fade(LIGHTGRAY, 0.9f));
                        } else {
                            DrawRectangleLines(wx + c * cellW + 2, wy + r * cellH + 2, cellW - 4, cellH - 6, Fade(DARKGRAY, 0.6f));
                        }
                    }
                }
            }
        } else if (getCurrentScene() == DESERT) {
            // Sun
            DrawCircle(SCREEN_WIDTH - 100, 100, 50, ORANGE);
            DrawCircle(SCREEN_WIDTH - 100, 100, 60, Fade(ORANGE, 0.25f));
            // Sand dunes
            for (int i = 0; i < 5; i++) {
                DrawCircle(i * 250 + 100, (int)(SCREEN_HEIGHT * 0.5f) - 20, 80, Fade((Color){210, 180, 140, 255}, 0.6f));
            }
            // Cacti
            DrawRectangle(150, (int)(SCREEN_HEIGHT * 0.5f) - 80, 20, 80, (Color){34, 139, 34, 255});
            DrawRectangle(135, (int)(SCREEN_HEIGHT * 0.5f) - 50, 30, 15, (Color){34, 139, 34, 255});
            DrawRectangle(700, (int)(SCREEN_HEIGHT * 0.5f) - 70, 18, 70, (Color){34, 139, 34, 255});
        } else if (getCurrentScene() == NIGHT) {
            // Moon
            DrawCircle(100, 80, 30, Fade(WHITE, 0.8f));
            DrawCircle(110, 75, 28, Fade((Color){25, 25, 50, 255}, 1.0f));
            // Stars
            for (int i=0;i<30;i++){
                int x=(i*123)%SCREEN_WIDTH;
                int y=(i*456)%300;
                DrawCircle(x,y,2,WHITE);
            }
        } else if (getCurrentScene() == FOREST) {
            // Trees in background
            for (int i = 0; i < 8; i++) {
                int x = i * 130 + 50;
                int h = 100 + (i * 17) % 50;
                DrawTriangle(
                    (Vector2){(float)x, (float)(SCREEN_HEIGHT * 0.5f) - h},
                    (Vector2){(float)(x - 40), (float)(SCREEN_HEIGHT * 0.5f)},
                    (Vector2){(float)(x + 40), (float)(SCREEN_HEIGHT * 0.5f)},
                    (Color){34, 139, 34, 200}
                );
                DrawRectangle(x - 10, (int)(SCREEN_HEIGHT * 0.5f) - h/3, 20, h/3, (Color){101, 67, 33, 255});
            }
            // Birds
            for (int i = 0; i < 5; i++) {
                int x = (i * 200 + sceneTimer) % SCREEN_WIDTH;
                int y = 50 + (i * 30) % 100;
                DrawText("^", x, y, 20, Fade(BLACK, 0.3f));
            }
        } else if (getCurrentScene() == SNOW) {
            // Mountains
            DrawTriangle(
                (Vector2){200, (float)(SCREEN_HEIGHT * 0.5f)},
                (Vector2){100, (float)(SCREEN_HEIGHT * 0.5f)},
                (Vector2){150, (float)(SCREEN_HEIGHT * 0.5f) - 120},
                (Color){200, 200, 220, 255}
            );
            DrawTriangle(
                (Vector2){400, (float)(SCREEN_HEIGHT * 0.5f)},
                (Vector2){250, (float)(SCREEN_HEIGHT * 0.5f)},
                (Vector2){325, (float)(SCREEN_HEIGHT * 0.5f) - 150},
                (Color){220, 220, 240, 255}
            );
            DrawTriangle(
                (Vector2){900, (float)(SCREEN_HEIGHT * 0.5f)},
                (Vector2){700, (float)(SCREEN_HEIGHT * 0.5f)},
                (Vector2){800, (float)(SCREEN_HEIGHT * 0.5f) - 130},
                (Color){210, 210, 230, 255}
            );
            // Falling snow
            for (int i = 0; i < 50; i++) {
                int x = (i * 77 + sceneTimer) % SCREEN_WIDTH;
                int y = (i * 93 + sceneTimer * 2) % SCREEN_HEIGHT;
                DrawCircle(x, y, 2, WHITE);
            }
        } else if (getCurrentScene() == SUNSET) {
            // Large sun on horizon
            DrawCircle((int)(SCREEN_WIDTH * 0.5f), (int)(SCREEN_HEIGHT * 0.5f) - 50, 80, (Color){255, 140, 0, 200});
            DrawCircle((int)(SCREEN_WIDTH * 0.5f), (int)(SCREEN_HEIGHT * 0.5f) - 50, 100, Fade((Color){255, 100, 0, 255}, 0.3f));
            // Clouds
            for (int i = 0; i < 4; i++) {
                int x = i * 250 + 50;
                int y = 100 + (i * 30) % 80;
                DrawCircle(x, y, 30, Fade((Color){255, 180, 120, 255}, 0.6f));
                DrawCircle(x + 30, y, 25, Fade((Color){255, 180, 120, 255}, 0.5f));
                DrawCircle(x - 20, y + 10, 20, Fade((Color){255, 180, 120, 255}, 0.4f));
            }
        } else if (getCurrentScene() == RAIN) {
            // Dark clouds
            for (int i = 0; i < 6; i++) {
                int x = i * 180 + (sceneTimer/2) % 180;
                int y = 50 + (i * 20) % 60;
                DrawCircle(x, y, 40, Fade((Color){60, 70, 80, 255}, 0.7f));
                DrawCircle(x + 30, y, 35, Fade((Color){60, 70, 80, 255}, 0.6f));
            }
            // Rain drops
            for (int i = 0; i < 100; i++) {
                int x = (i * 53) % SCREEN_WIDTH;
                int y = (i * 71 + sceneTimer * 8) % SCREEN_HEIGHT;
                DrawLine(x, y, x + 2, y + 10, Fade((Color){150, 180, 200, 255}, 0.5f));
            }
        }
    }
};

// -------------------- Car --------------------
class Car {
private:
    Position pos;
    Position target;
    float speed;
    int lane;
    Color color;
    bool isPlayer;
    float smooth;
public:
    Car(float x, float y, int laneIdx, float spd, Color c, bool player=false)
        : pos(x,y), target(x,y), speed(spd), lane(laneIdx), color(c), isPlayer(player), smooth(0.15f) {}
    void update(float speedMultiplier = 1.0f) {
        if (!isPlayer) pos.y += speed * speedMultiplier;
        else { pos.x += (target.x - pos.x) * smooth; pos.y += (target.y - pos.y) * smooth; }
    }
    void draw() const {
        DrawEllipse(pos.x, pos.y + 45, 30, 10, Fade(BLACK, 0.3f));
        DrawRectangle(pos.x - 30, pos.y - 50, 60, 100, color);
        DrawRectangleGradientV(pos.x - 30, pos.y - 50, 60, 40, Fade(WHITE,0.2f), Fade(BLACK,0.0f));
        if (isPlayer) {
            DrawRectangle(pos.x - 30, pos.y - 60, 60, 20, Fade(color, 0.8f));
            DrawTriangle((Vector2){pos.x, pos.y - 60}, (Vector2){pos.x - 30, pos.y - 40}, (Vector2){pos.x + 30, pos.y - 40}, RED);
            DrawRectangle(pos.x - 5, pos.y - 50, 10, 100, Fade(WHITE,0.7f));
        }
        Color wc = {100,150,200,200};
        DrawRectangle(pos.x-22, pos.y-30, 44, 25, wc);
        DrawRectangle(pos.x-22, pos.y-30, 44, 5, Fade(WHITE, 0.5f));
        Rectangle w1 = {pos.x - 35, pos.y - 35, 12, 20};
        Rectangle w2 = {pos.x + 23, pos.y - 35, 12, 20};
        Rectangle w3 = {pos.x - 35, pos.y + 15, 12, 20};
        Rectangle w4 = {pos.x + 23, pos.y + 15, 12, 20};
        DrawRectangleRounded(w1, 0.3f, 6, DARKGRAY);
        DrawRectangleRounded(w2, 0.3f, 6, DARKGRAY);
        DrawRectangleRounded(w3, 0.3f, 6, DARKGRAY);
        DrawRectangleRounded(w4, 0.3f, 6, DARKGRAY);
        if (isPlayer) {
            DrawRectangle(pos.x - 25, pos.y + 45, 18, 6, YELLOW);
            DrawRectangle(pos.x + 7, pos.y + 45, 18, 6, YELLOW);
            DrawCircle(pos.x - 16, pos.y + 48, 4, Fade(YELLOW,0.6f));
            DrawCircle(pos.x + 16, pos.y + 48, 4, Fade(YELLOW,0.6f));
        } else {
            DrawRectangle(pos.x - 25, pos.y - 48, 18, 6, RED);
            DrawRectangle(pos.x + 7, pos.y - 48, 18, 6, RED);
        }
    }
    CollisionBox box() const { return { pos.x - 30, pos.y - 50, 60, 100 }; }
    Position getPos() const { return pos; }
    int getLane() const { return lane; }
    void setLane(int l) { lane = l; }
    void setPos(float x, float y) { pos.x = x; pos.y = y; }
    void setTarget(float x, float y) { target.x = x; target.y = y; }
    void setSpeed(float s) { speed = s; }
};

// -------------------- PowerUp --------------------
class PowerUp {
private:
    Position pos;
    PowerUpType type;
    Color color;
    float rot, pulse;
    bool collected;
public:
    PowerUp(float x, float y, PowerUpType t) : pos(x,y), type(t), rot(0), pulse(0), collected(false) {
        switch(t) { case SHIELD: color = SKYBLUE; break; case SLOW_MOTION: color = PURPLE; break; case SCORE_MULTIPLIER: color = GOLD; break; case EXTRA_LIFE: color = RED; break; }
    }
    void update() { pos.y += 2.5f; rot += 3.0f; pulse += 0.08f; }
    void draw() const {
        if (collected) return;
        float psize = 30 + sin(pulse) * 5;
        DrawCircle(pos.x, pos.y, psize + 10, Fade(color, 0.2f));
        DrawCircle(pos.x, pos.y, psize + 5, Fade(color, 0.3f));
        DrawCircle(pos.x, pos.y, psize, Fade(color, 0.4f));
        Rectangle r = { pos.x - 17.5f, pos.y - 17.5f, 35, 35 };
        DrawRectanglePro(r, (Vector2){17.5f,17.5f}, rot, color);
        Rectangle i = { pos.x - 12.5f, pos.y - 12.5f, 25, 25 };
        DrawRectanglePro(i, (Vector2){12.5f,12.5f}, -rot*1.5f, Fade(WHITE, 0.5f));
        const char* s = "";
        switch(type) { case SHIELD: s="S"; break; case SLOW_MOTION: s="T"; break; case SCORE_MULTIPLIER: s="X"; break; case EXTRA_LIFE: s="H"; break; }
        DrawText(s, pos.x - 8, pos.y - 12, 25, WHITE);
    }
    CollisionBox box() const { return { pos.x - 20, pos.y - 20, 40, 40 }; }
    Position getPos() const { return pos; }
    PowerUpType getType() const { return type; }
    bool isCollected() const { return collected; }
    void setCollected(bool v) { collected = v; }
    void setPos(float x, float y) { pos.x = x; pos.y = y; }
};

// -------------------- ActivePowerUp --------------------
struct ActivePowerUp { PowerUpType type; float timeRemaining; ActivePowerUp(PowerUpType t, float tm): type(t), timeRemaining(tm) {} };

// -------------------- Thread-safe Job Queue --------------------
class JobQueue {
    vector<function<void()>> jobs;
    mutex mtx;
    condition_variable cv;
    thread worker;
    atomic<bool> running;
public:
    JobQueue(): running(true) {
        worker = thread([this]{
            while (running) {
                function<void()> job;
                {
                    unique_lock<mutex> lk(mtx);
                    cv.wait(lk, [this]{ return !jobs.empty() || !running; });
                    if (!running && jobs.empty()) break;
                    job = move(jobs.back());
                    jobs.pop_back();
                }
                try {
                    if (job) job();
                } catch (...) { /* swallow exceptions inside worker */ }
            }
        });
    }
    ~JobQueue() {
        shutdown();
    }
    void push(function<void()> job) {
        {
            lock_guard<mutex> lk(mtx);
            jobs.push_back(move(job));
        }
        cv.notify_one();
    }
    void shutdown() {
        if (running.exchange(false)) {
            cv.notify_all();
            if (worker.joinable()) worker.join();
        }
    }
};

// -------------------- Event Scheduler --------------------
struct Event {
    uint64_t tick;
    function<void()> action;
    bool operator>(const Event &o) const { return tick > o.tick; }
};
class EventScheduler {
    priority_queue<Event, vector<Event>, greater<Event>> pq;
    mutex mtx;
public:
    void scheduleAt(uint64_t tick, function<void()> action) {
        lock_guard<mutex> lk(mtx);
        pq.push(Event{tick, action});
    }
    void scheduleAfter(uint64_t nowTick, uint64_t afterFrames, function<void()> action) {
        scheduleAt(nowTick + afterFrames, action);
    }
    void process(uint64_t currentTick) {
        vector<Event> toRun;
        {
            lock_guard<mutex> lk(mtx);
            while (!pq.empty() && pq.top().tick <= currentTick) {
                toRun.push_back(pq.top()); pq.pop();
            }
        }
        for (auto &e : toRun) {
            if (e.action) e.action();
        }
    }
    void clear() {
        lock_guard<mutex> lk(mtx);
        while (!pq.empty()) pq.pop();
    }
};

// -------------------- EnemyManager --------------------
class EnemyManager {
private:
    vector<Car> enemies;
    int level;
public:
    EnemyManager(): level(1) {}
    const vector<Car>& getEnemies() const { return enemies; }
    int getLevel() const { return level; }
    void reset() { enemies.clear(); level = 1; }
    void setLevel(int newLevel) {
        if (newLevel < 1) newLevel = 1;
        if (newLevel > MAX_LEVEL) newLevel = MAX_LEVEL;
        level = newLevel;
    }
    static float laneCenterX(int lane) { return ROAD_X + 60 + lane * LANE_WIDTH; }

    void spawnAtLane(int chosen) {
        if (chosen < 0 || chosen >= NUM_LANES) return;
        float base = 2.2f;
        float speed = base + pow((float)level, 1.15f) * 0.16f + (rand()%100)/100.0f;
        Color colors[] = {RED, BLUE, GREEN, ORANGE, PURPLE, PINK, MAROON};
        enemies.push_back(Car(laneCenterX(chosen), -120.0f, chosen, speed, colors[rand()%7]));
    }

    int chooseSafeLane() {
        const float nearY = 200.0f;
        const float extendedY = 330.0f;
        vector<int> candidate;
        for (int lane = 0; lane < NUM_LANES; ++lane) {
            bool occupiedNear = false;
            for (auto &e : enemies) {
                if (e.getLane() == lane && e.getPos().y < nearY) { occupiedNear = true; break; }
            }
            if (!occupiedNear) candidate.push_back(lane);
        }
        vector<int> safe;
        for (int lane : candidate) {
            bool adjBlocked = false;
            for (auto &e : enemies) {
                if (e.getPos().y < extendedY && abs(e.getLane() - lane) == 1) { adjBlocked = true; break; }
            }
            if (!adjBlocked) safe.push_back(lane);
        }
        int chosen = -1;
        if (!safe.empty()) chosen = safe[rand() % safe.size()];
        else if (!candidate.empty()) chosen = candidate[rand() % candidate.size()];
        return chosen;
    }

    void update(bool slowMotion) {
        float speedMult = slowMotion ? 0.5f : 1.0f;
        for (size_t i=0;i<enemies.size();++i) {
            enemies[i].update(speedMult);
        }
        enemies.erase(remove_if(enemies.begin(), enemies.end(),
                    [](const Car &c){ return c.getPos().y > SCREEN_HEIGHT + 150; }), enemies.end());
    }

    void draw() const { for (auto &e : enemies) e.draw(); }
};

// -------------------- PowerUpManager --------------------
class PowerUpManager {
private:
    vector<PowerUp> list;
public:
    PowerUpManager() {}
    vector<PowerUp>& getPowerUps() { return list; }
    void reset() { list.clear(); }

    static float laneCenterX(int lane) { return ROAD_X + 60 + lane * LANE_WIDTH; }

    void spawnAtLane(int lane) {
        if (lane < 0 || lane >= NUM_LANES) return;
        PowerUpType types[] = { SHIELD, SLOW_MOTION, SCORE_MULTIPLIER, EXTRA_LIFE };
        PowerUpType t = types[rand() % 4];
        list.push_back(PowerUp(laneCenterX(lane), -80.0f, t));
    }

    int chooseFreeLaneBasedOnEnemies(const EnemyManager &enemyMgr) {
        const float safeY = 320.0f;
        vector<int> freeLanes;
        for (int lane = 0; lane < NUM_LANES; ++lane) {
            bool blocked = false;
            for (auto &e : enemyMgr.getEnemies()) {
                if (e.getLane() == lane && e.getPos().y < safeY) { blocked = true; break; }
            }
            if (!blocked) freeLanes.push_back(lane);
        }
        if (!freeLanes.empty()) return freeLanes[rand() % freeLanes.size()];
        return -1;
    }

    void update() {
        for (auto &p : list) p.update();
        list.erase(remove_if(list.begin(), list.end(),
            [](const PowerUp &u){ return u.getPos().y > SCREEN_HEIGHT + 120 || u.isCollected(); }), list.end());
    }
    void draw() const { for (auto &p : list) p.draw(); }
};

// -------------------- ScoreManager --------------------
class ScoreManager {
private:
    int currentScore;
    int highScore;
    priority_queue<int, vector<int>, greater<int>> topScores;
    int streak;
    int maxStreak;
    int multiplier;
public:
    ScoreManager(): currentScore(0), highScore(0), streak(0), maxStreak(0), multiplier(1) {
        load();
    }
    void addScore(int pts) { currentScore += pts * multiplier; streak++; if (streak > maxStreak) maxStreak = streak; }
    void resetStreak() { streak = 0; }
    void setMultiplier(int m) { multiplier = m; }
    int getCurrent() const { return currentScore; }
    int getHigh() const { return highScore; }
    int getStreak() const { return streak; }
    int getMaxStreak() const { return maxStreak; }
    vector<int> getTopScores() const {
        vector<int> v;
        auto copy = topScores;
        while (!copy.empty()) { v.push_back(copy.top()); copy.pop(); }
        sort(v.begin(), v.end(), greater<int>());
        return v;
    }
    void saveScoreAsync(JobQueue &jobQueue) {
        updateTopK(currentScore);
        vector<int> toWrite = getTopScores();
        jobQueue.push([toWrite](){
            ofstream f("traffic_scores.dat");
            if (f.is_open()) {
                for (auto &v : toWrite) f << v << endl;
                f.close();
            }
        });
    }
    void saveScoreSync() {
        updateTopK(currentScore);
        ofstream f("traffic_scores.dat");
        if (f.is_open()) {
            auto v = getTopScores();
            for (auto &s : v) f << s << endl;
            f.close();
        }
    }
    void reset() { currentScore = 0; streak = 0; multiplier = 1; }
private:
    void updateTopK(int score) {
        if ((int)topScores.size() < TOP_K_SCORES) topScores.push(score);
        else {
            if (score > topScores.top()) {
                topScores.pop();
                topScores.push(score);
            }
        }
        vector<int> tmp;
        {
            auto copy = topScores;
            while (!copy.empty()) { tmp.push_back(copy.top()); copy.pop(); }
        }
        if (!tmp.empty()) highScore = *max_element(tmp.begin(), tmp.end());
        else highScore = max(highScore, 0);
    }
    void load() {
        topScores = priority_queue<int, vector<int>, greater<int>>();
        ifstream f("traffic_scores.dat");
        if (f.is_open()) {
            int s; vector<int> loaded;
            while (f >> s) {
                loaded.push_back(s);
            }
            f.close();
            for (int s : loaded) {
                if ((int)topScores.size() < TOP_K_SCORES) topScores.push(s);
                else if (s > topScores.top()) { topScores.pop(); topScores.push(s); }
            }
        }
        vector<int> tmp;
        auto copy = topScores;
        while(!copy.empty()) { tmp.push_back(copy.top()); copy.pop(); }
        if (!tmp.empty()) highScore = *max_element(tmp.begin(), tmp.end()); else highScore = 0;
    }
};

// -------------------- TrafficRacingGame --------------------
class TrafficRacingGame {
private:
    Car player;
    EnemyManager enemyMgr;
    PowerUpManager powerUpMgr;
    ScoreManager scoreMgr;
    SceneManager sceneMgr;
    vector<ActivePowerUp> activePowerUps;
    GameState state;
    int lives;
    int currentLane;
    float roadOffset;
    uint64_t frameCount;
    float invincibilityTimer;
    int menuSelection;

    struct Particle { Vector2 pos, vel; Color col; float life, size; };
    vector<Particle> particles;
    
    // Camera shake
    float shakeIntensity;
    float shakeDuration;
    Vector2 shakeOffset;

    // Audio
    Music bgMusic;
    Sound sfxHit;
    Sound sfxPowerup;
    Sound sfxEngine;
    bool audioDeviceReady;
    bool hasMusic, hasSfxHit, hasSfxPowerup, hasSfxEngine;

    // New components
    Quadtree *qtRoot;
    EventScheduler scheduler;
    JobQueue jobQueue;

    mutex schedMtx;

    static float laneCenterX(int lane) { return ROAD_X + 60 + lane * LANE_WIDTH; }

    void triggerShake(float intensity, float duration) {
        shakeIntensity = intensity;
        shakeDuration = duration;
    }

    void updateCameraShake() {
        if (shakeDuration > 0) {
            shakeDuration -= 1.0f;
            float progress = shakeDuration / 30.0f;
            float currentIntensity = shakeIntensity * progress;
            shakeOffset.x = ((rand() % 200 - 100) / 100.0f) * currentIntensity;
            shakeOffset.y = ((rand() % 200 - 100) / 100.0f) * currentIntensity;
        } else {
            shakeOffset.x = 0;
            shakeOffset.y = 0;
        }
    }

    void createParticles(float x, float y, Color c, int count=20) {
        for (int i=0;i<count;++i) {
            Particle p;
            p.pos = { x, y };
            float ang = (rand()%360) * DEG2RAD;
            float sp = 1.5f + (rand()%100) / 60.0f;
            p.vel = { cos(ang)*sp, sin(ang)*sp };
            p.col = c; p.life = 1.0f; p.size = 2.0f + (rand()%50)/50.0f;
            particles.push_back(p);
        }
    }
    void updateParticles() {
        for (auto &p : particles) {
            p.pos.x += p.vel.x; p.pos.y += p.vel.y; p.vel.y += 0.15f;
            p.life -= 0.02f; p.size -= 0.02f;
        }
        particles.erase(remove_if(particles.begin(), particles.end(), [](const Particle &p){ return p.life <= 0 || p.size <= 0; }), particles.end());
    }
    void drawParticles() const { for (auto &p : particles) DrawCircleV(p.pos, p.size, Fade(p.col, p.life)); }

    void drawRoad() {
        Color roadColor = sceneMgr.getRoadColor();
        DrawRectangleGradientV(ROAD_X, 0, ROAD_WIDTH, SCREEN_HEIGHT, roadColor, Fade(roadColor, 0.7f));
        const float dashH = 30.0f, gapH = 22.0f;
        const float pattern = dashH + gapH;
        float offset = fmodf(roadOffset, pattern);
        if (offset < 0) offset += pattern;
        for (int lane = 1; lane < NUM_LANES; ++lane) {
            float x = ROAD_X + (lane * LANE_WIDTH);
            for (float y = -pattern; y < SCREEN_HEIGHT + pattern; y += pattern) {
                float yPos = y + offset;
                DrawRectangle((int)(x-4), (int)yPos, 8, (int)dashH, sceneMgr.getLineColor());
                DrawRectangle((int)(x-3), (int)(yPos+1), 6, (int)(dashH-2), Fade(WHITE, 0.5f));
            }
        }
        DrawRectangleGradientH(ROAD_X - 20, 0, 20, SCREEN_HEIGHT, BLACK, roadColor);
        DrawRectangleGradientH(ROAD_X + ROAD_WIDTH, 0, 20, SCREEN_HEIGHT, roadColor, BLACK);
        DrawRectangle(ROAD_X - 5, 0, 5, SCREEN_HEIGHT, WHITE);
        DrawRectangle(ROAD_X + ROAD_WIDTH, 0, 5, SCREEN_HEIGHT, WHITE);
        roadOffset += 6.0f;
        if (roadOffset > 1e6) roadOffset = fmodf(roadOffset, pattern);
    }

    void drawUI() {
        DrawRectangleGradientV(0, 0, SCREEN_WIDTH, 80, Fade(BLACK, 0.85f), Fade(BLACK, 0.6f));
        DrawText(TextFormat("SCORE: %d", scoreMgr.getCurrent()), 25, 15, 28, Fade(YELLOW, 0.45f));
        DrawText(TextFormat("SCORE: %d", scoreMgr.getCurrent()), 23, 13, 28, YELLOW);
        DrawText(TextFormat("BEST: %d", scoreMgr.getHigh()), 25, 45, 20, GOLD);
        DrawText("LIVES:", SCREEN_WIDTH - 270, 20, 22, WHITE);
        for (int i=0;i<3;i++){
            if (i < lives) { DrawCircle(SCREEN_WIDTH - 180 + (i*45), 35, 16, RED); DrawCircle(SCREEN_WIDTH - 180 + (i*45), 35, 12, Fade(PINK, 0.7f)); }
            else DrawCircleLines(SCREEN_WIDTH - 180 + (i*45), 35, 16, DARKGRAY);
        }
        DrawText(TextFormat("LEVEL %d", enemyMgr.getLevel()), 350, 20, 25, LIME);
        if (scoreMgr.getStreak() > 5) DrawText(TextFormat("STREAK x%d", scoreMgr.getStreak()), 550, 20, 22, ORANGE);
        DrawText(sceneMgr.getSceneName(), (int)(SCREEN_WIDTH * 0.5f) - 50, 50, 20, Fade(WHITE, 0.7f));
        int px = 20;
        for (size_t i=0;i<activePowerUps.size();i++){
            const char* txt = ""; Color c = WHITE;
            switch(activePowerUps[i].type){ case SHIELD: txt="SHIELD"; c = SKYBLUE; break; case SLOW_MOTION: txt="SLOW-MO"; c = PURPLE; break; case SCORE_MULTIPLIER: txt="2X SCORE"; c = GOLD; break; default: break; }
            Rectangle r = {(float)px, (float)SCREEN_HEIGHT - 50, 110, 35};
            DrawRectangleRounded(r, 0.3f, 6, Fade(c, 0.6f));
            DrawRectangleRoundedLines(r, 0.3f, 6, c);
            DrawText(txt, px + 12, SCREEN_HEIGHT - 43, 18, WHITE);
            float prog = activePowerUps[i].timeRemaining / 300.0f;
            Rectangle pr = {(float)px + 5, (float)SCREEN_HEIGHT - 20, 100 * prog, 6};
            if (prog > 0.0001f) DrawRectangleRounded(pr, 0.5f, 4, c);
            px += 120;
        }
        DrawRectangle(0, SCREEN_HEIGHT - 35, SCREEN_WIDTH, 35, Fade(BLACK, 0.7f));
        DrawText("Arrow Keys or A/D: Move |  Q: Quit", (int)(SCREEN_WIDTH * 0.5f) - 250, SCREEN_HEIGHT - 25, 18, LIGHTGRAY);
    }

    void getMenuRects(Rectangle out[3], int sel){
        const float w = 300, h = 60;
        for (int i=0;i<3;i++){
            float offset = (i == sel) ? 10 : 0;
            out[i] = { (float)(SCREEN_WIDTH * 0.5f - w/2) + offset, 300.0f + i*80, w - offset*2, h };
        }
    }

    bool hasShield() const { for (auto &ap : activePowerUps) if (ap.type == SHIELD) return true; return false; }
    bool hasSlowMotion() const { for (auto &ap : activePowerUps) if (ap.type == SLOW_MOTION) return true; return false; }

    void checkCollisions() {
        if (invincibilityTimer > 0.0f) { invincibilityTimer -= 1.0f; return; }
        CollisionBox pbox = player.box();
        vector<QTItem> candidates;
        qtRoot->query(pbox, candidates);

        for (auto &it : candidates) {
            if (it.type == 1) {
                Car *e = (Car*)it.ref;
                if (pbox.checkCollision(e->box())) {
                    if (hasShield()) {
                        for (size_t k=0;k<activePowerUps.size();k++){
                            if (activePowerUps[k].type == SHIELD) { activePowerUps.erase(activePowerUps.begin() + k); break; }
                        }
                        createParticles(player.getPos().x, player.getPos().y, SKYBLUE, 30);
                        triggerShake(8.0f, 15.0f);
                        if (hasSfxHit) PlaySound(sfxHit);
                    } else {
                        lives--; scoreMgr.resetStreak(); createParticles(player.getPos().x, player.getPos().y, RED, 40);
                        triggerShake(15.0f, 30.0f);
                        if (hasSfxHit) PlaySound(sfxHit);
                        if (lives <= 0) { state = GAME_OVER; scoreMgr.saveScoreAsync(jobQueue); }
                    }
                    invincibilityTimer = 80.0f;
                    break;
                }
            }
        }

        for (auto &it : candidates) {
            if (it.type == 2) {
                PowerUp *pu = (PowerUp*)it.ref;
                if (!pu->isCollected() && pbox.checkCollision(pu->box())) {
                    PowerUpType t = pu->getType();
                    pu->setCollected(true);
                    createParticles(pu->getPos().x, pu->getPos().y, GOLD, 28);
                    triggerShake(3.0f, 8.0f);
                    if (hasSfxPowerup) PlaySound(sfxPowerup);
                    switch(t) {
                        case SHIELD: activePowerUps.push_back(ActivePowerUp(SHIELD, 350)); break;
                        case SLOW_MOTION: activePowerUps.push_back(ActivePowerUp(SLOW_MOTION, 250)); break;
                        case SCORE_MULTIPLIER: activePowerUps.push_back(ActivePowerUp(SCORE_MULTIPLIER, 300)); scoreMgr.setMultiplier(2); break;
                        case EXTRA_LIFE: if (lives < 3) lives++; scoreMgr.addScore(50); break;
                    }
                }
            }
        }
    }

    void updatePowerUps() {
        for (size_t i=0;i<activePowerUps.size(); ++i) {
            activePowerUps[i].timeRemaining -= 1.0f;
            if (activePowerUps[i].timeRemaining <= 0.0f) {
                if (activePowerUps[i].type == SCORE_MULTIPLIER) scoreMgr.setMultiplier(1);
                activePowerUps.erase(activePowerUps.begin() + i); i--;
            }
        }
    }

    void handleInput() {
        int newLane = currentLane;
        if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) && currentLane > 0) newLane--;
        if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) && currentLane < NUM_LANES - 1) newLane++;
        if (newLane != currentLane) {
            currentLane = newLane;
            float nx = laneCenterX(currentLane);
            player.setTarget(nx, player.getPos().y);
            player.setLane(currentLane);
        }
        if (IsKeyPressed(KEY_ESCAPE)) state = PAUSED;
        if (IsKeyPressed(KEY_Q)) { state = MENU; scoreMgr.saveScoreAsync(jobQueue); }
    }

    void drawMenu() {
        sceneMgr.drawBackground();
        DrawText("TRAFFIC RACER", (int)(SCREEN_WIDTH * 0.5f) - 250, 100, 70, Fade(YELLOW, 0.5f));
        DrawText("TRAFFIC RACER", (int)(SCREEN_WIDTH * 0.5f) - 253, 97, 70, YELLOW);
        DrawText("DSA PROJECT", (int)(SCREEN_WIDTH * 0.5f) - 110, 180, 30, GOLD);
        const char* options[] = { "START GAME", "VIEW SCORES", "QUIT" };
        Rectangle r[3]; getMenuRects(r, menuSelection);
        for (int i=0;i<3;i++){
            Color c = (i==menuSelection) ? LIME : WHITE;
            DrawRectangleRounded(r[i], 0.3f, 6, Fade(c, 0.3f));
            DrawRectangleRoundedLines(r[i], 0.3f, 6, c);
            int tw = MeasureText(options[i], 30);
            DrawText(options[i], (int)(r[i].x + r[i].width/2 - tw/2), (int)(r[i].y + 15), 30, c);
        }
        DrawText("Use UP/DOWN arrows to navigate, ENTER to select or click the item", (int)(SCREEN_WIDTH * 0.5f) - 320, SCREEN_HEIGHT - 100, 18, LIGHTGRAY);
        DrawText("Features: 8 Dynamic Scenes | Camera Shake | Smooth Slow Motion", (int)(SCREEN_WIDTH * 0.5f) - 310, SCREEN_HEIGHT - 60, 18, DARKGRAY);
    }

    void drawScoresScreen() {
        sceneMgr.drawBackground();
        Rectangle stats = {150, 180, SCREEN_WIDTH - 300, 350};
        DrawRectangleRounded(stats, 0.2f, 6, Fade(BLACK, 0.8f));
        DrawRectangleRoundedLines(stats, 0.2f, 6, GOLD);
        DrawText("TOP SCORES", (int)(SCREEN_WIDTH * 0.5f) - 80, 200, 40, YELLOW);
        const vector<int> ts = scoreMgr.getTopScores();
        for (size_t i=0;i<min((size_t)8, ts.size()); ++i) {
            DrawText(TextFormat("%d. %d", (int)i+1, ts[i]), 260, 260 + (int)i * 30, 26, WHITE);
        }
        DrawText("Press ENTER or ESC to return", (int)(SCREEN_WIDTH * 0.5f) - 160, SCREEN_HEIGHT - 80, 22, LIGHTGRAY);
    }

    void drawPauseScreen() {
        DrawRectangle(0,0,SCREEN_WIDTH,SCREEN_HEIGHT, Fade(BLACK, 0.7f));
        DrawText("PAUSED", (int)(SCREEN_WIDTH * 0.5f) - 100, SCREEN_HEIGHT/2 - 80, 50, YELLOW);
        DrawText("Press ESC to Resume", (int)(SCREEN_WIDTH * 0.5f) - 150, SCREEN_HEIGHT/2, 25, WHITE);
        DrawText("Press Q to Quit to Menu", (int)(SCREEN_WIDTH * 0.5f) - 150, SCREEN_HEIGHT/2 + 40, 22, WHITE);
    }

    void drawGameOver() {
        sceneMgr.drawBackground();
        DrawText("GAME OVER", (int)(SCREEN_WIDTH * 0.5f) - 200, 80, 60, RED);
        Rectangle statsBox = {150, 180, SCREEN_WIDTH - 300, 350};
        DrawRectangleRounded(statsBox, 0.2f, 6, Fade(BLACK, 0.85f));
        DrawRectangleRoundedLines(statsBox, 0.2f, 6, GOLD);
        DrawText("FINAL STATISTICS", (int)(SCREEN_WIDTH * 0.5f) - 140, 210, 30, YELLOW);
        DrawText(TextFormat("Final Score: %d", scoreMgr.getCurrent()), 200, 270, 28, LIME);
        DrawText(TextFormat("High Score: %d", scoreMgr.getHigh()), 200, 310, 25, GOLD);
        DrawText(TextFormat("Max Streak: %d", scoreMgr.getMaxStreak()), 200, 350, 25, ORANGE);
        DrawText(TextFormat("Level Reached: %d", enemyMgr.getLevel()), 200, 390, 25, SKYBLUE);
        DrawText("TOP 5 SCORES", 500, 270, 25, PURPLE);
        const vector<int> s = scoreMgr.getTopScores();
        for (size_t i=0;i<min((size_t)5, s.size()); ++i) DrawText(TextFormat("%d. %d", (int)i+1, s[i]), 520, 310 + (int)i * 30, 20, WHITE);
        DrawText("Press ENTER to return to menu", (int)(SCREEN_WIDTH * 0.5f) - 200, SCREEN_HEIGHT - 80, 22, LIGHTGRAY);
    }

    void resetGame() {
        lives = 3; currentLane = 2; roadOffset = 0; frameCount = 0; invincibilityTimer = 0;
        shakeIntensity = 0; shakeDuration = 0; shakeOffset = {0, 0};
        float sx = laneCenterX(currentLane);
        player.setPos(sx, SCREEN_HEIGHT - 150); player.setTarget(sx, SCREEN_HEIGHT - 150);
        enemyMgr.reset(); powerUpMgr.reset(); scoreMgr.reset(); activePowerUps.clear(); particles.clear();
        sceneMgr = SceneManager();

        scheduler.clear();
        if (qtRoot) { delete qtRoot; qtRoot = nullptr; }
        qtRoot = new Quadtree({0,0,(float)SCREEN_WIDTH,(float)SCREEN_HEIGHT}, 8);

        scheduleEnemySpawn(frameCount + 20);
        schedulePowerupSpawn(frameCount + 350);

        if (hasMusic) {
            StopMusicStream(bgMusic);
            PlayMusicStream(bgMusic);
        }
    }

    void initAudio() {
        InitAudioDevice();
        audioDeviceReady = IsAudioDeviceReady();

        hasMusic = hasSfxHit = hasSfxPowerup = hasSfxEngine = false;

        if (!audioDeviceReady) return;

        const char *musicPath   = "src/assets/music.mp3";
        const char *hitPath     = "src/assets/hit.wav";
        const char *powerPath   = "src/assets/powerup.wav";
        const char *enginePath  = "src/assets/engine.wav";

        if (FileExists(musicPath)) {
            bgMusic = LoadMusicStream(musicPath);
            PlayMusicStream(bgMusic);
            hasMusic = true;
        }

        if (FileExists(hitPath)) {
            sfxHit = LoadSound(hitPath);
            hasSfxHit = true;
        }

        if (FileExists(powerPath)) {
            sfxPowerup = LoadSound(powerPath);
            hasSfxPowerup = true;
        }

        if (FileExists(enginePath)) {
            sfxEngine = LoadSound(enginePath);
            hasSfxEngine = true;
        }
    }

    void updateAudio() {
        if (!audioDeviceReady) return;
        if (hasMusic) UpdateMusicStream(bgMusic);
    }

    void unloadAudio() {
        if (!audioDeviceReady) return;
        if (hasMusic) { StopMusicStream(bgMusic); UnloadMusicStream(bgMusic); hasMusic = false; }
        if (hasSfxHit) { StopSound(sfxHit); UnloadSound(sfxHit); hasSfxHit = false; }
        if (hasSfxPowerup) { StopSound(sfxPowerup); UnloadSound(sfxPowerup); hasSfxPowerup = false; }
        if (hasSfxEngine) { StopSound(sfxEngine); UnloadSound(sfxEngine); hasSfxEngine = false; }
        CloseAudioDevice();
        audioDeviceReady = false;
    }

    void scheduleEnemySpawn(uint64_t atTick) {
        scheduler.scheduleAt(atTick, [this, atTick]() {
            int chosen = enemyMgr.chooseSafeLane();
            if (chosen != -1) enemyMgr.spawnAtLane(chosen);
            float spawnBase = 85.0f - enemyMgr.getLevel() * 0.65f;
            float spawn = std::max(7.0f, spawnBase + (rand()%20 - 10));
            uint64_t frames = (uint64_t)std::max(7.0f, spawn);
            scheduleEnemySpawn(atTick + frames);
        });
    }

    void schedulePowerupSpawn(uint64_t atTick) {
        scheduler.scheduleAt(atTick, [this, atTick]() {
            int lane = powerUpMgr.chooseFreeLaneBasedOnEnemies(enemyMgr);
            if (lane != -1) powerUpMgr.spawnAtLane(lane);
            uint64_t next = 500 + (rand()%300);
            schedulePowerupSpawn(atTick + next);
        });
    }

public:
    TrafficRacingGame()
        : player(ROAD_X + 60 + (2 * LANE_WIDTH), SCREEN_HEIGHT - 150, 2, 0.0f, GREEN, true),
          state(MENU), lives(3), currentLane(2), roadOffset(0), frameCount(0), invincibilityTimer(0),
          menuSelection(0), shakeIntensity(0), shakeDuration(0), shakeOffset({0, 0}),
          audioDeviceReady(false), hasMusic(false), hasSfxHit(false), hasSfxPowerup(false), hasSfxEngine(false),
          qtRoot(nullptr)
    {
        srand((unsigned)time(NULL));
        qtRoot = new Quadtree({0,0,(float)SCREEN_WIDTH,(float)SCREEN_HEIGHT}, 8);
    }

    ~TrafficRacingGame() {
        if (qtRoot) delete qtRoot;
        jobQueue.shutdown();
    }

    void run() {
        InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Traffic Racer - DSA Upgraded");
        SetTargetFPS(FRAME_RATE);
        initAudio();
        sceneMgr.loadTextures(); // Load background textures

        bool running = true;
        while (running && !WindowShouldClose()) {
            scheduler.process(frameCount);

            switch (state) {
                case MENU: {
                    sceneMgr.update();
                    if (IsKeyPressed(KEY_UP)) menuSelection = (menuSelection - 1 + 3) % 3;
                    if (IsKeyPressed(KEY_DOWN)) menuSelection = (menuSelection + 1) % 3;
                    Rectangle mr[3]; getMenuRects(mr, menuSelection);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        Vector2 mp = { (float)GetMouseX(), (float)GetMouseY() };
                        for (int i=0;i<3;i++) {
                            if (CheckCollisionPointRec(mp, mr[i])) {
                                menuSelection = i;
                                if (i==0) { resetGame(); state = PLAYING; }
                                else if (i==1) { state = SCORES; }
                                else if (i==2) { running = false; }
                            }
                        }
                    }
                    if (IsKeyPressed(KEY_ENTER)) {
                        if (menuSelection == 0) { resetGame(); state = PLAYING; }
                        else if (menuSelection == 1) state = SCORES;
                        else if (menuSelection == 2) running = false;
                    }
                } break;

                case PLAYING: {
                    handleInput();
                    sceneMgr.update();
                    player.update();
                    updateCameraShake();

                    int desiredLevel = 1 + (scoreMgr.getCurrent() / LEVEL_SCORE_INTERVAL);
                    if (desiredLevel > MAX_LEVEL) desiredLevel = MAX_LEVEL;
                    if (desiredLevel != enemyMgr.getLevel()) {
                        enemyMgr.setLevel(desiredLevel);
                        if (hasSfxEngine) PlaySound(sfxEngine);
                    }

                    bool slow = hasSlowMotion();
                    enemyMgr.update(slow);
                    powerUpMgr.update();

                    if (qtRoot) { qtRoot->clear(); }
                   else qtRoot = new Quadtree({0,0,(float)SCREEN_WIDTH,(float)SCREEN_HEIGHT}, 8);

                    for (auto &e : enemyMgr.getEnemies()) {
                 
                        QTItem it; it.box = e.box(); it.ref = (void*)&e; it.type = 1;
                        qtRoot->insert(it);
                    }
                    for (auto &p : powerUpMgr.getPowerUps()) {
                        QTItem it; it.box = p.box(); it.ref = (void*)&p; it.type = 2;
                        qtRoot->insert(it);
                    }

                    checkCollisions();
                    updatePowerUps();
                    updateParticles();

                    frameCount++;
                    if (frameCount % 25 == 0) scoreMgr.addScore(10);
                    if (frameCount % 350 == 0) { scoreMgr.addScore(150); }

                    updateAudio();
                } break;

                case PAUSED:
                    if (IsKeyPressed(KEY_ESCAPE)) state = PLAYING;
                    if (IsKeyPressed(KEY_Q)) { state = MENU; scoreMgr.saveScoreAsync(jobQueue); }
                    updateAudio();
                    break;

                case GAME_OVER:
                    if (IsKeyPressed(KEY_ENTER)) { state = MENU; menuSelection = 0; }
                    updateAudio();
                    break;

                case SCORES:
                    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) state = MENU;
                    updateAudio();
                    break;
            }

            BeginDrawing();
            ClearBackground(RAYWHITE);

            switch (state) {
                case MENU: drawMenu(); break;
                case PLAYING:
                    if (shakeDuration > 0) {
                        BeginMode2D(Camera2D{{0, 0}, {shakeOffset.x, shakeOffset.y}, 0, 1.0f});
                    }
                    
                    sceneMgr.drawBackground();
                    drawRoad();
                    enemyMgr.draw();
                    powerUpMgr.draw();
                    if (invincibilityTimer <= 0 || (frameCount % 12 < 6)) player.draw();
                    if (hasShield() && frameCount % 20 < 10) {
                        DrawCircleLines(player.getPos().x, player.getPos().y, 60, SKYBLUE);
                        DrawCircleLines(player.getPos().x, player.getPos().y, 65, Fade(SKYBLUE,0.5f));
                    }
                    drawParticles();
                    
                    if (shakeDuration > 0) {
                        EndMode2D();
                    }
                    
                    drawUI();
                    break;
                case PAUSED:
                    sceneMgr.drawBackground();
                    drawRoad();
                    enemyMgr.draw();
                    powerUpMgr.draw();
                    player.draw();
                    drawUI();
                    drawPauseScreen();
                    break;
                case GAME_OVER: drawGameOver(); break;
                case SCORES: drawScoresScreen(); break;
            }

            EndDrawing();
        }

        scoreMgr.saveScoreAsync(jobQueue);
        jobQueue.shutdown();

        unloadAudio();
        CloseWindow();
    }
};

int main() {
    TrafficRacingGame game;
    game.run();
    return 0;
}
