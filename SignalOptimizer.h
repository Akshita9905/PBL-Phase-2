Member 2: Vaani Singh (Priority Queue, Density Weights, Preemption)
#ifndef SIGNAL_OPTIMIZER_H
#define SIGNAL_OPTIMIZER_H

#include "VehicleInflow.h"
#include <queue>
#include <vector>
#include <algorithm>

struct ApproachScore {
    Direction direction;
    double score;
    int queueDepth;
    bool emergencyFlag;

    ApproachScore(Direction dir, double s, int depth, bool emg)
        : direction(dir), score(s), queueDepth(depth), emergencyFlag(emg) {}
};

struct CompareApproachScore {
    bool operator()(const ApproachScore& a, const ApproachScore& b) const {
        // Strict emergency vehicle preemption
        if (a.emergencyFlag != b.emergencyFlag) {
            return b.emergencyFlag; // Higher priority if emergency is true
        }
        // Max-heap prioritizes highest computed density/wait score
        return a.score < b.score;
    }
};

class SignalOptimizer {
private:
    const int MIN_GREEN_TICKS = 6;
    const int MAX_GREEN_TICKS = 24;

public:
    double calculateDensityScore(const LaneQueue& lane, int currentTick) {
        if (lane.empty()) return 0.0;
        int queueLength = static_cast<int>(lane.size());
        int accumulatedWait = lane.computeAccumulatedWait(currentTick);
        
        // Dynamic scoring formula: Weighted sum of queue depth and accumulated latency
        double score = (queueLength * 1.8) + (accumulatedWait * 0.4);
        return score;
    }

    Direction selectOptimalPhase(const std::vector<LaneQueue>& lanes, int currentTick) {
        std::priority_queue<ApproachScore, std::vector<ApproachScore>, CompareApproachScore> maxHeap;

        for (int i = 0; i < 4; ++i) {
            double score = calculateDensityScore(lanes[i], currentTick);
            bool hasEmg = lanes[i].hasEmergencyVehicle();
            maxHeap.push(ApproachScore(static_cast<Direction>(i), score, static_cast<int>(lanes[i].size()), hasEmg));
        }

        if (maxHeap.empty()) {
            return NORTH;
        }

        return maxHeap.top().direction;
    }

    int computeGreenDuration(const LaneQueue& activeLane) {
        int length = static_cast<int>(activeLane.size());
        
        // Adaptive split proportional to backlog
        int allocated = MIN_GREEN_TICKS + (length * 2);
        return std::clamp(allocated, MIN_GREEN_TICKS, MAX_GREEN_TICKS);
    }
};

#endif // SIGNAL_OPTIMIZER_H
