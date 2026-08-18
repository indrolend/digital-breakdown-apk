#include "DesktopMultiplayer.hpp"
#include "BoundedEventQueue.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#elif defined(__APPLE__)
#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <memory>

namespace {
#ifdef _WIN32
std::wstring wide(const std::string& value){if(value.empty())return{};const int n=MultiByteToWideChar(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),nullptr,0);std::wstring out(static_cast<std::size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),out.data(),n);return out;}
struct UrlParts{bool secure=false;std::wstring host,path;INTERNET_PORT port=0;};
bool crack(const std::string& value,UrlParts& out){const std::wstring w=wide(value);URL_COMPONENTS c{};c.dwStructSize=sizeof(c);c.dwSchemeLength=c.dwHostNameLength=c.dwUrlPathLength=c.dwExtraInfoLength=static_cast<DWORD>(-1);if(!WinHttpCrackUrl(w.c_str(),0,0,&c))return false;out.secure=c.nScheme==INTERNET_SCHEME_HTTPS;out.host.assign(c.lpszHostName,c.dwHostNameLength);out.path.assign(c.lpszUrlPath,c.dwUrlPathLength);if(c.lpszExtraInfo&&c.dwExtraInfoLength)out.path.append(c.lpszExtraInfo,c.dwExtraInfoLength);out.port=c.nPort;return true;}
#endif
const char* roleName(DesktopMultiplayer::Role role){return role==DesktopMultiplayer::Role::Host?"host":role==DesktopMultiplayer::Role::Guest?"guest":"offline";}
std::int64_t steadyMilliseconds(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
const char* eventMarker(dbnet::GameplayEventType type){
  using Type=dbnet::GameplayEventType;
  switch(type){
    case Type::PlayerActionStarted:return "MULTIPLAYER_ACTION_STARTED";
    case Type::PlayerActionContact:return "MULTIPLAYER_ACTION_CONTACT";
    case Type::EnemyHitConfirmed:return "MULTIPLAYER_HIT_CONFIRMED";
    case Type::EnemyShellBroken:return "MULTIPLAYER_SHELL_BROKEN";
    case Type::SoulEmergenceStarted:return "MULTIPLAYER_SOUL_EMERGED";
    case Type::VacuumStarted:return "MULTIPLAYER_VACUUM_CONFIRMED";
    case Type::SoulAttractionStarted:return "MULTIPLAYER_SOUL_ATTRACTED";
    case Type::SoulLatched:return "MULTIPLAYER_SOUL_LATCHED";
    case Type::SoulIngestionStarted:return "MULTIPLAYER_SOUL_INGESTING";
    case Type::SoulCaptureCompleted:return "MULTIPLAYER_SOUL_STORED";
    case Type::DischargeStarted:return "MULTIPLAYER_DISCHARGE_CONFIRMED";
    case Type::ProjectileSpawned:return "MULTIPLAYER_PROJECTILE_SPAWNED";
    case Type::ProjectileImpacted:return "MULTIPLAYER_PROJECTILE_IMPACTED";
    case Type::ProjectileDespawned:return "MULTIPLAYER_PROJECTILE_DESPAWNED";
    case Type::PlayerDowned:return "MULTIPLAYER_PLAYER_DOWNED";
    case Type::PlayerRevived:return "MULTIPLAYER_PLAYER_REVIVED";
    case Type::PlayerDied:return "MULTIPLAYER_PLAYER_DIED";
  }
  return "MULTIPLAYER_EVENT";
}
#ifdef _WIN32
std::string readResponse(HINTERNET request){std::string response;DWORD available=0;while(WinHttpQueryDataAvailable(request,&available)&&available){const std::size_t old=response.size();response.resize(old+available);DWORD read=0;if(!WinHttpReadData(request,response.data()+old,available,&read)){response.resize(old);break;}response.resize(old+read);}return response;}
#endif
}

DesktopMultiplayer::~DesktopMultiplayer(){disconnect();}
void DesktopMultiplayer::enqueueIncoming(Incoming&& item){std::lock_guard<std::mutex> lock(queueMutex_);if(dbnet::pushBoundedIncoming(incoming_,std::move(item))){std::printf("MULTIPLAYER_QUEUE_DROP limit=%zu\n",dbnet::MAX_INCOMING_EVENTS);std::fflush(stdout);}}
void DesktopMultiplayer::applyPresentation(GameState& renderState) const{
  if(role_.load()==Role::Guest&&snapshotInterpolator_.ready())
    snapshotInterpolator_.apply(renderState,static_cast<std::uint8_t>(playerId_.load()),steadyMilliseconds());
}
void DesktopMultiplayer::configureImpairment(int latencyMs,int jitterMs,
    int dropSnapshotEvery,int dropInputEvery,std::uint32_t seed){
  netLatencyMs_=std::max(0,latencyMs);netJitterMs_=std::max(0,jitterMs);
  dropSnapshotEvery_=std::max(0,dropSnapshotEvery);
  dropInputEvery_=std::max(0,dropInputEvery);impairmentSeed_=seed?seed:1;
  snapshotSendCount_=inputSendCount_=0;
  if(netLatencyMs_||netJitterMs_||dropSnapshotEvery_||dropInputEvery_){
    std::printf("MULTIPLAYER_NET_IMPAIRMENT latency_ms=%d jitter_ms=%d drop_snapshot_every=%d drop_input_every=%d seed=%u\n",
      netLatencyMs_,netJitterMs_,dropSnapshotEvery_,dropInputEvery_,impairmentSeed_);
    std::fflush(stdout);
  }
}
std::string DesktopMultiplayer::roomCode()const{std::lock_guard<std::mutex> lock(stateMutex_);return roomCode_;}
std::string DesktopMultiplayer::status()const{std::lock_guard<std::mutex> lock(stateMutex_);return status_;}
void DesktopMultiplayer::printMetrics()const{
  std::printf("MULTIPLAYER_METRICS snapshots_received=%llu stale_snapshots_rejected=%llu events_received=%llu duplicate_events_rejected=%llu stale_events_rejected=%llu predicted_actions=%llu confirmed_actions=%llu corrected_actions=%llu cancelled_actions=%llu hash_matches=%llu hash_mismatches=%llu maximum_position_correction=%.4f maximum_action_phase_correction=%.4f\n",
    static_cast<unsigned long long>(metrics_.snapshotsReceived),static_cast<unsigned long long>(metrics_.staleSnapshotsRejected),
    static_cast<unsigned long long>(metrics_.eventsReceived),static_cast<unsigned long long>(metrics_.duplicateEventsRejected),
    static_cast<unsigned long long>(metrics_.staleEventsRejected),static_cast<unsigned long long>(metrics_.predictedActions),
    static_cast<unsigned long long>(metrics_.confirmedActions),static_cast<unsigned long long>(metrics_.correctedActions),
    static_cast<unsigned long long>(metrics_.cancelledActions),static_cast<unsigned long long>(metrics_.hashMatches),
    static_cast<unsigned long long>(metrics_.hashMismatches),metrics_.maximumPositionCorrection,metrics_.maximumActionPhaseCorrection);
  std::fflush(stdout);
}
void DesktopMultiplayer::setStatus(const std::string& value){std::lock_guard<std::mutex> lock(stateMutex_);status_=value;}
void DesktopMultiplayer::host(const std::string& serviceUrl){begin(Role::Host,serviceUrl,{});}
void DesktopMultiplayer::join(const std::string& serviceUrl,const std::string& roomCode){const std::string code=dbmultiplayer::normalizeRoomCode(roomCode);if(code.empty()){role_=Role::Guest;phase_=dbmultiplayer::Phase::Failed;setStatus("ROOM NOT FOUND");std::printf("MULTIPLAYER_REJECT stage=room_code reason=invalid\n");std::fflush(stdout);return;}begin(Role::Guest,serviceUrl,code);}
void DesktopMultiplayer::begin(Role role,const std::string& service,const std::string& code){if(pending()||connected()){std::printf("MULTIPLAYER_DUPLICATE_IGNORED role=%s\n",roleName(role));std::fflush(stdout);return;}disconnect();role_=role;phase_=dbmultiplayer::transition(phase_.load(),role==Role::Host?dbmultiplayer::Event::CreateRoom:dbmultiplayer::Event::JoinRoom);serviceUrl_=service;{std::lock_guard<std::mutex> lock(stateMutex_);roomCode_=code;hostKey_.clear();status_=role==Role::Host?"CREATING ROOM":"JOINING "+code;}std::printf("MULTIPLAYER_BEGIN role=%s service=%s room=%s\n",roleName(role),service.c_str(),code.c_str());std::fflush(stdout);stop_=false;configuredGame_=false;sessionEndReported_=false;loggedInput_=loggedSnapshot_=false;playerCount_=0;startId_=0;worldContext_={};outgoingSequence_=0;localInputTick_=0;lastInputSendMs_=0;lastSnapshotTick_=0;lastSnapshotSequence_=0;lastSnapshotReceiveMs_=0;lastPredictedButtons_=0;metrics_={};lastInputSequence_.fill(0);snapshotInterpolator_.reset();const auto now=steadyMilliseconds();lastValidMessageMs_=lastHeartbeatMs_=phaseStartedMs_=now;worker_=std::thread(&DesktopMultiplayer::workerMain,this);}

