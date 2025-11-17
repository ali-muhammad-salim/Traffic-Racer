# Traffic Racer Game -- Project Report

## Submitted By

-   **Ali Muhammad Salim** -- CT-24083\
-   **Abdul Rahim** -- CT-24088\
-   **Muhammad Fouzan Abdul Aziz** -- CT-24090\
-   **Syed Irtaza Shahid Zaidi** -- CT-24072

------------------------------------------------------------------------

## 1. Introduction

This report presents the design and implementation of a **Traffic
Racer** game developed using **C++ and Raylib**.\
The project demonstrates: - Object‑oriented programming\
- Real‑time rendering\
- Game state management\
- Collision detection\
- Threading\
- Multiple optimized data structures for smooth gameplay

------------------------------------------------------------------------

## 2. Project Overview

Traffic Racer is a **lane‑based racing simulation** where the player: -
Avoids incoming vehicles\
- Collects power‑ups\
- Earns points\
- Progresses through increasingly challenging levels

The game includes: - Animated backgrounds\
- Dynamic scenes\
- Power‑up effects\
- Particle systems\
- Sound effects\
- Scoring system with high‑score tracking

------------------------------------------------------------------------

## 3. System Architecture

The project uses modular classes for clean and scalable design:

-   **Car**\
-   **EnemyManager**\
-   **PowerUpManager**\
-   **ScoreManager**\
-   **SceneManager**\
-   **TrafficRacingGame**

Each class handles well‑defined responsibilities to ensure
maintainability.

------------------------------------------------------------------------

## 4. Data Structures Used

### **Vector (`std::vector`)**

Used for dynamic lists (enemies, power‑ups, particles, jobs).\
Provides fast iteration and amortized O(1) insertion.

### **Quadtree (Custom Implementation)**

Improves collision‑detection efficiency by checking only nearby objects.

### **Priority Queue (`std::priority_queue`)**

Used in the event scheduler for timed enemy/power‑up spawns.

### **Thread‑Safe Job Queue**

Implemented with **mutex** + **condition_variable**, handles
asynchronous file I/O.

### **CollisionBox Struct**

Used for fast AABB collision detection.

### **Enums**

Used for: 
- Game states\
- Scene types\
- Power‑up categories

------------------------------------------------------------------------

## 5. Algorithms Implemented

-   Lane selection algorithm for safe enemy spawning\
-   Power‑up spawn logic avoiding conflict with enemies\
-   Quadtree-based efficient collision detection\
-   Event scheduler with timed callbacks\
-   Level progression algorithm that increases difficulty dynamically

------------------------------------------------------------------------

## 6. Game Features

-   **8 dynamic background scenes**\
-   Smooth lane-switching animation\
-   Power-ups: Shield, Slow Motion, Score Multiplier, Extra Life\
-   Camera shake on collision\
-   Particle-based explosion effects\
-   High‑score saving (asynchronous)\
-   Optimized collision system

------------------------------------------------------------------------

## 7. Conclusion

The Traffic Racer project demonstrates how **data structures**,
**algorithms**, and **OOP principles** combine to build a complete
real‑time game.\
The use of **Quadtrees** and **async job queues** highlights the
importance of performance optimization in game development.

------------------------------------------------------------------------

## 8. References

-   Raylib Documentation\
-   C++ Standard Library\
-   OOP Concepts\
-   Data Structures & Algorithms Theory
