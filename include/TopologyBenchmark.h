#ifndef TOPOLOGY_BENCHMARK_H
#define TOPOLOGY_BENCHMARK_H

#include "VehicleInflow.h"
#include "SignalOptimizer.h"
#include <stack>
#include <list>
#include <iomanip>

class IntersectionTopology {
private:
    // Directed Adjacency List: Nodes represent approaches (0-3) and intersection center (4)
    std::vector<std::vector<int>> adjList;

public:
    IntersectionTopology() {
        adjList.resize(5);
        for (int i = 0; i < 4; ++i) {
            adjList[i].push_back(4); // Approaches 0..3 lead directly to junction 4
        }
    }

    bool hasDirectPath(int source, int destination) const {
        if (source < 0 || source >= 5) return false;
        for (int neighbor : adjList[source]) {
            if (neighbor == destination) return true;
        }
        return false;
    }
};

enum LightState {
    RED,
    YELLOW,
    GREEN
};

struct PhaseTransitionRecord {
    Direction direction;
    LightState state;
    int transitionTick;
};

class IntersectionController {
private:
    std::stack<PhaseTransitionRecord> phaseHistory;
    std::list<Vehicle> crossingZone; // Linked List tracking active vehicles clearing conflict zone
    Direction activeDirection;
    LightState currentState;
    int stateRemainingTicks;

public:
    IntersectionController()
        : activeDirection(NORTH), currentState(RED), stateRemainingTicks(0) {}

    void setPhase(Direction dir, LightState state, int duration, int currentTick) {
        activeDirection = dir;
        currentState = state;
        stateRemainingTicks = duration;
        phaseHistory.push({dir, state, currentTick});
    }

    bool isCycleComplete() const {
        return stateRemainingTicks <= 0;
    }

    void decrementTimer() {
        if (stateRemainingTicks > 0) stateRemainingTicks--;
    }

    Direction getActiveDirection() const {
        return activeDirection;
    }

    LightState getCurrentState() const {
        return currentState;
    }

    void admitToCrossingZone(const Vehicle& v) {
        crossingZone.push_back(v);
    }

    void updateCrossingZone() {
        // Linked list constant-time traversal and removal as cars exit
        for (auto it = crossingZone.begin(); it != crossingZone.end();) {
            it = crossingZone.erase(it);
        }
    }

    size_t getCrossingCount() const {
        return crossingZone.size();
    }
};

class BenchmarkEngine {
public:
    struct SimulationResults {
        std::string mode;
        int totalVehiclesCleared;
        double averageWaitTicks;
        int maxQueueBacklog;
        int emergencyVehiclesCleared;
    };

    static SimulationResults runFixedTimeSimulation(int totalTicks, double arrivalRate, unsigned int seed) {
        std::vector<LaneQueue> lanes = { NORTH, SOUTH, EAST, WEST };
        ArrivalSimulator arrivalEngine(seed);
        
        int currentPhaseIndex = 0;
        const int FIXED_GREEN = 12;
        int timer = FIXED_GREEN;
        int emergencyCleared = 0;
        int maxBacklog = 0;

        for (int tick = 0; tick < totalTicks; ++tick) {
            arrivalEngine.step(lanes, tick, arrivalRate);

            // Record maximum queue depth across all approaches
            for (const auto& l : lanes) {
                if (static_cast<int>(l.size()) > maxBacklog) {
                    maxBacklog = static_cast<int>(l.size());
                }
            }

            if (!lanes[currentPhaseIndex].empty()) {
                Vehicle v = lanes[currentPhaseIndex].dequeueVehicle(tick);
                if (v.isEmergency) emergencyCleared++;
            }

            timer--;
            if (timer <= 0) {
                currentPhaseIndex = (currentPhaseIndex + 1) % 4;
                timer = FIXED_GREEN;
            }
        }

        int totalCleared = 0;
        int totalWait = 0;
        for (const auto& l : lanes) {
            totalCleared += l.getTotalCleared();
            totalWait += l.getCumulativeWaitTime();
        }

        double avgWait = totalCleared > 0 ? static_cast<double>(totalWait) / totalCleared : 0.0;
        return { "Fixed-Time Controller (Static)", totalCleared, avgWait, maxBacklog, emergencyCleared };
    }