void DesktopMultiplayer::setWorldContext(const dbnet::NetworkWorldContext& world,const char* reason){
  worldContext_=world;lastSnapshotSequence_=0;lastInputSequence_.fill(0);snapshotInterpolator_.reset();
  eventTracker_.reset(world);hasPreviousEventWorld_=false;nextEventId_=0;
  projectileSources_.fill(0);
  lastDischargeSource_=0;
  std::printf("MULTIPLAYER_WORLD_CONTEXT_CHANGED reason=%s session=%u run=%u room_generation=%u room=%u\n",reason,world.sessionId,world.runGeneration,world.roomGeneration,world.roomIndex);
  std::printf("MULTIPLAYER_ROOM_GENERATION_RESET generation=%u room=%u\n",world.roomGeneration,world.roomIndex);std::fflush(stdout);
}

bool DesktopMultiplayer::acceptWorldContext(const dbnet::NetworkWorldContext& packet,const dbnet::PacketHeader& header,bool allowNewerRoom){
  const auto compatibility=dbnet::compareWorldContext(packet,worldContext_);
  if(compatibility==dbnet::WorldContextCompatibility::Compatible)return true;
  if(allowNewerRoom&&(compatibility==dbnet::WorldContextCompatibility::NewerRoom||
      compatibility==dbnet::WorldContextCompatibility::NewerRun)){
    setWorldContext(packet,compatibility==dbnet::WorldContextCompatibility::NewerRun
      ?"authoritative_run_restart":"authoritative_room_rollover");
    return true;
  }
  const char* reason=compatibility==dbnet::WorldContextCompatibility::Older
    ?"older_world":compatibility==dbnet::WorldContextCompatibility::NewerRoom
    ?"unexpected_newer_room":compatibility==dbnet::WorldContextCompatibility::NewerRun
    ?"unexpected_newer_run":"incompatible_session_or_room";
  if(header.type==dbnet::MessageType::Snapshot)++metrics_.staleSnapshotsRejected;
  if(header.type==dbnet::MessageType::Event)++metrics_.staleEventsRejected;
  std::printf("MULTIPLAYER_STALE_PACKET_REJECTED type=%u sequence=%u packet_session=%u packet_run=%u packet_room_generation=%u packet_room=%u current_session=%u current_run=%u current_room_generation=%u current_room=%u reason=%s\n",static_cast<unsigned>(header.type),header.sequence,packet.sessionId,packet.runGeneration,packet.roomGeneration,packet.roomIndex,worldContext_.sessionId,worldContext_.runGeneration,worldContext_.roomGeneration,worldContext_.roomIndex,reason);std::fflush(stdout);return false;
}

void DesktopMultiplayer::emitCombatEvents(const dbnet::WorldSnapshot& world){
  if(!hasPreviousEventWorld_){previousEventWorld_=world;hasPreviousEventWorld_=true;return;}
  dbnet::GameplayEventDerivationState derivation;
  derivation.nextEventId=nextEventId_;
  derivation.lastDischargeSource=lastDischargeSource_;
  derivation.projectileSources=projectileSources_;
  for(const auto& event:dbnet::deriveGameplayEvents(
          previousEventWorld_,world,derivation)){
    sendBinary(dbnet::encodeEvent(0,event));
    std::printf("%s event=%u source=%u target=%u tick=%u flags=%u\n",
      eventMarker(event.type),event.eventId,event.sourceEntityId,
      event.targetEntityId,event.authoritativeTick,event.flags);
    std::fflush(stdout);
  }
  nextEventId_=derivation.nextEventId;
  lastDischargeSource_=derivation.lastDischargeSource;
  projectileSources_=derivation.projectileSources;
  previousEventWorld_=world;
}
void DesktopMultiplayer::disconnect(){stop_=true;void* session=nullptr;void* connection=nullptr;void* request=nullptr;void* socket=nullptr;{std::lock_guard<std::mutex> sendLock(sendMutex_);std::lock_guard<std::mutex> handleLock(handleMutex_);session=session_;connection=connection_;request=request_;socket=webSocket_;session_=connection_=request_=webSocket_=nullptr;}
#ifdef _WIN32
if(socket)WinHttpCloseHandle(static_cast<HINTERNET>(socket));if(request)WinHttpCloseHandle(static_cast<HINTERNET>(request));if(connection)WinHttpCloseHandle(static_cast<HINTERNET>(connection));if(session)WinHttpCloseHandle(static_cast<HINTERNET>(session));
#elif defined(__APPLE__)
if(socket)static_cast<ix::WebSocket*>(socket)->stop();
#endif
if(worker_.joinable()&&worker_.get_id()!=std::this_thread::get_id())worker_.join();{std::lock_guard<std::mutex> lock(queueMutex_);incoming_.clear();}connected_=false;role_=Role::Offline;phase_=dbmultiplayer::Phase::Offline;configuredGame_=false;snapshotInterpolator_.reset();}

