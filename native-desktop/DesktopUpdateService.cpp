#include "DesktopUpdateService.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#elif defined(__APPLE__)
#include <ixwebsocket/IXHttpClient.h>
#endif

#include <algorithm>
#include <cstdio>
#include <memory>

namespace {
constexpr const char* ManifestUrl =
    "https://github.com/indrolend/Digital-breakdown-dev/releases/download/latest-native/build-manifest.json";

#ifdef _WIN32
std::wstring wide(const std::string& value){if(value.empty())return{};const int n=MultiByteToWideChar(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),nullptr,0);std::wstring out(static_cast<std::size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),out.data(),n);return out;}
struct UrlParts{bool secure=false;std::wstring host,path;INTERNET_PORT port=0;};
bool crack(const std::string& value,UrlParts& out){const std::wstring w=wide(value);URL_COMPONENTS c{};c.dwStructSize=sizeof(c);c.dwSchemeLength=c.dwHostNameLength=c.dwUrlPathLength=c.dwExtraInfoLength=static_cast<DWORD>(-1);if(!WinHttpCrackUrl(w.c_str(),0,0,&c))return false;out.secure=c.nScheme==INTERNET_SCHEME_HTTPS;out.host.assign(c.lpszHostName,c.dwHostNameLength);out.path.assign(c.lpszUrlPath,c.dwUrlPathLength);if(c.lpszExtraInfo&&c.dwExtraInfoLength)out.path.append(c.lpszExtraInfo,c.dwExtraInfoLength);out.port=c.nPort;return true;}
std::string readResponse(HINTERNET request){std::string response;DWORD available=0;while(WinHttpQueryDataAvailable(request,&available)&&available){const std::size_t old=response.size();response.resize(old+available);DWORD read=0;if(!WinHttpReadData(request,response.data()+old,available,&read)){response.resize(old);break;}response.resize(old+read);}return response;}
#endif

std::string jsonString(const std::string& json,const char* key){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return{};at=json.find(':',at+needle.size());if(at==std::string::npos)return{};at=json.find('"',at+1);if(at==std::string::npos)return{};const std::size_t end=json.find('"',at+1);return end==std::string::npos?std::string{}:json.substr(at+1,end-at-1);}
int jsonInt(const std::string& json,const char* key,int fallback=-1){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return fallback;at=json.find(':',at+needle.size());if(at==std::string::npos)return fallback;try{return std::stoi(json.substr(at+1));}catch(...){return fallback;}}
bool containsPlatformArtifact(const std::string& json,const std::string& platform){return json.find("\"platform\":\""+platform+"\"")!=std::string::npos||json.find("\"platform\": \""+platform+"\"")!=std::string::npos;}

bool fetchManifest(std::string& body) {
#ifdef _WIN32
    UrlParts url;
    if(!crack(ManifestUrl,url))return false;
    HINTERNET session=WinHttpOpen(L"DigitalBreakdownUpdater/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)return false;
    WinHttpSetTimeouts(session,5000,5000,5000,15000);
    HINTERNET connection=WinHttpConnect(session,url.host.c_str(),url.port,0);
    HINTERNET request=connection?WinHttpOpenRequest(connection,L"GET",url.path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,url.secure?WINHTTP_FLAG_SECURE:0):nullptr;
    BOOL ok=request&&WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(request,nullptr);
    DWORD status=0,statusSize=sizeof(status);if(ok)WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX);
    if(ok&&status>=200&&status<300)body=readResponse(request);
    if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
    return ok&&status>=200&&status<300&&!body.empty();
#elif defined(__APPLE__)
    ix::HttpClient client;
    const auto response=client.get(ManifestUrl,std::make_shared<ix::HttpRequestArgs>());
    if(!response||response->statusCode<200||response->statusCode>=300)return false;
    body=response->body;
    return !body.empty();
#else
    return false;
#endif
}
}

DesktopUpdateService::~DesktopUpdateService(){disconnect();}

void DesktopUpdateService::disconnect(){if(worker_.joinable())worker_.join();}

void DesktopUpdateService::checkForUpdates(const BuildIdentity& current){
    if(state_.load()==State::Checking)return;
    disconnect();
    set(State::Checking,"Checking for updates");
    worker_=std::thread(&DesktopUpdateService::workerMain,this,current);
}

std::string DesktopUpdateService::status() const{std::lock_guard<std::mutex> lock(mutex_);return status_;}

void DesktopUpdateService::set(State state,const std::string& status){state_=state;{std::lock_guard<std::mutex> lock(mutex_);status_=status;}std::printf("UPDATE_%s %s\n",stateLabel(state).c_str(),status.c_str());std::fflush(stdout);}

std::string DesktopUpdateService::stateLabel(State state){
    switch(state){
    case State::Idle:return "IDLE";
    case State::Checking:return "CHECKING";
    case State::Current:return "CURRENT";
    case State::Available:return "AVAILABLE";
    case State::Incompatible:return "INCOMPATIBLE";
    case State::Failed:return "FAILED";
    }
    return "UNKNOWN";
}

void DesktopUpdateService::workerMain(BuildIdentity current){
    std::string manifest;
    if(!fetchManifest(manifest)){set(State::Failed,"Update check failed");return;}
    if(jsonInt(manifest,"schemaVersion",-1)>3){set(State::Failed,"Unsupported update manifest");return;}
    const int protocol=jsonInt(manifest,"protocolVersion",-1);
    const int gameplay=jsonInt(manifest,"gameplayVersion",-1);
    if(protocol!=current.protocolVersion||gameplay!=current.gameplayVersion){set(State::Incompatible,"Latest build is incompatible");return;}
    if(!containsPlatformArtifact(manifest,current.platform)){set(State::Incompatible,"No package for this platform");return;}
    const std::string commit=jsonString(manifest,"commit");
    const std::string shortCommit=jsonString(manifest,"shortCommit");
    if(commit.empty()){set(State::Failed,"Update manifest missing commit");return;}
    if(commit==current.commit){set(State::Current,"Already current "+current.commitShort);return;}
    set(State::Available,"Update available "+(shortCommit.empty()?commit.substr(0,std::min<std::size_t>(7,commit.size())):shortCommit));
}
