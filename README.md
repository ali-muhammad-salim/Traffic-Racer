# 🚗 Traffic Racer – DSA Based 2D Game (C++ & Raylib)

Traffic Racer is a 2D lane-based racing game built using **C++** and **Raylib**, designed to showcase real-time gaming techniques powered by **Data Structures & Algorithms (DSA)**.

This project is optimized, clean, fast, and perfect for academic submission.

---

## 🎮 Game Features

- Five-lane endless highway
- Smooth road scrolling animation
- Increasing difficulty with dynamic levels
- Enemy cars with adaptive speed
- Power-ups:
  - Shield
  - Slow Motion
  - Double Score
  - Extra Life
- Collision detection
- Top 10 score saving system (auto-updates)
- Menus: Main Menu, Pause, Game Over, Scoreboard
- Uses modern C++ with Raylib 5.5

---

## 🧠 Data Structures & Algorithms Used

### 1. Quadtree – Spatial Partitioning
Used to optimize collision detection.

### 2. Priority Queue (Min-Heap) – Event Scheduler
Handles timed events like enemy and power-up spawning.

### 3. Priority Queue (Min-Heap) – Top Scores
Stores only Top 10 high scores.

### 4. Producer–Consumer Algorithm (JobQueue)
Background thread system for saving scores.

### 5. AABB Collision Detection Algorithm
Used for detecting collisions between player and in-game objects.

### 6. Lane Safe-Selection Algorithm
Ensures enemies spawn in safe lanes.

### 7. Road Scroll Algorithm
Provides infinite road movement effect.

### 8. Level Progression Algorithm
Increases difficulty level based on score.

---

## 🕹 Controls
- Left / A → Move left
- Right / D → Move right
- ESC → Pause / Resume
- Enter → Select
- Q → Quit

---

## ⚡ Power-Ups
- Shield
- Slow Motion
- Double Score
- Extra Life

---

## 💾 File Saving
High scores saved to:
traffic_scores.dat

---

## 🛠 How to Compile
Linux / Mac / MinGW:
g++ main.cpp -lraylib -pthread -o TrafficRacer

Windows (MSVC):
cl main.cpp /EHsc raylib.lib

---

## 📂 Project Structure
main.cpp  
traffic_scores.dat  
README.md  

---

## 👨‍💻 Authors
Ali Muhammad Salim - CT-24083   
Abdul Rahim - CT-24088   
Muhammad Fouzan Abdul Aziz - CT-24090   
Syed Irtaza Shahid Zaidi - CT-24072   
**Traffic Racer – DSA Edition** 