void DesktopMultiplayer::fail(const std::string& visibleStatus,const char* stage,unsigned long error){connected_=false;phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::Failure);setStatus(visibleStatus);std::printf("MULTIPLAYER_FAILED stage=%s status=%s error=%lu\n",stage,visibleStatus.c_str(),error);std::fflush(stdout);}
void DesktopMultiplayer::endGameplaySession(Game& game,const char* reason,const char* visibleStatus){
  connected_=false;configuredGame_=false;setStatus(visibleStatus);
  snapshotInterpolator_.reset();eventTracker_.reset({});lastInputSequence_.fill(0);
  game.prepareStartScreen();
  if(!sessionEndReported_){
    sessionEndReported_=true;
    std::printf("MULTIPLAYER_SESSION_END role=%s reason=%s\n",
      roleName(role_.load()),reason);std::fflush(stdout);
  }
}
void DesktopMultiplayer::publishHandles(void* session,void* connection,void* request,void* socket){std::lock_guard<std::mutex> lock(handleMutex_);session_=session;connection_=connection;request_=request;if(socket)webSocket_=socket;}
bool DesktopMultiplayer::releaseHandles(void* session,void* connection,void* request,void* socket){std::lock_guard<std::mutex> lock(handleMutex_);const bool owns=session&&session_==session;if(session_==session)session_=nullptr;if(connection_==connection)connection_=nullptr;if(request_==request)request_=nullptr;if(webSocket_==socket)webSocket_=nullptr;return owns;}

std::string DesktopMultiplayer::jsonString(const std::string& json,const char* key){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return{};at=json.find(':',at+needle.size());if(at==std::string::npos)return{};at=json.find('"',at+1);if(at==std::string::npos)return{};const std::size_t end=json.find('"',at+1);return end==std::string::npos?std::string{}:json.substr(at+1,end-at-1);}
int DesktopMultiplayer::jsonInt(const std::string& json,const char* key,int fallback){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return fallback;at=json.find(':',at+needle.size());if(at==std::string::npos)return fallback;try{return std::stoi(json.substr(at+1));}catch(...){return fallback;}}

bool DesktopMultiplayer::checkServiceCompatibility() {
  setStatus("CHECKING COMPATIBILITY");
#ifdef _WIN32
  UrlParts url;
  if (!crack(serviceUrl_ + "/health", url)) {
    fail("CONNECTION FAILED","health_url",GetLastError());
    return false;
  }
  HINTERNET session =
      WinHttpOpen(L"DigitalBreakdown/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    fail("CONNECTION FAILED","health_session",GetLastError());
    return false;
  }
  WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);
  HINTERNET connection = WinHttpConnect(session, url.host.c_str(), url.port, 0);
  HINTERNET request =
      connection
          ? WinHttpOpenRequest(connection, L"GET", url.path.c_str(), nullptr,
                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                               url.secure ? WINHTTP_FLAG_SECURE : 0)
          : nullptr;
  BOOL ok = request &&
            WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr);
  DWORD status=0,statusSize=sizeof(status);if(ok)WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX);
  const DWORD error=ok?0:GetLastError();
  const std::string response=ok?readResponse(request):std::string{};
  if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
  std::printf("MULTIPLAYER_HEALTH status=%lu error=%lu\n",static_cast<unsigned long>(status),static_cast<unsigned long>(error));std::fflush(stdout);
  if(!ok||status<200||status>=300){fail("CONNECTION FAILED","health",error);return false;}
  const int protocol=jsonInt(response,"protocolVersion",jsonInt(response,"protocol",-1));
  if(protocol!=dbnet::PROTOCOL_VERSION){std::printf("MULTIPLAYER_HEALTH_REJECT protocol=%d expected=%u\n",protocol,dbnet::PROTOCOL_VERSION);std::fflush(stdout);fail("VERSION MISMATCH","health_version");return false;}
  return true;
#elif defined(__APPLE__)
  ix::HttpClient client;
  const auto response=client.get(serviceUrl_+"/health",std::make_shared<ix::HttpRequestArgs>());
  if(!response||response->statusCode<200||response->statusCode>=300){fail("CONNECTION FAILED","health");return false;}
  const int protocol=jsonInt(response->body,"protocolVersion",jsonInt(response->body,"protocol",-1));
  std::printf("MULTIPLAYER_HEALTH status=%d protocol=%d\n",response->statusCode,protocol);std::fflush(stdout);
  if(protocol!=dbnet::PROTOCOL_VERSION){fail("VERSION MISMATCH","health_version");return false;}
  return true;
#else
  fail("CONNECTION FAILED","health_unsupported");
  return false;
#endif
}

bool DesktopMultiplayer::acceptWelcome(const std::string& message){
  if(jsonString(message,"type")!="welcome")return false;
  const int protocol=jsonInt(message,"protocol"),gameplay=jsonInt(message,"gameplayVersion");
  const int assigned=jsonInt(message,"playerId",-1);
  const std::string welcomeRoom=jsonString(message,"room"),welcomeRole=jsonString(message,"role");
  if(protocol!=dbnet::PROTOCOL_VERSION||gameplay!=dbnet::GAMEPLAY_VERSION){std::printf("MULTIPLAYER_WELCOME_REJECT reason=version protocol=%d gameplay=%d\n",protocol,gameplay);std::fflush(stdout);fail("VERSION MISMATCH","welcome_version");stop_=true;return false;}
  const bool expectedPlayer=role_==Role::Host?assigned==0:assigned>0;
  if(welcomeRoom!=roomCode()||welcomeRole!=roleName(role_.load())||!expectedPlayer){std::printf("MULTIPLAYER_WELCOME_REJECT reason=identity role=%s player=%d room_match=%d\n",welcomeRole.c_str(),assigned,welcomeRoom==roomCode()?1:0);std::fflush(stdout);fail("CONNECTION FAILED","welcome_identity");stop_=true;return false;}
  if(phase_.load()!=dbmultiplayer::Phase::Connecting)return false;
  playerId_=assigned;phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::Welcome);connected_=true;
  playerCount_=jsonInt(message,"playerCount",role_==Role::Host?1:2);
  setStatus(role_==Role::Host?"WAITING FOR PLAYER 1/2":"CONNECTED - WAITING FOR HOST");
  std::printf("MULTIPLAYER_WELCOME_RECEIVED protocol=%d gameplay=%d\n",protocol,gameplay);
  std::printf("MULTIPLAYER_CONNECTED role=%s player=%d room=%s\n",roleName(role_.load()),assigned,roomCode().c_str());std::fflush(stdout);
  return true;
}

