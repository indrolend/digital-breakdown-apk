#include "DesktopMultiplayer.hpp"

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
#ifdef _WIN32
std::string readResponse(HINTERNET request){std::string response;DWORD available=0;while(WinHttpQueryDataAvailable(request,&available)&&available){const std::size_t old=response.size();response.resize(old+available);DWORD read=0;if(!WinHttpReadData(request,response.data()+old,available,&read)){response.resize(old);break;}response.resize(old+read);}return response;}
#endif
}

DesktopMultiplayer::~DesktopMultiplayer(){disconnect();}
std::string DesktopMultiplayer::roomCode()const{std::lock_guard<std::mutex> lock(stateMutex_);return roomCode_;}
std::string DesktopMultiplayer::status()const{std::lock_guard<std::mutex> lock(stateMutex_);return status_;}
void DesktopMultiplayer::setStatus(const std::string& value){std::lock_guard<std::mutex> lock(stateMutex_);status_=value;}
void DesktopMultiplayer::host(const std::string& serviceUrl){begin(Role::Host,serviceUrl,{});}
void DesktopMultiplayer::join(const std::string& serviceUrl,const std::string& roomCode){const std::string code=dbmultiplayer::normalizeRoomCode(roomCode);if(code.empty()){role_=Role::Guest;phase_=dbmultiplayer::Phase::Failed;setStatus("ROOM NOT FOUND");std::printf("MULTIPLAYER_REJECT stage=room_code reason=invalid\n");std::fflush(stdout);return;}begin(Role::Guest,serviceUrl,code);}
void DesktopMultiplayer::begin(Role role,const std::string& service,const std::string& code){if(pending()||connected()){std::printf("MULTIPLAYER_DUPLICATE_IGNORED role=%s\n",roleName(role));std::fflush(stdout);return;}disconnect();role_=role;phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::Begin);serviceUrl_=service;{std::lock_guard<std::mutex> lock(stateMutex_);roomCode_=code;hostKey_.clear();status_=role==Role::Host?"JOINING":"JOINING "+code;}std::printf("MULTIPLAYER_BEGIN role=%s service=%s room=%s\n",roleName(role),service.c_str(),code.c_str());std::fflush(stdout);stop_=false;configuredGame_=false;outgoingSequence_=0;lastSnapshotTick_=0;lastSnapshotSequence_=0;lastInputSequence_.fill(0);worker_=std::thread(&DesktopMultiplayer::workerMain,this);}
void DesktopMultiplayer::disconnect(){stop_=true;void* session=nullptr;void* connection=nullptr;void* request=nullptr;void* socket=nullptr;{std::lock_guard<std::mutex> sendLock(sendMutex_);std::lock_guard<std::mutex> handleLock(handleMutex_);session=session_;connection=connection_;request=request_;socket=webSocket_;session_=connection_=request_=webSocket_=nullptr;}
#ifdef _WIN32
if(socket)WinHttpCloseHandle(static_cast<HINTERNET>(socket));if(request)WinHttpCloseHandle(static_cast<HINTERNET>(request));if(connection)WinHttpCloseHandle(static_cast<HINTERNET>(connection));if(session)WinHttpCloseHandle(static_cast<HINTERNET>(session));
#elif defined(__APPLE__)
if(socket)static_cast<ix::WebSocket*>(socket)->stop();
#endif
if(worker_.joinable()&&worker_.get_id()!=std::this_thread::get_id())worker_.join();connected_=false;role_=Role::Offline;phase_=dbmultiplayer::Phase::Offline;configuredGame_=false;}

void DesktopMultiplayer::fail(const std::string& visibleStatus,const char* stage,unsigned long error){connected_=false;phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::Failure);setStatus(visibleStatus);std::printf("MULTIPLAYER_FAILED stage=%s status=%s error=%lu\n",stage,visibleStatus.c_str(),error);std::fflush(stdout);}
void DesktopMultiplayer::publishHandles(void* session,void* connection,void* request,void* socket){std::lock_guard<std::mutex> lock(handleMutex_);session_=session;connection_=connection;request_=request;if(socket)webSocket_=socket;}
bool DesktopMultiplayer::releaseHandles(void* session,void* connection,void* request,void* socket){std::lock_guard<std::mutex> lock(handleMutex_);const bool owns=session&&session_==session;if(session_==session)session_=nullptr;if(connection_==connection)connection_=nullptr;if(request_==request)request_=nullptr;if(webSocket_==socket)webSocket_=nullptr;return owns;}

