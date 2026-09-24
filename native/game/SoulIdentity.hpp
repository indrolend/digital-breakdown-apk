#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace soul_identity {

enum class Gender : unsigned char { Woman, Man, Nonbinary };

struct Profile {
    const char* givenName="UNKNOWN";
    const char* familyName="";
    const char* occupation="UNRECORDED";
    Gender gender=Gender::Nonbinary;
    int age=0;
};

// A restrained, deterministic material signature carried by the same identity
// through loose soul, projectile, recovery, and goal deposit. This is derived
// presentation data: it introduces no lifecycle state or gameplay authority.
struct VisualSignature {
    float red=0.76f;
    float green=0.96f;
    float blue=0.72f;
    float phase=0.0f;
    float spinRate=1.0f;
    float aspect=1.0f;
};

inline std::uint64_t mix(std::uint64_t value){
    value^=value>>30;value*=0xbf58476d1ce4e5b9ULL;
    value^=value>>27;value*=0x94d049bb133111ebULL;
    return value^(value>>31);
}

inline Profile profile(std::uint64_t soulId,bool brute,int originRoom){
    static constexpr std::array<const char*,24> Given{{
        "MARA","DEVON","INES","CALEB","NIA","OMAR","JUNE","ELI",
        "PRIYA","MATEO","ROBIN","ADA","NOAH","MINA","LEO","TESS",
        "SAM","YARA","BEN","LINA","ARI","MAE","IVAN","ZOE"
    }};
    static constexpr std::array<const char*,24> Family{{
        "ORTIZ","KIM","PATEL","REED","NGUYEN","BROOKS","SATO","BELL",
        "SINGH","FLORES","PARK","WARD","ALI","PRICE","ROSS","SHAW",
        "DIAZ","BAKER","CHEN","GRAY","KHAN","MOSS","RIVERA","STONE"
    }};
    static constexpr std::array<const char*,24> Occupation{{
        "BUS DRIVER","DENTAL ASSISTANT","NIGHT AUDITOR","SUBSTITUTE TEACHER",
        "BAKER","WAREHOUSE PICKER","LIBRARY AIDE","HOME HEALTH WORKER",
        "LINE COOK","FLORIST","MAIL CARRIER","PHARMACY TECH",
        "JANITOR","DAYCARE WORKER","MECHANIC","CASHIER",
        "BARBER","SECURITY GUARD","BOOKKEEPER","PARAMEDIC",
        "DOG GROOMER","LAB TECH","PAINTER","RECEPTIONIST"
    }};
    if(soulId==0)return {};
    const std::uint64_t seed=mix(soulId^(static_cast<std::uint64_t>(static_cast<std::uint32_t>(originRoom))*0x9e3779b97f4a7c15ULL)^(brute?0xd1b54a32d192ed03ULL:0ULL));
    Profile result;
    result.givenName=Given[seed%Given.size()];
    result.familyName=Family[(seed>>8)%Family.size()];
    result.occupation=Occupation[(seed>>16)%Occupation.size()];
    result.gender=static_cast<Gender>((seed>>24)%3ULL);
    result.age=18+static_cast<int>((seed>>32)%70ULL);
    return result;
}

inline VisualSignature visualSignature(std::uint64_t soulId,bool brute,int originRoom){
    if(soulId==0)return {};
    const std::uint64_t seed=mix(soulId^(static_cast<std::uint64_t>(static_cast<std::uint32_t>(originRoom))*0x9e3779b97f4a7c15ULL)^(brute?0xd1b54a32d192ed03ULL:0ULL));
    const float a=static_cast<float>((seed>>8)&255ULL)/255.0f;
    const float b=static_cast<float>((seed>>24)&255ULL)/255.0f;
    VisualSignature result;
    // Keep every soul inside one recognizable family while preserving enough
    // variation to follow a particular person through each physical form.
    result.red=0.62f+0.22f*a;
    result.green=0.86f+0.12f*b;
    result.blue=0.66f+0.24f*(1.0f-a);
    result.phase=static_cast<float>((seed>>40)&1023ULL)/1023.0f;
    result.spinRate=0.86f+0.34f*static_cast<float>((seed>>50)&255ULL)/255.0f;
    result.aspect=0.92f+0.16f*static_cast<float>((seed>>58)&63ULL)/63.0f;
    return result;
}

inline const char* genderName(Gender gender){
    switch(gender){case Gender::Woman:return "WOMAN";case Gender::Man:return "MAN";case Gender::Nonbinary:return "NONBINARY";}
    return "PERSON";
}

inline std::string name(const Profile& profile){return std::string(profile.givenName)+" "+profile.familyName;}
inline std::string details(const Profile& profile){return std::to_string(profile.age)+" / "+genderName(profile.gender)+" / "+profile.occupation;}

} // namespace soul_identity