bool DesktopMultiplayer::createRoom() {
#ifdef _WIN32
  UrlParts url;
  if (!crack(serviceUrl_ + "/v1/rooms", url))
    return false;
  HINTERNET session =
      WinHttpOpen(L"DigitalBreakdown/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    return false;
  WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);
  HINTERNET connection = WinHttpConnect(session, url.host.c_str(), url.port, 0);
  HINTERNET request =
      connection
          ? WinHttpOpenRequest(connection, L"POST", url.path.c_str(), nullptr,
                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                               url.secure ? WINHTTP_FLAG_SECURE : 0)
          : nullptr;
  publishHandles(session,connection,request,nullptr);
  const std::string body = "{\"gameplayVersion\":" + std::to_string(dbnet::GAMEPLAY_VERSION) + "}";
  BOOL ok = request &&
            WinHttpSendRequest(request, L"Content-Type: application/json\r\n",
                               static_cast<DWORD>(-1), const_cast<char *>(body.data()),
                               static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) &&
            WinHttpReceiveResponse(request, nullptr);
  DWORD status=0,statusSize=sizeof(status);if(ok)WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX);
  const DWORD error=ok?0:GetLastError();
  const std::string response=ok?readResponse(request):std::string{};
  const bool owns=releaseHandles(session,connection,request,nullptr);
  if(owns){if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);}
  std::printf("MULTIPLAYER_HTTP stage=create status=%lu error=%lu\n",static_cast<unsigned long>(status),static_cast<unsigned long>(error));std::fflush(stdout);
  if (!ok||status<200||status>=300)
    return false;
  const std::string code = jsonString(response, "code"),
                    key = jsonString(response, "hostKey");
  if (code.size() != 6 || key.empty()||jsonInt(response,"protocol")!=dbnet::PROTOCOL_VERSION||jsonInt(response,"gameplayVersion")!=dbnet::GAMEPLAY_VERSION){
    if(jsonInt(response,"protocol")!=dbnet::PROTOCOL_VERSION)fail("VERSION MISMATCH","create_version");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    roomCode_ = code;
    hostKey_ = key;
  }
  std::printf("MULTIPLAYER_ROOM_CODE %s\n", code.c_str());
  std::fflush(stdout);
  return true;
#elif defined(__APPLE__)
  ix::HttpClient client;
  auto args=std::make_shared<ix::HttpRequestArgs>();
  args->extraHeaders["Content-Type"]="application/json";
  const auto response=client.post(serviceUrl_+"/v1/rooms","{\"gameplayVersion\":"+std::to_string(dbnet::GAMEPLAY_VERSION)+"}",args);
  if(!response||response->statusCode<200||response->statusCode>=300)return false;
  const std::string code=jsonString(response->body,"code"),key=jsonString(response->body,"hostKey");
  if(code.size()!=6||key.empty()||jsonInt(response->body,"protocol")!=dbnet::PROTOCOL_VERSION||jsonInt(response->body,"gameplayVersion")!=dbnet::GAMEPLAY_VERSION){fail("VERSION MISMATCH","create_version");return false;}
  {std::lock_guard<std::mutex> lock(stateMutex_);roomCode_=code;hostKey_=key;}
  std::printf("MULTIPLAYER_ROOM_CODE %s\n",code.c_str());std::fflush(stdout);return true;
#else
  return false;
#endif
}

bool DesktopMultiplayer::connectWebSocket() {
  const std::string code = roomCode();
  std::string url = serviceUrl_ + "/v1/rooms/" + code + "/connect?role=" +
                    (role_ == Role::Host ? "host" : "guest") +
                    "&build=pass7-native&gameplay=5";
  if (role_ == Role::Host) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    url += "&key=" + hostKey_;
  }
#ifdef _WIN32
  UrlParts parts;
  if (!crack(url, parts)) {
    fail("CONNECTION FAILED","url",GetLastError());
    return false;
  }
  std::printf("MULTIPLAYER_CONNECT role=%s room=%s secure=%d path=/v1/rooms/<room>/connect build=pass7-native gameplay=%u\n",roleName(role_.load()),code.c_str(),parts.secure?1:0,dbnet::GAMEPLAY_VERSION);std::fflush(stdout);
  HINTERNET session =
      WinHttpOpen(L"DigitalBreakdown/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    fail("CONNECTION FAILED","session",GetLastError());
    return false;
  }
  WinHttpSetTimeouts(session,5000,5000,5000,10000);
  HINTERNET connection =
      WinHttpConnect(session, parts.host.c_str(), parts.port, 0);
  HINTERNET request =
      connection
          ? WinHttpOpenRequest(connection, L"GET", parts.path.c_str(), nullptr,
                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                               parts.secure ? WINHTTP_FLAG_SECURE : 0)
          : nullptr;
  publishHandles(session,connection,request,nullptr);
  BOOL ok = request &&
            WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
                             nullptr, 0) &&
            WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr);

  DWORD status=0;
  if (!ok) {
    const DWORD error = GetLastError();
    std::printf(
        "MULTIPLAYER_HANDSHAKE_FAILED stage=request error=%lu\n",
        static_cast<unsigned long>(error)
    );
    std::fflush(stdout);
  } else {
    DWORD statusSize = sizeof(status);

    if (WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
      std::printf(
          "MULTIPLAYER_HANDSHAKE_HTTP status=%lu\n",
          static_cast<unsigned long>(status)
      );
      std::fflush(stdout);
    }
  }

  if(!ok||status!=101){
    const DWORD error=ok?0:GetLastError();
    const std::string body=ok?readResponse(request):std::string{};
    const std::string serverError=jsonString(body,"error");
    const std::string visible=(status==404||serverError=="room_not_found"||serverError=="host_offline")?"ROOM NOT FOUND":serverError=="incompatible_build"?"VERSION MISMATCH":serverError=="late_join_unsupported"?"LATE JOIN UNSUPPORTED":serverError=="room_full"?"ROOM FULL":"CONNECTION FAILED";
    std::printf("MULTIPLAYER_HANDSHAKE_REJECT status=%lu reason=%s\n",static_cast<unsigned long>(status),serverError.empty()?"unknown":serverError.c_str());std::fflush(stdout);
    fail(visible,"http",error);
    const bool owns=releaseHandles(session,connection,request,nullptr);
    if(owns){if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);}
    return false;
  }

  HINTERNET socket = WinHttpWebSocketCompleteUpgrade(request, 0);

  if (ok && !socket) {
    const DWORD error = GetLastError();
    std::printf(
        "MULTIPLAYER_HANDSHAKE_FAILED stage=upgrade error=%lu\n",
        static_cast<unsigned long>(error)
    );
    std::fflush(stdout);
  }
  bool ownsRequest=false;{std::lock_guard<std::mutex> lock(handleMutex_);if(request_==request){request_=nullptr;ownsRequest=true;}}
  if(ownsRequest&&request)WinHttpCloseHandle(request);
  if (!socket) {
    fail("CONNECTION FAILED","upgrade",GetLastError());
    const bool owns=releaseHandles(session,connection,nullptr,nullptr);
    if(owns){if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);}
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(sendMutex_);
    publishHandles(session,connection,nullptr,socket);
    webSocket_ = socket;
  }
  std::printf("MULTIPLAYER_WEBSOCKET_UPGRADED role=%s room=%s\n",roleName(role_.load()),code.c_str());std::fflush(stdout);
  std::thread welcomeWatchdog([this,session]{
    for(int elapsed=0;elapsed<100&&!stop_&&phase_.load()==dbmultiplayer::Phase::Connecting;++elapsed)std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if(stop_||phase_.load()!=dbmultiplayer::Phase::Connecting)return;
    fail("CONNECTION FAILED","welcome_timeout");
    void* ownedSession=nullptr;void* ownedConnection=nullptr;void* ownedRequest=nullptr;void* ownedSocket=nullptr;
    {std::lock_guard<std::mutex> sendLock(sendMutex_);std::lock_guard<std::mutex> handleLock(handleMutex_);if(session_==session){ownedSession=session_;ownedConnection=connection_;ownedRequest=request_;ownedSocket=webSocket_;session_=connection_=request_=webSocket_=nullptr;}}
    if(ownedSocket)WinHttpCloseHandle(static_cast<HINTERNET>(ownedSocket));if(ownedRequest)WinHttpCloseHandle(static_cast<HINTERNET>(ownedRequest));if(ownedConnection)WinHttpCloseHandle(static_cast<HINTERNET>(ownedConnection));if(ownedSession)WinHttpCloseHandle(static_cast<HINTERNET>(ownedSession));
  });
  receiveLoop();
  welcomeWatchdog.join();
  {
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (webSocket_ == socket)
      webSocket_ = nullptr;
  }
  const bool owns=releaseHandles(session,connection,nullptr,socket);
  if(owns){WinHttpCloseHandle(socket);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);}
  return connected_;
