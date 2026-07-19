#include "DesktopMultiplayer.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {
std::wstring wide(const std::string& value){if(value.empty())return{};const int n=MultiByteToWideChar(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),nullptr,0);std::wstring out(static_cast<std::size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,0,value.c_str(),static_cast<int>(value.size()),out.data(),n);return out;}
struct UrlParts{bool secure=false;std::wstring host,path;INTERNET_PORT port=0;};
bool crack(const std::string& value,UrlParts& out){const std::wstring w=wide(value);URL_COMPONENTS c{};c.dwStructSize=sizeof(c);c.dwSchemeLength=c.dwHostNameLength=c.dwUrlPathLength=c.dwExtraInfoLength=static_cast<DWORD>(-1);if(!WinHttpCrackUrl(w.c_str(),0,0,&c))return false;out.secure=c.nScheme==INTERNET_SCHEME_HTTPS;out.host.assign(c.lpszHostName,c.dwHostNameLength);out.path.assign(c.lpszUrlPath,c.dwUrlPathLength);if(c.lpszExtraInfo&&c.dwExtraInfoLength)out.path.append(c.lpszExtraInfo,c.dwExtraInfoLength);out.port=c.nPort;return true;}
std::string upperCode(std::string code){code.erase(std::remove_if(code.begin(),code.end(),[](unsigned char c){return std::isspace(c)!=0;}),code.end());std::transform(code.begin(),code.end(),code.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});return code;}
}

DesktopMultiplayer::~DesktopMultiplayer(){disconnect();}
std::string DesktopMultiplayer::roomCode()const{std::lock_guard<std::mutex> lock(stateMutex_);return roomCode_;}
std::string DesktopMultiplayer::status()const{std::lock_guard<std::mutex> lock(stateMutex_);return status_;}
void DesktopMultiplayer::setStatus(const std::string& value){std::lock_guard<std::mutex> lock(stateMutex_);status_=value;}
void DesktopMultiplayer::host(const std::string& serviceUrl){begin(Role::Host,serviceUrl,{});}
void DesktopMultiplayer::join(const std::string& serviceUrl,const std::string& roomCode){begin(Role::Guest,serviceUrl,upperCode(roomCode));}
void DesktopMultiplayer::begin(Role role,const std::string& service,const std::string& code){disconnect();role_=role;serviceUrl_=service;{std::lock_guard<std::mutex> lock(stateMutex_);roomCode_=code;status_=role==Role::Host?"CREATING ROOM":"JOINING "+code;}stop_=false;configuredGame_=false;worker_=std::thread(&DesktopMultiplayer::workerMain,this);}
void DesktopMultiplayer::disconnect(){stop_=true;void* socket=nullptr;{std::lock_guard<std::mutex> lock(sendMutex_);socket=webSocket_;webSocket_=nullptr;}if(socket)WinHttpWebSocketClose(static_cast<HINTERNET>(socket),WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,nullptr,0);if(worker_.joinable())worker_.join();connected_=false;role_=Role::Offline;configuredGame_=false;}

std::string DesktopMultiplayer::jsonString(const std::string& json,const char* key){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return{};at=json.find(':',at+needle.size());if(at==std::string::npos)return{};at=json.find('"',at+1);if(at==std::string::npos)return{};const std::size_t end=json.find('"',at+1);return end==std::string::npos?std::string{}:json.substr(at+1,end-at-1);}
int DesktopMultiplayer::jsonInt(const std::string& json,const char* key,int fallback){const std::string needle=std::string("\"")+key+"\"";std::size_t at=json.find(needle);if(at==std::string::npos)return fallback;at=json.find(':',at+needle.size());if(at==std::string::npos)return fallback;try{return std::stoi(json.substr(at+1));}catch(...){return fallback;}}

