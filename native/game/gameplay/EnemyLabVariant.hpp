#pragma once

#include <string_view>

namespace gameplay {

enum class EnemyLabVariant : unsigned char {
    RabidAnimator,
    EuphoriaLite,
    TraversalPredator,
    FeralHybrid,
    SupportDriven
};

enum class EnemyGaitAuthority : unsigned char { AuthoredAnimation, PhysicalContacts };
enum class EnemyLocomotionAuthority : unsigned char { KinematicRoot, PhysicalSupport };

struct EnemyMotorProfile {
    float speedScale;
    float turnCommitmentScale;
    float recklessness;
};

struct EnemyTraversalProfile {
    bool aggressiveRouting;
    bool mayStepUp;
    bool mayVault;
    bool mayMantle;
    bool mayClimb;
    bool mayJumpGap;
};

struct EnemyPresentationProfile {
    EnemyGaitAuthority ordinaryGait;
    float proceduralExpression;
    float physicalPoseAuthority;
};

struct EnemyConsequenceProfile {
    float impactResponse;
    float imbalanceResponse;
    float recoveryAggression;
};

struct EnemyLabProfile {
    EnemyLocomotionAuthority locomotionAuthority;
    EnemyMotorProfile motor;
    EnemyTraversalProfile traversal;
    EnemyPresentationProfile presentation;
    EnemyConsequenceProfile consequences;
};

constexpr EnemyLabProfile enemyLabProfile(EnemyLabVariant variant) {
    switch (variant) {
        case EnemyLabVariant::RabidAnimator:
            return {EnemyLocomotionAuthority::KinematicRoot,{1.10f,1.05f,0.38f},{false,true,true,true,true,false},
                    {EnemyGaitAuthority::AuthoredAnimation,0.72f,0.18f},{0.42f,0.30f,0.92f}};
        case EnemyLabVariant::EuphoriaLite:
            return {EnemyLocomotionAuthority::KinematicRoot,{1.00f,0.92f,0.58f},{false,true,true,true,true,false},
                    {EnemyGaitAuthority::PhysicalContacts,0.55f,0.72f},{0.90f,0.82f,0.64f}};
        case EnemyLabVariant::TraversalPredator:
            return {EnemyLocomotionAuthority::KinematicRoot,{1.22f,1.18f,0.62f},{true,true,true,true,true,true},
                    {EnemyGaitAuthority::AuthoredAnimation,0.82f,0.24f},{0.55f,0.46f,1.08f}};
        case EnemyLabVariant::FeralHybrid:
            return {EnemyLocomotionAuthority::KinematicRoot,{1.28f,1.12f,0.92f},{true,true,true,true,true,true},
                    {EnemyGaitAuthority::PhysicalContacts,1.0f,0.88f},{1.0f,1.0f,1.18f}};
        case EnemyLabVariant::SupportDriven:
            return {EnemyLocomotionAuthority::PhysicalSupport,{1.0f,1.0f,0.62f},{true,true,true,true,true,true},
                    {EnemyGaitAuthority::PhysicalContacts,0.72f,0.82f},{0.92f,0.90f,0.82f}};
    }
    return enemyLabProfile(EnemyLabVariant::RabidAnimator);
}

constexpr std::string_view enemyLabVariantName(EnemyLabVariant variant) {
    switch (variant) {
        case EnemyLabVariant::RabidAnimator: return "rabid-animator";
        case EnemyLabVariant::EuphoriaLite: return "euphoria-lite";
        case EnemyLabVariant::TraversalPredator: return "traversal-predator";
        case EnemyLabVariant::FeralHybrid: return "feral-hybrid";
        case EnemyLabVariant::SupportDriven: return "support-driven";
    }
    return "rabid-animator";
}

inline bool parseEnemyLabVariant(std::string_view text, EnemyLabVariant& variant) {
    if (text=="rabid-animator" || text=="rabid") variant=EnemyLabVariant::RabidAnimator;
    else if (text=="euphoria-lite" || text=="euphoria") variant=EnemyLabVariant::EuphoriaLite;
    else if (text=="traversal-predator" || text=="predator") variant=EnemyLabVariant::TraversalPredator;
    else if (text=="feral-hybrid" || text=="feral") variant=EnemyLabVariant::FeralHybrid;
    else if (text=="support-driven" || text=="physical-support") variant=EnemyLabVariant::SupportDriven;
    else return false;
    return true;
}

} // namespace gameplay
