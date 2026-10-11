#ifndef VEHICLE_INFLOW_H
#define VEHICLE_INFLOW_H

#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <random>

enum Direction {
    NORTH = 0,
    SOUTH = 1,
    EAST = 2,
    WEST = 3
};

inline std::string directionToString(Direction dir) {
    switch (dir) {
        case NORTH: return "NORTH";
        case SOUTH: return "SOUTH";
        case EAST:  return "EAST";
        case WEST:  return "WEST";
        default:    return "UNKNOWN";
    }
}

struct Vehicle {
    int id;
    Direction approach;
    int arrivalTick;
    bool isEmergency;
    int waitTicks;

    Vehicle(int vId, Direction dir, int tick, bool emergency = false)
        : id(vId), approach(dir), arrivalTick(tick), isEmergency(emergency), waitTicks(0) {}
};

class LaneQueue {
private:
    std::queue<Vehicle> buffer;
    Direction direction;
    int totalCleared;
    int cumulativeWaitTime;

public:
    LaneQueue(Direction dir = NORTH) 
        : direction(dir), totalCleared(0), cumulativeWaitTime(0) {}

    void enqueueVehicle(const Vehicle& v) {
        buffer.push(v);
    }

    Vehicle dequeueVehicle(int currentTick) {
        if (buffer.empty()) {
            throw std::runtime_error("Attempted dequeue on empty lane queue.");
        }
        Vehicle front = buffer.front();
        buffer.pop();
        front.waitTicks = currentTick - front.arrivalTick;
        totalCleared++;
        cumulativeWaitTime += front.waitTicks;
        return front;
    }

    bool hasEmergencyVehicle() const {
        std::queue<Vehicle> temp = buffer;
        while (!temp.empty()) {
            if (temp.front().isEmergency) {
                return true;
            }
            temp.pop();
        }
        return false;
    }

    size_t size() const {
        return buffer.size();
    }

    bool empty() const {
        return buffer.empty();
    }

    Direction getDirection() const {
        return direction;
    }

    int getTotalCleared() const {
        return totalCleared;
    }

    int getCumulativeWaitTime() const {
        return cumulativeWaitTime;
    }

    int computeAccumulatedWait(int currentTick) const {
        int total = 0;
        std::queue<Vehicle> temp = buffer;
        while (!temp.empty()) {
            total += (currentTick - temp.front().arrivalTick);
            temp.pop();
        }
        return total;
    }
};

class ArrivalSimulator {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<double> distProb;
    std::uniform_real_distribution<double> emergencyProb;
    int nextVehicleId;

public:
    ArrivalSimulator(unsigned int seed = 42)
        : rng(seed), distProb(0.0, 1.0), emergencyProb(0.0, 1.0), nextVehicleId(1001) {}

    void step(std::vector<LaneQueue>& lanes, int currentTick, double arrivalRate = 0.45) {
        for (int i = 0; i < 4; ++i) {
            if (distProb(rng) < arrivalRate) {
                bool isEmergency = (emergencyProb(rng) < 0.05); // 5% chance of emergency vehicle
                Vehicle v(nextVehicleId++, static_cast<Direction>(i), currentTick, isEmergency);
                lanes[i].enqueueVehicle(v);
            }
        }
    }
};

#endif // VEHICLE_INFLOW_H