bool DesktopMultiplayer::createRoom() {
  UrlParts url;
  if (!crack(serviceUrl_ + "/v1/rooms", url))
    return false;
  HINTERNET session =
      WinHttpOpen(L"DigitalBreakdown/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    return false;
  HINTERNET connection = WinHttpConnect(session, url.host.c_str(), url.port, 0);
  HINTERNET request =
      connection
          ? WinHttpOpenRequest(connection, L"POST", url.path.c_str(), nullptr,
                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                               url.secure ? WINHTTP_FLAG_SECURE : 0)
          : nullptr;
  const char body[] = "{\"gameplayVersion\":2}";
  BOOL ok = request &&
            WinHttpSendRequest(request, L"Content-Type: application/json\r\n",
                               static_cast<DWORD>(-1), const_cast<char *>(body),
                               sizeof(body) - 1, sizeof(body) - 1, 0) &&
            WinHttpReceiveResponse(request, nullptr);
  std::string response;
  if (ok) {
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available) {
      const std::size_t old = response.size();
      response.resize(old + available);
      DWORD read = 0;
      if (!WinHttpReadData(request, response.data() + old, available, &read))
        break;
      response.resize(old + read);
    }
  }
  if (request)
    WinHttpCloseHandle(request);
  if (connection)
    WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);
  if (!ok)
    return false;
  const std::string code = jsonString(response, "code"),
                    key = jsonString(response, "hostKey");
  if (code.size() != 6 || key.empty())
    return false;
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    roomCode_ = code;
    hostKey_ = key;
  }
  std::printf("MULTIPLAYER_ROOM_CODE %s\n", code.c_str());
  std::fflush(stdout);
  return true;
}

bool DesktopMultiplayer::connectWebSocket() {
  const std::string code = roomCode();
  std::string url = serviceUrl_ + "/v1/rooms/" + code + "/connect?role=" +
                    (role_ == Role::Host ? "host" : "guest") +
                    "&build=pass7-native&gameplay=2";
  if (role_ == Role::Host) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    url += "&key=" + hostKey_;
  }
  UrlParts parts;
  if (!crack(url, parts))
    return false;
  HINTERNET session =
      WinHttpOpen(L"DigitalBreakdown/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session)
    return false;
  HINTERNET connection =
      WinHttpConnect(session, parts.host.c_str(), parts.port, 0);
  HINTERNET request =
      connection
          ? WinHttpOpenRequest(connection, L"GET", parts.path.c_str(), nullptr,
                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                               parts.secure ? WINHTTP_FLAG_SECURE : 0)
          : nullptr;
  BOOL ok = request &&
            WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,
                             nullptr, 0) &&
            WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr);
  HINTERNET socket = ok ? WinHttpWebSocketCompleteUpgrade(request, 0) : nullptr;
  if (request)
    WinHttpCloseHandle(request);
  if (connection)
    WinHttpCloseHandle(connection);
  if (!socket) {
    WinHttpCloseHandle(session);
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(sendMutex_);
    webSocket_ = socket;
  }
  receiveLoop();
  {
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (webSocket_ == socket)
      webSocket_ = nullptr;
  }
  WinHttpCloseHandle(socket);
  WinHttpCloseHandle(session);
  return true;
}

void DesktopMultiplayer::workerMain(){if(role_==Role::Host&&!createRoom()){setStatus("ROOM CREATE FAILED");return;}setStatus("CONNECTING "+roomCode());if(!connectWebSocket()&&!stop_)setStatus("CONNECTION FAILED");connected_=false;}
void DesktopMultiplayer::receiveLoop(){std::vector<std::uint8_t> assembled;while(!stop_){std::uint8_t buffer[8192];DWORD read=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE type=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;DWORD result=WinHttpWebSocketReceive(static_cast<HINTERNET>(webSocket_),buffer,sizeof(buffer),&read,&type);if(result!=NO_ERROR)break;if(type==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)break;assembled.insert(assembled.end(),buffer,buffer+read);const bool complete=type==WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE||type==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;if(!complete)continue;Incoming item;if(type==WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE){item.text.assign(reinterpret_cast<const char*>(assembled.data()),assembled.size());const std::string kind=jsonString(item.text,"type");if(kind=="welcome"){playerId_=jsonInt(item.text,"playerId",0);connected_=true;setStatus(role_==Role::Host?"ROOM "+roomCode():"JOINED "+roomCode());std::printf("MULTIPLAYER_CONNECTED role=%s player=%d room=%s\n",role_==Role::Host?"host":"guest",playerId_.load(),roomCode().c_str());std::fflush(stdout);}}else{item.binary=true;item.bytes=assembled;} {std::lock_guard<std::mutex> lock(queueMutex_);incoming_.push_back(std::move(item));}assembled.clear();}}
bool DesktopMultiplayer::sendBinary(const std::vector<std::uint8_t>& packet){std::lock_guard<std::mutex> lock(sendMutex_);if(!webSocket_||packet.empty())return false;return WinHttpWebSocketSend(static_cast<HINTERNET>(webSocket_),WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,const_cast<std::uint8_t*>(packet.data()),static_cast<DWORD>(packet.size()))==NO_ERROR;}

