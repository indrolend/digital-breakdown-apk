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

inline const char* genderName(Gender gender){
    switch(gender){case Gender::Woman:return "WOMAN";case Gender::Man:return "MAN";case Gender::Nonbinary:return "NONBINARY";}
    return "PERSON";
}

inline std::string name(const Profile& profile){return std::string(profile.givenName)+" "+profile.familyName;}
inline std::string details(const Profile& profile){return std::to_string(profile.age)+" / "+genderName(profile.gender)+" / "+profile.occupation;}

} // namespace soul_identity