std::string DesktopMultiplayer::jsonString(const std::string& json,const char* key){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return{};at=json.find(':',at+needle.size());if(at==std::string::npos)return{};at=json.find('"',at+1);if(at==std::string::npos)return{};const std::size_t end=json.find('"',at+1);return end==std::string::npos?std::string{}:json.substr(at+1,end-at-1);}
int DesktopMultiplayer::jsonInt(const std::string& json,const char* key,int fallback){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return fallback;at=json.find(':',at+needle.size());if(at==std::string::npos)return fallback;try{return std::stoi(json.substr(at+1));}catch(...){return fallback;}}

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
  setStatus(role_==Role::Host?"ROOM "+roomCode():"JOINED "+roomCode());
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
  const char body[] = "{\"gameplayVersion\":5}";
  BOOL ok = request &&
            WinHttpSendRequest(request, L"Content-Type: application/json\r\n",
                               static_cast<DWORD>(-1), const_cast<char *>(body),
                               sizeof(body) - 1, sizeof(body) - 1, 0) &&
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
  const auto response=client.post(serviceUrl_+"/v1/rooms","{\"gameplayVersion\":5}",args);
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
    const std::string visible=(status==404||serverError=="room_not_found"||serverError=="host_offline")?"ROOM NOT FOUND":serverError=="incompatible_build"?"VERSION MISMATCH":"CONNECTION FAILED";
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
    Incoming item;
    item.binary=message->binary;
    if(item.binary)item.bytes.assign(message->str.begin(),message->str.end());
    else {
      item.text=message->str;
      if(jsonString(item.text,"type")=="welcome")acceptWelcome(item.text);
    }
    std::lock_guard<std::mutex> lock(queueMutex_);incoming_.push_back(std::move(item));
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

void DesktopMultiplayer::workerMain(){std::printf("MULTIPLAYER_WORKER role=%s\n",roleName(role_.load()));std::fflush(stdout);if(role_==Role::Host&&!createRoom()){if(!failed())fail("CONNECTION FAILED","create");return;}if(stop_)return;setStatus("CONNECTING "+roomCode());if(!connectWebSocket()&&!stop_&&!failed())fail("CONNECTION FAILED","connect");if(connected_&&!stop_){fail("CONNECTION FAILED","socket_ended");}connected_=false;}
void DesktopMultiplayer::receiveLoop(){
#ifdef _WIN32
std::vector<std::uint8_t> assembled;while(!stop_){HINTERNET socket=nullptr;{std::lock_guard<std::mutex> lock(handleMutex_);socket=static_cast<HINTERNET>(webSocket_);}if(!socket)break;std::uint8_t buffer[8192];DWORD read=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE type=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;DWORD result=WinHttpWebSocketReceive(socket,buffer,sizeof(buffer),&read,&type);if(result!=NO_ERROR){if(!stop_)std::printf("MULTIPLAYER_SOCKET_RECEIVE_FAILED error=%lu\n",static_cast<unsigned long>(result));std::fflush(stdout);break;}if(type==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE){USHORT closeCode=0;DWORD reasonBytes=0;WinHttpWebSocketQueryCloseStatus(socket,&closeCode,nullptr,0,&reasonBytes);std::printf("MULTIPLAYER_SOCKET_CLOSE code=%u reason_bytes=%lu\n",static_cast<unsigned>(closeCode),static_cast<unsigned long>(reasonBytes));std::fflush(stdout);break;}assembled.insert(assembled.end(),buffer,buffer+read);const bool complete=type==WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE||type==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;if(!complete)continue;Incoming item;if(type==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE){item.text.assign(reinterpret_cast<const char*>(assembled.data()),assembled.size());const std::string kind=jsonString(item.text,"type");if(kind=="welcome")acceptWelcome(item.text);else if(kind.empty()){std::printf("MULTIPLAYER_PACKET_REJECT type=text reason=malformed\n");std::fflush(stdout);}}else{item.binary=true;item.bytes=assembled;} {std::lock_guard<std::mutex> lock(queueMutex_);incoming_.push_back(std::move(item));}assembled.clear();if(stop_)break;}
#endif
}
bool DesktopMultiplayer::sendBinary(const std::vector<std::uint8_t>& packet){std::lock_guard<std::mutex> lock(sendMutex_);if(!webSocket_||packet.empty())return false;
#ifdef _WIN32
return WinHttpWebSocketSend(static_cast<HINTERNET>(webSocket_),WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,const_cast<std::uint8_t*>(packet.data()),static_cast<DWORD>(packet.size()))==NO_ERROR;
#elif defined(__APPLE__)
return static_cast<ix::WebSocket*>(webSocket_)->sendBinary(std::string(reinterpret_cast<const char*>(packet.data()),packet.size())).success;
#else
return false;
#endif
}