#elif defined(__APPLE__)
  static const bool netReady=ix::initNetSystem();
  if(!netReady)return false;
  if(url.rfind("https://",0)==0)url.replace(0,5,"wss");else if(url.rfind("http://",0)==0)url.replace(0,4,"ws");
  std::printf("MULTIPLAYER_CONNECT role=%s room=%s secure=1 path=/v1/rooms/<room>/connect build=pass7-native gameplay=%u\n",roleName(role_.load()),code.c_str(),dbnet::GAMEPLAY_VERSION);std::fflush(stdout);
  ix::WebSocket socket;
  socket.disableAutomaticReconnection();
  socket.setHandshakeTimeout(5);
  socket.setUrl(url);
  socket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& message){
    if(message->type==ix::WebSocketMessageType::Open){std::printf("MULTIPLAYER_SOCKET_OPEN\n");std::fflush(stdout);return;}
    if(message->type==ix::WebSocketMessageType::Close){std::printf("MULTIPLAYER_SOCKET_CLOSE code=%d reason=%s\n",message->closeInfo.code,message->closeInfo.reason.c_str());std::fflush(stdout);connected_=false;return;}
    if(message->type==ix::WebSocketMessageType::Error){std::printf("MULTIPLAYER_SOCKET_ERROR retries=%u wait=%.2f reason=%s\n",message->errorInfo.retries,message->errorInfo.wait_time,message->errorInfo.reason.c_str());std::fflush(stdout);connected_=false;return;}
    if(message->type!=ix::WebSocketMessageType::Message)return;
    if(message->str.size()>dbnet::MAX_INCOMING_MESSAGE_BYTES){std::printf("MULTIPLAYER_PACKET_REJECT reason=size bytes=%zu\n",message->str.size());std::fflush(stdout);return;}
    Incoming item;
    item.binary=message->binary;
    if(item.binary)item.bytes.assign(message->str.begin(),message->str.end());
    else {
      item.text=message->str;
      if(jsonString(item.text,"type")=="welcome")acceptWelcome(item.text);
    }
    enqueueIncoming(std::move(item));
  });
  {std::lock_guard<std::mutex> lock(sendMutex_);webSocket_=&socket;}
  const auto connectResult=socket.connect(5);
  if(!connectResult.success){std::printf("MULTIPLAYER_CONNECT_FAILED %s\n",connectResult.errorStr.c_str());std::fflush(stdout);{std::lock_guard<std::mutex> lock(sendMutex_);if(webSocket_==&socket)webSocket_=nullptr;}return false;}
  socket.run();
  while(!stop_&&socket.getReadyState()!=ix::ReadyState::Closed)std::this_thread::sleep_for(std::chrono::milliseconds(20));
  socket.stop();
  {std::lock_guard<std::mutex> lock(sendMutex_);if(webSocket_==&socket)webSocket_=nullptr;}
  return connected_;
#else
  return false;
#endif
}