void DesktopMultiplayer::update(Game& game){std::deque<Incoming> messages;{std::lock_guard<std::mutex> lock(queueMutex_);messages.swap(incoming_);}if(connected_&&!configuredGame_){if(role_==Role::Host){game.restart();game.configureNetworkHost();}else{game.restart();game.configureNetworkGuest(playerId_);}configuredGame_=true;}if(role_!=Role::Offline){const std::string code=roomCode(),current=status();game.setNetworkRoom(code.c_str(),current.c_str(),connected_);}for(auto& message:messages){if(!message.binary){const std::string kind=jsonString(message.text,"type");if(kind=="player_joined"&&role_==Role::Host)game.setNetworkPeerActive(jsonInt(message.text,"playerId"),true);else if(kind=="player_left")game.setNetworkPeerActive(jsonInt(message.text,"playerId"),false);else if(kind=="match_closed"){setStatus("HOST LEFT");connected_=false;}continue;}dbnet::PacketHeader header;if(!dbnet::decodeHeader(message.bytes.data(),message.bytes.size(),header))continue;if(role_==Role::Host&&header.type==dbnet::MessageType::Input){dbnet::InputCommand input;if(dbnet::decodeInput(message.bytes.data(),message.bytes.size(),header,input))game.setNetworkPeerInput(header.playerId,input.sequence,input.moveX,input.moveZ,input.yaw,input.pitch,input.buttons);}else if(role_==Role::Guest&&header.type==dbnet::MessageType::Snapshot){dbnet::WorldSnapshot snapshot;if(dbnet::decodeSnapshot(message.bytes.data(),message.bytes.size(),header,snapshot))dbnet::applyWorld(game.networkMutableState(),snapshot,static_cast<std::uint8_t>(playerId_.load()));}}
if(!connected_||!configuredGame_)return;const GameState& state=game.state();if(role_==Role::Guest&&state.frame%2==0){dbnet::InputCommand input;input.sequence=++outgoingSequence_;input.tick=static_cast<std::uint32_t>(std::max(0,state.frame));input.moveX=clampf((state.input.right?1.0f:0.0f)-(state.input.left?1.0f:0.0f)+state.input.touchMoveX,-1,1);input.moveZ=clampf((state.input.forward?1.0f:0.0f)-(state.input.back?1.0f:0.0f)+state.input.touchMoveZ,-1,1);input.yaw=state.camera.yaw;input.pitch=state.camera.pitch;if(state.input.forward)input.buttons|=dbnet::Forward;if(state.input.back)input.buttons|=dbnet::Back;if(state.input.left)input.buttons|=dbnet::Left;if(state.input.right)input.buttons|=dbnet::Right;if(state.input.sprint||state.input.touchSprint)input.buttons|=dbnet::Sprint;if(state.input.jumpPressed)input.buttons|=dbnet::Jump;if(state.input.primaryHeld||state.input.touchPrimaryHeld)input.buttons|=dbnet::Vacuum;if(state.input.meleePressed)input.buttons|=dbnet::Melee;if(state.input.shootPressed)input.buttons|=dbnet::Shoot;if(state.input.cameraTogglePressed)input.buttons|=dbnet::CameraToggle;sendBinary(dbnet::encodeInput(static_cast<std::uint8_t>(playerId_.load()),input));}else if(role_==Role::Host&&static_cast<std::uint32_t>(state.frame)>=lastSnapshotTick_+3){lastSnapshotTick_=static_cast<std::uint32_t>(state.frame);auto world=dbnet::captureWorld(state,dbnet::capturePlayers(state),lastSnapshotTick_);sendBinary(dbnet::encodeSnapshot(0,world,++outgoingSequence_));}}