    static SimulationResults runAdaptiveSimulation(int totalTicks, double arrivalRate, unsigned int seed) {
        std::vector<LaneQueue> lanes = { NORTH, SOUTH, EAST, WEST };
        ArrivalSimulator arrivalEngine(seed);
        SignalOptimizer optimizer;
        IntersectionController controller;

        int emergencyCleared = 0;
        int maxBacklog = 0;

        for (int tick = 0; tick < totalTicks; ++tick) {
            arrivalEngine.step(lanes, tick, arrivalRate);

            for (const auto& l : lanes) {
                if (static_cast<int>(l.size()) > maxBacklog) {
                    maxBacklog = static_cast<int>(l.size());
                }
            }

            if (controller.isCycleComplete()) {
                Direction nextDir = optimizer.selectOptimalPhase(lanes, tick);
                int duration = optimizer.computeGreenDuration(lanes[nextDir]);
                controller.setPhase(nextDir, GREEN, duration, tick);
            }

            Direction active = controller.getActiveDirection();
            if (!lanes[active].empty()) {
                Vehicle v = lanes[active].dequeueVehicle(tick);
                controller.admitToCrossingZone(v);
                if (v.isEmergency) emergencyCleared++;
            }

            controller.updateCrossingZone();
            controller.decrementTimer();
        }

        int totalCleared = 0;
        int totalWait = 0;
        for (const auto& l : lanes) {
            totalCleared += l.getTotalCleared();
            totalWait += l.getCumulativeWaitTime();
        }

        double avgWait = totalCleared > 0 ? static_cast<double>(totalWait) / totalCleared : 0.0;
        return { "Dynamic Adaptive Controller (Ours)", totalCleared, avgWait, maxBacklog, emergencyCleared };
    }

    static void printComparisonReport(const SimulationResults& baseline, const SimulationResults& adaptive) {
        std::cout << "\n=========================================================================================\n";
        std::cout << "                 INTELLIGENT TRAFFIC OPTIMIZATION SYSTEM: BENCHMARK REPORT               \n";
        std::cout << "=========================================================================================\n";
        std::cout << std::left << std::setw(36) << "Performance Metric"
                  << std::setw(26) << "Fixed-Time Baseline"
                  << std::setw(26) << "Dynamic Optimizer" << "\n";
        std::cout << "-----------------------------------------------------------------------------------------\n";

        std::cout << std::left << std::setw(36) << "Total Vehicles Cleared"
                  << std::setw(26) << baseline.totalVehiclesCleared
                  << std::setw(26) << adaptive.totalVehiclesCleared << "\n";

        std::cout << std::left << std::setw(36) << "Average Wait Time (ticks)"
                  << std::setw(26) << std::fixed << std::setprecision(2) << baseline.averageWaitTicks
                  << std::setw(26) << std::fixed << std::setprecision(2) << adaptive.averageWaitTicks << "\n";

        std::cout << std::left << std::setw(36) << "Peak Approach Backlog (cars)"
                  << std::setw(26) << baseline.maxQueueBacklog
                  << std::setw(26) << adaptive.maxQueueBacklog << "\n";

        std::cout << std::left << std::setw(36) << "Emergency Vehicles Handled"
                  << std::setw(26) << baseline.emergencyVehiclesCleared
                  << std::setw(26) << adaptive.emergencyVehiclesCleared << "\n";

        double waitImprovement = ((baseline.averageWaitTicks - adaptive.averageWaitTicks) / baseline.averageWaitTicks) * 100.0;
        std::cout << "-----------------------------------------------------------------------------------------\n";
        std::cout << "CONCLUSION: Dynamic adaptive system reduced average wait latency by " 
                  << std::fixed << std::setprecision(1) << waitImprovement << "%.\n";
        std::cout << "=========================================================================================\n\n";
    }
};

#endif // TOPOLOGY_BENCHMARK_H
