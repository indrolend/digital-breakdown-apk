package com.indrolend.digitalbreakdown;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.util.Locale;
import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;
import okio.ByteString;
import org.json.JSONObject;

public final class MultiplayerClient {
    public interface Listener { void onStatus(String status, String roomCode, boolean connected); }
    private static final MediaType JSON = MediaType.get("application/json; charset=utf-8");
    private static final int GAMEPLAY_VERSION = 7;
    private static final long MAX_QUEUED_BYTES = 512L * 1024L;
    private final Activity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Listener listener;
    private final OkHttpClient http = new OkHttpClient();
    private final String serviceUrl;
    private volatile WebSocket socket;
    private volatile boolean host;
    private volatile boolean closedByUser = true;
    private volatile boolean connected;
    private volatile int connectionGeneration;
    private volatile int reconnectAttempts;
    private volatile String role = "guest";
    private volatile String hostKey = "";
    private volatile String roomCode = "";

    public MultiplayerClient(Activity activity, String serviceUrl, Listener listener) {
        this.activity = activity;
        this.serviceUrl = trimSlash(serviceUrl);
        this.listener = listener;
    }

    public void host() {
        close(); closedByUser = false; host = true; role = "host"; hostKey = ""; status("CREATING ROOM", "", false);
        final int generation = connectionGeneration;
        Request request = new Request.Builder().url(serviceUrl + "/v1/rooms")
            .post(RequestBody.create("{\"gameplayVersion\":" + GAMEPLAY_VERSION + "}", JSON)).build();
        http.newCall(request).enqueue(new Callback() {
            @Override public void onFailure(Call call, java.io.IOException error) { status("ROOM CREATE FAILED", "", false); }
            @Override public void onResponse(Call call, Response response) {
                try (Response done = response) {
                    if (closedByUser || generation != connectionGeneration) return;
                    if (!done.isSuccessful() || done.body() == null) { status("ROOM CREATE FAILED", "", false); return; }
                    JSONObject json = new JSONObject(done.body().string());
                    roomCode = json.getString("code");
                    hostKey = json.getString("hostKey");
                    Log.i("DBMULTI", "MULTIPLAYER_ROOM_CODE " + roomCode);
                    connect("host", hostKey);
                } catch (Exception error) { status("ROOM CREATE FAILED", "", false); }
            }
        });
    }

    public void join(String code) {
        close(); closedByUser = false; host = false; role = "guest"; hostKey = ""; roomCode = code.trim().toUpperCase(Locale.US);
        if (roomCode.length() != 6) { status("ENTER 6-CHARACTER CODE", roomCode, false); return; }
        connect("guest", "");
    }

    private void connect(String role, String key) {
        this.role = role;
        if (!key.isEmpty()) hostKey = key;
        final int generation = ++connectionGeneration;
        connected = false;
        status("CONNECTING " + roomCode, roomCode, false);
        String wsBase = serviceUrl.startsWith("https://") ? "wss://" + serviceUrl.substring(8)
            : "ws://" + serviceUrl.substring("http://".length());
        String url = wsBase + "/v1/rooms/" + roomCode + "/connect?role=" + role
            + "&build=pass7-native-android&gameplay=" + GAMEPLAY_VERSION + (key.isEmpty() ? "" : "&key=" + key);
        socket = http.newWebSocket(new Request.Builder().url(url).build(), new WebSocketListener() {
            @Override public void onMessage(WebSocket webSocket, String text) {
                if (generation != connectionGeneration) return;
                NativeBridge.onNetworkControl(text);
                try {
                    JSONObject json = new JSONObject(text);
                    if ("welcome".equals(json.optString("type"))) {
                        int playerId = json.getInt("playerId");
                        connected = true;
                        reconnectAttempts = 0;
                        NativeBridge.configureNetwork(host, playerId, roomCode, host ? "ROOM " + roomCode : "JOINED " + roomCode);
                        status(host ? "ROOM " + roomCode : "JOINED " + roomCode, roomCode, true);
                    } else if ("host_disconnected".equals(json.optString("type"))) {
                        status("HOST RECONNECTING", roomCode, false);
                    } else if ("host_reconnected".equals(json.optString("type"))) {
                        status(host ? "ROOM " + roomCode : "JOINED " + roomCode, roomCode, true);
                    } else if ("match_closed".equals(json.optString("type"))) {
                        connected = false;
                        closedByUser = true;
                        status("HOST LEFT", roomCode, false);
                    }
                } catch (Exception ignored) { }
            }
            @Override public void onMessage(WebSocket webSocket, ByteString bytes) { if (generation == connectionGeneration) NativeBridge.onNetworkPacket(bytes.toByteArray()); }
            @Override public void onFailure(WebSocket webSocket, Throwable error, Response response) { handleDisconnect(generation, "CONNECTION FAILED"); }
            @Override public void onClosed(WebSocket webSocket, int code, String reason) { handleDisconnect(generation, "DISCONNECTED"); }
        });
    }

    private void handleDisconnect(int generation, String finalStatus) {
        if (generation != connectionGeneration || closedByUser) return;
        connected = false;
        WebSocket active = socket;
        if (active != null) active.cancel();
        if (reconnectAttempts >= 6 || roomCode.isEmpty()) {
            status(finalStatus, roomCode, false);
            return;
        }
        int attempt = ++reconnectAttempts;
        long delayMs = Math.min(6000L, 350L * (1L << Math.min(attempt, 4)));
        status("RECONNECTING " + roomCode, roomCode, false);
        handler.postDelayed(() -> {
            if (!closedByUser && generation == connectionGeneration && !roomCode.isEmpty()) connect(role, host ? hostKey : "");
        }, delayMs);
    }

    public void send(byte[] packet) {
        WebSocket active = socket;
        if (active == null || packet == null || packet.length == 0 || !connected) return;
        if (active.queueSize() > MAX_QUEUED_BYTES) return;
        active.send(ByteString.of(packet));
    }
    public boolean isHost() { return host; }
    public void close() { closedByUser = true; connected = false; ++connectionGeneration; reconnectAttempts = 0; handler.removeCallbacksAndMessages(null); WebSocket active = socket; socket = null; if (active != null) active.close(1000, "leaving"); }
    private void status(String value, String code, boolean connected) { activity.runOnUiThread(() -> listener.onStatus(value, code, connected)); }
    private static String trimSlash(String value) { while (value.endsWith("/")) value = value.substring(0, value.length() - 1); return value; }
}