void DesktopMultiplayer::workerMain(){
  std::printf("MULTIPLAYER_WORKER role=%s\n",roleName(role_.load()));
  std::fflush(stdout);
  if(!checkServiceCompatibility())return;
  if(role_==Role::Host&&!createRoom()){
    if(!failed())fail("CONNECTION FAILED","create");
    return;
  }
  if(stop_)return;
  phase_=dbmultiplayer::transition(
    phase_.load(),dbmultiplayer::Event::RoomReady);
  setStatus("CONNECTING "+roomCode());
  if(!connectWebSocket()&&!stop_&&!failed())
    fail("CONNECTION FAILED","connect");
  if(connected_&&!stop_){
    if(role_==Role::Guest){
      connected_=false;
      phase_=dbmultiplayer::transition(
        phase_.load(),dbmultiplayer::Event::HostDisconnected);
      setStatus("HOST DISCONNECTED");
    }else{
      fail("SESSION EXPIRED","socket_ended");
    }
  }
  connected_=false;
}
void DesktopMultiplayer::receiveLoop(){
#ifdef _WIN32
std::vector<std::uint8_t> assembled;while(!stop_){HINTERNET socket=nullptr;{std::lock_guard<std::mutex> lock(handleMutex_);socket=static_cast<HINTERNET>(webSocket_);}if(!socket)break;std::uint8_t buffer[8192];DWORD read=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE type=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;DWORD result=WinHttpWebSocketReceive(socket,buffer,sizeof(buffer),&read,&type);if(result!=NO_ERROR){if(!stop_)std::printf("MULTIPLAYER_SOCKET_RECEIVE_FAILED error=%lu\n",static_cast<unsigned long>(result));std::fflush(stdout);break;}if(type==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE){USHORT closeCode=0;DWORD reasonBytes=0;WinHttpWebSocketQueryCloseStatus(socket,&closeCode,nullptr,0,&reasonBytes);std::printf("MULTIPLAYER_SOCKET_CLOSE code=%u reason_bytes=%lu\n",static_cast<unsigned>(closeCode),static_cast<unsigned long>(reasonBytes));std::fflush(stdout);break;}if(assembled.size()+read>dbnet::MAX_INCOMING_MESSAGE_BYTES){std::printf("MULTIPLAYER_PACKET_REJECT type=fragmented reason=size bytes=%zu\n",assembled.size()+read);std::fflush(stdout);break;}assembled.insert(assembled.end(),buffer,buffer+read);const bool complete=type==WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE||type==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;if(!complete)continue;Incoming item;if(type==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE){item.text.assign(reinterpret_cast<const char*>(assembled.data()),assembled.size());const std::string kind=jsonString(item.text,"type");if(kind=="welcome")acceptWelcome(item.text);else if(kind.empty()){std::printf("MULTIPLAYER_PACKET_REJECT type=text reason=malformed\n");std::fflush(stdout);}}else{item.binary=true;item.bytes=assembled;}enqueueIncoming(std::move(item));assembled.clear();if(stop_)break;}
#endif
}
bool DesktopMultiplayer::sendBinary(const std::vector<std::uint8_t>& packet){
dbnet::PacketHeader impairmentHeader;
if(dbnet::decodeHeader(packet.data(),packet.size(),impairmentHeader)){
  std::uint32_t* count=nullptr;int every=0;
  if(impairmentHeader.type==dbnet::MessageType::Snapshot){count=&snapshotSendCount_;every=dropSnapshotEvery_;}
  else if(impairmentHeader.type==dbnet::MessageType::Input){count=&inputSendCount_;every=dropInputEvery_;}
  if(count&&every>0&&++(*count)%static_cast<std::uint32_t>(every)==0){
    std::printf("MULTIPLAYER_NET_DROP type=%s sequence=%u\n",
      impairmentHeader.type==dbnet::MessageType::Snapshot?"snapshot":"input",impairmentHeader.sequence);
    std::fflush(stdout);return true;
  }
  if(netLatencyMs_||netJitterMs_){
    impairmentSeed_=impairmentSeed_*1664525u+1013904223u;
    const int span=netJitterMs_*2+1;
    const int jitter=span>1?static_cast<int>(impairmentSeed_%static_cast<std::uint32_t>(span))-netJitterMs_:0;
    std::this_thread::sleep_for(std::chrono::milliseconds(std::max(0,netLatencyMs_+jitter)));
  }
}
std::lock_guard<std::mutex> lock(sendMutex_);if(!webSocket_||packet.empty())return false;
#ifdef _WIN32
return WinHttpWebSocketSend(static_cast<HINTERNET>(webSocket_),WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,const_cast<std::uint8_t*>(packet.data()),static_cast<DWORD>(packet.size()))==NO_ERROR;
#elif defined(__APPLE__)
return static_cast<ix::WebSocket*>(webSocket_)->sendBinary(std::string(reinterpret_cast<const char*>(packet.data()),packet.size())).success;
#else
return false;
#endif
}

bool DesktopMultiplayer::sendText(const std::string& message){std::lock_guard<std::mutex> lock(sendMutex_);if(!webSocket_||message.empty())return false;
#ifdef _WIN32
const DWORD result=WinHttpWebSocketSend(static_cast<HINTERNET>(webSocket_),WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,const_cast<char*>(message.data()),static_cast<DWORD>(message.size()));
if(result!=NO_ERROR){std::printf("MULTIPLAYER_TEXT_SEND_FAILED error=%lu type=%s\n",static_cast<unsigned long>(result),jsonString(message,"type").c_str());std::fflush(stdout);}
return result==NO_ERROR;
#elif defined(__APPLE__)
return static_cast<ix::WebSocket*>(webSocket_)->send(message).success;
#else
return false;
#endif
}

bool DesktopMultiplayer::startMatch(){
  if(role_!=Role::Host||phase_.load()!=dbmultiplayer::Phase::Lobby||playerCount_.load()!=2){
    std::printf("MULTIPLAYER_START_IGNORED phase=%d players=%d\n",static_cast<int>(phase_.load()),playerCount_.load());std::fflush(stdout);return false;
  }
  startId_=++outgoingSequence_;
  phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::StartRequested);
  setStatus("STARTING");
  const std::string message="{\"type\":\"start_match\",\"startId\":"+std::to_string(startId_)+",\"gameplayVersion\":"+std::to_string(dbnet::GAMEPLAY_VERSION)+",\"roomSeed\":1,\"roomIndex\":0,\"startTick\":0}";
  if(sendText(message)){std::printf("MULTIPLAYER_START_REQUESTED id=%u\n",startId_);std::fflush(stdout);return true;}
  fail("CONNECTION FAILED","start_send");return false;
}