void DesktopMultiplayer::update(Game& game){std::deque<Incoming> messages;{std::lock_guard<std::mutex> lock(queueMutex_);messages.swap(incoming_);}if(connected_&&!configuredGame_){if(role_==Role::Host){game.restart();game.configureNetworkHost();}else{game.restart();game.configureNetworkGuest(playerId_);}configuredGame_=true;}if(role_!=Role::Offline){const std::string code=roomCode(),current=status();game.setNetworkRoom(code.c_str(),current.c_str(),connected_);}for(auto& message:messages){if(!message.binary){const std::string kind=jsonString(message.text,"type");if(kind=="player_joined"){const int id=jsonInt(message.text,"playerId");std::printf("MULTIPLAYER_PLAYER_JOINED player=%d\n",id);std::fflush(stdout);if(role_==Role::Host)game.setNetworkPeerActive(id,true);}else if(kind=="player_left"){const int id=jsonInt(message.text,"playerId");std::printf("MULTIPLAYER_PLAYER_LEFT player=%d\n",id);std::fflush(stdout);game.setNetworkPeerActive(id,false);}else if(kind=="match_closed"||kind=="host_disconnected"){std::printf("MULTIPLAYER_HOST_LEFT event=%s\n",kind.c_str());std::fflush(stdout);setStatus("HOST LEFT");phase_=dbmultiplayer::transition(phase_.load(),dbmultiplayer::Event::HostDisconnected);connected_=false;configuredGame_=false;}else if(kind!="welcome"){std::printf("MULTIPLAYER_PACKET_REJECT type=text reason=%s\n",kind.empty()?"malformed":kind.c_str());std::fflush(stdout);}continue;}dbnet::PacketHeader header;if(!dbnet::decodeHeader(message.bytes.data(),message.bytes.size(),header)){std::printf("MULTIPLAYER_PACKET_REJECT type=binary reason=header\n");std::fflush(stdout);continue;}if(role_==Role::Host&&header.type==dbnet::MessageType::Input){if(header.playerId>=lastInputSequence_.size()||header.sequence<=lastInputSequence_[header.playerId]){std::printf("MULTIPLAYER_PACKET_REJECT type=input reason=stale_or_player\n");std::fflush(stdout);continue;}dbnet::InputCommand input;if(dbnet::decodeInput(message.bytes.data(),message.bytes.size(),header,input)){lastInputSequence_[header.playerId]=header.sequence;game.setNetworkPeerInput(header.playerId,input.sequence,input.moveX,input.moveZ,input.yaw,input.pitch,input.buttons);}}else if(role_==Role::Guest&&header.type==dbnet::MessageType::Snapshot){if(header.sequence<=lastSnapshotSequence_){std::printf("MULTIPLAYER_PACKET_REJECT type=snapshot reason=stale\n");std::fflush(stdout);continue;}dbnet::WorldSnapshot snapshot;if(dbnet::decodeSnapshot(message.bytes.data(),message.bytes.size(),header,snapshot)){lastSnapshotSequence_=header.sequence;dbnet::applyWorld(game.networkMutableState(),snapshot,static_cast<std::uint8_t>(playerId_.load()));}}else{std::printf("MULTIPLAYER_PACKET_REJECT type=binary reason=role_or_type\n");std::fflush(stdout);}}
if(!connected_||!configuredGame_)return;const GameState& state=game.state();if(role_==Role::Guest&&(state.frame%2==0||state.input.commSignalPressed!=0)){dbnet::InputCommand input;input.sequence=++outgoingSequence_;input.tick=static_cast<std::uint32_t>(std::max(0,state.frame));input.moveX=clampf((state.input.right?1.0f:0.0f)-(state.input.left?1.0f:0.0f)+state.input.touchMoveX,-1,1);input.moveZ=clampf((state.input.forward?1.0f:0.0f)-(state.input.back?1.0f:0.0f)+state.input.touchMoveZ,-1,1);input.yaw=state.camera.yaw;input.pitch=state.camera.pitch;if(state.input.forward)input.buttons|=dbnet::Forward;if(state.input.back)input.buttons|=dbnet::Back;if(state.input.left)input.buttons|=dbnet::Left;if(state.input.right)input.buttons|=dbnet::Right;if(state.input.sprint||state.input.touchSprint)input.buttons|=dbnet::Sprint;if(state.input.jumpPressed)input.buttons|=dbnet::Jump;if(state.input.primaryHeld||state.input.touchPrimaryHeld)input.buttons|=dbnet::Vacuum;if(state.input.meleePressed)input.buttons|=dbnet::Melee;if(state.input.shootPressed)input.buttons|=dbnet::Shoot;if(state.input.cameraTogglePressed)input.buttons|=dbnet::CameraToggle;if(state.input.wiggleAxis<0)input.buttons|=dbnet::WiggleLeft;else if(state.input.wiggleAxis>0)input.buttons|=dbnet::WiggleRight;if(state.input.commSignalPressed==1)input.buttons|=dbnet::CommHelp;else if(state.input.commSignalPressed==2)input.buttons|=dbnet::CommPing;else if(state.input.commSignalPressed==3)input.buttons|=dbnet::CommGroup;else if(state.input.commSignalPressed==4)input.buttons|=dbnet::CommOk;sendBinary(dbnet::encodeInput(static_cast<std::uint8_t>(playerId_.load()),input));}else if(role_==Role::Host&&static_cast<std::uint32_t>(state.frame)>=lastSnapshotTick_+3){lastSnapshotTick_=static_cast<std::uint32_t>(state.frame);auto world=dbnet::captureWorld(state,dbnet::capturePlayers(state),lastSnapshotTick_);sendBinary(dbnet::encodeSnapshot(0,world,++outgoingSequence_));}}