void DesktopMultiplayer::update(Game& game){
  std::deque<Incoming> messages;
  {std::lock_guard<std::mutex> lock(queueMutex_);messages.swap(incoming_);}
  if(role_!=Role::Offline){const std::string code=roomCode(),current=status();game.setNetworkRoom(code.c_str(),current.c_str(),connected_);}
  for(auto& message:messages){
    if(!message.binary){
      const std::string kind=jsonString(message.text,"type");
      if(!kind.empty())lastValidMessageMs_=steadyMilliseconds();
      if(kind=="lobby_state"){
        playerCount_=jsonInt(message.text,"playerCount",playerCount_.load());
        if(phase_.load()==dbmultiplayer::Phase::Lobby){
          setStatus(role_==Role::Host?(playerCount_==2?"READY 2/2 - START GAME":"WAITING FOR PLAYER 1/2"):"CONNECTED - WAITING FOR HOST");
          std::printf("MULTIPLAYER_LOBBY role=%s players=%d/2 room=%s\n",roleName(role_.load()),playerCount_.load(),roomCode().c_str());std::fflush(stdout);
        }
      }else if(kind=="player_joined"){
        const int id=jsonInt(message.text,"playerId");playerCount_=2;
        std::printf("MULTIPLAYER_PLAYER_JOINED player=%d\n",id);std::fflush(stdout);
        if(configuredGame_&&role_==Role::Host)game.setNetworkPeerActive(id,true);
      }else if(kind=="player_left"){
        const int id=jsonInt(message.text,"playerId");playerCount_=1;
        std::printf("MULTIPLAYER_PLAYER_LEFT player=%d\n",id);std::fflush(stdout);
        if(configuredGame_)game.setNetworkPeerActive(id,false);
        if(phase_.load()==dbmultiplayer::Phase::Lobby)setStatus("WAITING FOR PLAYER 1/2");
      }else if(kind=="start_match"){
        const std::uint32_t incomingStart=static_cast<std::uint32_t>(jsonInt(message.text,"startId",0));
        if(incomingStart==0||(startId_!=0&&startId_!=incomingStart)){std::printf("MULTIPLAYER_PACKET_REJECT type=start_match reason=id\n");std::fflush(stdout);continue;}
        startId_=incomingStart;setWorldContext({startId_,1,1,static_cast<std::uint16_t>(std::max(0,jsonInt(message.text,"roomIndex",0)))},"match_start");phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::StartReceived);phaseStartedMs_=steadyMilliseconds();setStatus("SYNCHRONIZING");
        if(!configuredGame_){game.restart();if(role_==Role::Host){game.configureNetworkHost();game.setNetworkPeerActive(1,true);}else game.configureNetworkGuest(playerId_);configuredGame_=true;}
        std::printf("MULTIPLAYER_SYNCHRONIZING role=%s start=%u\n",roleName(role_.load()),startId_);std::fflush(stdout);
        if(role_==Role::Host){const GameState& state=game.state();auto world=dbnet::captureWorld(state,dbnet::capturePlayers(state),static_cast<std::uint32_t>(std::max(0,state.frame)));world.world=worldContext_;emitCombatEvents(world);std::printf("MULTIPLAYER_AUTH_STATE_HASH tick=%u hash=%llu\n",world.tick,static_cast<unsigned long long>(dbnet::authoritativeStateHash(world)));std::printf("MULTIPLAYER_VISUAL_STATE entity=world id=0 tick=%u hash=%llu\n",world.tick,static_cast<unsigned long long>(dbnet::visualStateHash(world)));std::fflush(stdout);sendBinary(dbnet::encodeSnapshot(0,world,++outgoingSequence_));}
      }else if(kind=="start_ack"&&role_==Role::Host&&phase_.load()==dbmultiplayer::Phase::Synchronizing){
        if(static_cast<std::uint32_t>(jsonInt(message.text,"startId",0))==startId_)sendText("{\"type\":\"start_confirm\",\"startId\":"+std::to_string(startId_)+"}");
      }else if(kind=="start_confirm"){
        if(static_cast<std::uint32_t>(jsonInt(message.text,"startId",0))==startId_&&phase_.load()==dbmultiplayer::Phase::Synchronizing){
          phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::StartConfirmed);setStatus("PLAYING");
          std::printf("MULTIPLAYER_PLAYING role=%s room=%s start=%u\n",roleName(role_.load()),roomCode().c_str(),startId_);std::fflush(stdout);
        }
      }else if(kind=="heartbeat_ack"){
        if(status()=="CONNECTION UNSTABLE")setStatus(phase_.load()==dbmultiplayer::Phase::Playing?"PLAYING":phase_.load()==dbmultiplayer::Phase::Synchronizing?"SYNCHRONIZING":role_==Role::Host?(playerCount_==2?"READY 2/2 - START GAME":"WAITING FOR PLAYER 1/2"):"CONNECTED - WAITING FOR HOST");
      }else if(kind=="welcome"){
        // Welcome was validated on the receive thread; the Worker also sends
        // an authoritative lobby_state during the upgrade.
      }else if(kind=="error"){
        const std::string code=jsonString(message.text,"code");fail(code=="waiting_for_player"?"WAITING FOR PLAYER":"CONNECTION FAILED","server_control");
      }else if(kind=="match_closed"||kind=="host_disconnected"){
        std::printf("MULTIPLAYER_HOST_LEFT event=%s\n",kind.c_str());std::fflush(stdout);phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::HostDisconnected);
        endGameplaySession(game,kind=="match_closed"?"host_left":"host_disconnected","HOST DISCONNECTED");
      }else if(kind!="host_reconnected"){std::printf("MULTIPLAYER_PACKET_REJECT type=text reason=%s\n",kind.empty()?"malformed":kind.c_str());std::fflush(stdout);}
      continue;
    }
    dbnet::PacketHeader header;
    if(!dbnet::decodeHeader(message.bytes.data(),message.bytes.size(),header)){std::printf("MULTIPLAYER_PACKET_REJECT type=binary reason=header\n");std::fflush(stdout);continue;}
    lastValidMessageMs_=steadyMilliseconds();
    if(role_==Role::Host&&phase_.load()==dbmultiplayer::Phase::Playing&&header.type==dbnet::MessageType::Input){
      if(header.playerId>=lastInputSequence_.size()||header.sequence<=lastInputSequence_[header.playerId]){std::printf("MULTIPLAYER_PACKET_REJECT type=input reason=stale_or_player\n");std::fflush(stdout);continue;}
      dbnet::NetworkWorldContext packetWorld;dbnet::InputCommand input;if(dbnet::decodeInput(message.bytes.data(),message.bytes.size(),header,packetWorld,input)&&acceptWorldContext(packetWorld,header,false)){lastInputSequence_[header.playerId]=header.sequence;game.setNetworkPeerCommand(header.playerId,input);if(!loggedInput_){loggedInput_=true;std::printf("MULTIPLAYER_INPUT_RECEIVED player=%u sequence=%u\n",header.playerId,header.sequence);std::fflush(stdout);}}
    }else if(role_==Role::Guest&&header.type==dbnet::MessageType::Event){
      dbnet::GameplayEvent event;
      if(dbnet::decodeEvent(message.bytes.data(),message.bytes.size(),header,event)&&
         acceptWorldContext(event.world,header,false)){
        ++metrics_.eventsReceived;
        if(!eventTracker_.accept(event)){
          ++metrics_.duplicateEventsRejected;
          std::printf("MULTIPLAYER_EVENT_REJECTED event=%u reason=duplicate_or_out_of_order\n",event.eventId);std::fflush(stdout);continue;
        }
        const char* marker=event.type==dbnet::GameplayEventType::PlayerActionStarted
          ?"MULTIPLAYER_ACTION_CONFIRMED":eventMarker(event.type);
        if(event.type==dbnet::GameplayEventType::PlayerActionStarted||
           event.type==dbnet::GameplayEventType::VacuumStarted||
           event.type==dbnet::GameplayEventType::DischargeStarted)
          ++metrics_.confirmedActions;
        std::printf("%s event=%u source=%u target=%u tick=%u\n",marker,event.eventId,event.sourceEntityId,event.targetEntityId,event.authoritativeTick);std::fflush(stdout);
      }
    }else if(role_==Role::Guest&&header.type==dbnet::MessageType::Snapshot){
      if(header.sequence<=lastSnapshotSequence_){++metrics_.staleSnapshotsRejected;std::printf("MULTIPLAYER_PACKET_REJECT type=snapshot reason=stale\n");std::fflush(stdout);continue;}
      dbnet::WorldSnapshot snapshot;if(dbnet::decodeSnapshot(message.bytes.data(),message.bytes.size(),header,snapshot)&&acceptWorldContext(snapshot.world,header,true)){++metrics_.snapshotsReceived;const auto receivedAt=steadyMilliseconds();if(lastSnapshotReceiveMs_!=0){const auto gap=receivedAt-lastSnapshotReceiveMs_;if(gap>=100){std::printf("MULTIPLAYER_SNAPSHOT_GAP duration_ms=%lld\n",static_cast<long long>(gap));std::fflush(stdout);}}lastSnapshotReceiveMs_=receivedAt;lastSnapshotSequence_=header.sequence;std::printf("MULTIPLAYER_AUTH_STATE_HASH tick=%u hash=%llu\n",snapshot.tick,static_cast<unsigned long long>(dbnet::authoritativeStateHash(snapshot)));std::printf("MULTIPLAYER_VISUAL_STATE entity=world id=0 tick=%u hash=%llu\n",snapshot.tick,static_cast<unsigned long long>(dbnet::visualStateHash(snapshot)));std::fflush(stdout);snapshotInterpolator_.push(snapshot,receivedAt);const GameState& beforeState=game.state();const auto local=static_cast<std::uint8_t>(playerId_.load());const Vec3 before=beforeState.player.pos;const auto beforeAction=beforeState.meleeVisual.actionSequence;const auto& authoritative=snapshot.players[local];const float dx=before.x-authoritative.pos.x,dy=before.y-authoritative.pos.y,dz=before.z-authoritative.pos.z;metrics_.maximumPositionCorrection=std::max(metrics_.maximumPositionCorrection,std::sqrt(dx*dx+dy*dy+dz*dz));const float localProgress=beforeState.meleeVisual.visualDuration>0?1.0f-beforeState.meleeVisual.visualTimer/beforeState.meleeVisual.visualDuration:0.0f;metrics_.maximumActionPhaseCorrection=std::max(metrics_.maximumActionPhaseCorrection,std::fabs(localProgress-authoritative.actionProgress));if(beforeAction!=0&&beforeAction!=authoritative.actionSequence){++metrics_.correctedActions;if(authoritative.action==dbnet::NetActionState::None)++metrics_.cancelledActions;}dbnet::applyWorld(game,snapshot,local);const GameState& appliedState=game.state();auto applied=dbnet::captureWorld(appliedState,dbnet::capturePlayers(appliedState),snapshot.tick);applied.world=snapshot.world;if(dbnet::authoritativeStateHash(applied)==dbnet::authoritativeStateHash(snapshot))++metrics_.hashMatches;else ++metrics_.hashMismatches;if(!loggedSnapshot_){loggedSnapshot_=true;std::printf("MULTIPLAYER_SNAPSHOT_RECEIVED sequence=%u\n",header.sequence);std::fflush(stdout);}if(phase_.load()==dbmultiplayer::Phase::Synchronizing){sendText("{\"type\":\"start_ack\",\"startId\":"+std::to_string(startId_)+",\"snapshotSequence\":"+std::to_string(header.sequence)+"}");std::printf("MULTIPLAYER_INITIAL_SNAPSHOT_APPLIED sequence=%u\n",header.sequence);std::fflush(stdout);}}
    }else{std::printf("MULTIPLAYER_PACKET_REJECT type=binary reason=role_or_type\n");std::fflush(stdout);}
  }
  const auto currentPhase=phase_.load();
  if(!connected_&&configuredGame_&&
     (currentPhase==dbmultiplayer::Phase::Failed||
      currentPhase==dbmultiplayer::Phase::HostLeft)){
    endGameplaySession(game,currentPhase==dbmultiplayer::Phase::HostLeft
      ?"host_disconnected":"session_expired",
      currentPhase==dbmultiplayer::Phase::HostLeft
        ?"HOST DISCONNECTED":"SESSION EXPIRED");
    return;
  }
  if(connected_){
    const auto now=steadyMilliseconds(),silence=now-lastValidMessageMs_.load();
    if(now-lastHeartbeatMs_.load()>=3000){lastHeartbeatMs_=now;sendText("{\"type\":\"heartbeat\",\"sentAt\":"+std::to_string(now)+"}");}
    if(silence>=12000){
      if(role_==Role::Guest)phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::HostDisconnected);
      else phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::Failure);
      endGameplaySession(game,"message_timeout","SESSION EXPIRED");return;
    }
    if(silence>=7000&&status()!="CONNECTION UNSTABLE"){setStatus("CONNECTION UNSTABLE");std::printf("MULTIPLAYER_CONNECTION_UNSTABLE silence_ms=%lld\n",static_cast<long long>(silence));std::fflush(stdout);}
    if(phase_.load()==dbmultiplayer::Phase::Synchronizing&&now-phaseStartedMs_.load()>=10000){fail("CONNECTION FAILED","synchronization_timeout");return;}
  }
  if(!connected_||!configuredGame_||phase_.load()!=dbmultiplayer::Phase::Playing)return;
  const GameState& state=game.state();
  if(role_==Role::Host&&static_cast<std::uint32_t>(
      std::max(0,state.frame))<lastSnapshotTick_){
    auto next=worldContext_;
    ++next.runGeneration;
    next.roomGeneration=1;
    next.roomIndex=static_cast<std::uint16_t>(std::max(0,state.roomIndex));
    setWorldContext(next,"host_run_restart");
    lastSnapshotTick_=0;
  }
  const auto sendNow=steadyMilliseconds();
  if(role_==Role::Guest&&(lastInputSendMs_==0||sendNow-lastInputSendMs_>=16||state.input.commSignalPressed!=0)){
    lastInputSendMs_=sendNow;
    const PlayerCommand input=game.capturePlayerCommand(++outgoingSequence_,++localInputTick_);
    constexpr std::uint16_t predictedActionButtons=
      CommandVacuum|CommandMelee|CommandShoot;
    if((input.buttons&predictedActionButtons)!=0&&
       (lastPredictedButtons_&predictedActionButtons)==0)
      ++metrics_.predictedActions;
    lastPredictedButtons_=input.buttons;
    sendBinary(dbnet::encodeInput(static_cast<std::uint8_t>(playerId_.load()),worldContext_,input));
  }
  else if(role_==Role::Host&&static_cast<std::uint32_t>(state.frame)>=lastSnapshotTick_+3){lastSnapshotTick_=static_cast<std::uint32_t>(state.frame);const auto room=static_cast<std::uint16_t>(std::max(0,state.roomIndex));if(room!=worldContext_.roomIndex){auto next=worldContext_;++next.roomGeneration;next.roomIndex=room;setWorldContext(next,"host_room_transition");}auto world=dbnet::captureWorld(state,dbnet::capturePlayers(state),lastSnapshotTick_);world.world=worldContext_;emitCombatEvents(world);std::printf("MULTIPLAYER_AUTH_STATE_HASH tick=%u hash=%llu\n",world.tick,static_cast<unsigned long long>(dbnet::authoritativeStateHash(world)));std::printf("MULTIPLAYER_VISUAL_STATE entity=world id=0 tick=%u hash=%llu\n",world.tick,static_cast<unsigned long long>(dbnet::visualStateHash(world)));std::fflush(stdout);sendBinary(dbnet::encodeSnapshot(0,world,++outgoingSequence_));}
}
