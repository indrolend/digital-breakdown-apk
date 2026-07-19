package com.indrolend.digitalbreakdown;

import android.app.Activity;
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
    private final Activity activity;
    private final Listener listener;
    private final OkHttpClient http = new OkHttpClient();
    private final String serviceUrl;
    private volatile WebSocket socket;
    private volatile boolean host;
    private volatile String roomCode = "";

    public MultiplayerClient(Activity activity, String serviceUrl, Listener listener) {
        this.activity = activity;
        this.serviceUrl = trimSlash(serviceUrl);
        this.listener = listener;
    }

    public void host() {
        close(); host = true; status("CREATING ROOM", "", false);
        Request request = new Request.Builder().url(serviceUrl + "/v1/rooms")
            .post(RequestBody.create("{\"gameplayVersion\":2}", JSON)).build();
        http.newCall(request).enqueue(new Callback() {
            @Override public void onFailure(Call call, java.io.IOException error) { status("ROOM CREATE FAILED", "", false); }
            @Override public void onResponse(Call call, Response response) {
                try (Response done = response) {
                    if (!done.isSuccessful() || done.body() == null) { status("ROOM CREATE FAILED", "", false); return; }
                    JSONObject json = new JSONObject(done.body().string());
                    roomCode = json.getString("code");
                    connect("host", json.getString("hostKey"));
                } catch (Exception error) { status("ROOM CREATE FAILED", "", false); }
            }
        });
    }

    public void join(String code) {
        close(); host = false; roomCode = code.trim().toUpperCase(Locale.US);
        if (roomCode.length() != 6) { status("ENTER 6-CHARACTER CODE", roomCode, false); return; }
        connect("guest", "");
    }

    private void connect(String role, String key) {
        status("CONNECTING " + roomCode, roomCode, false);
        String wsBase = serviceUrl.startsWith("https://") ? "wss://" + serviceUrl.substring(8)
            : "ws://" + serviceUrl.substring("http://".length());
        String url = wsBase + "/v1/rooms/" + roomCode + "/connect?role=" + role
            + "&build=pass7-native-android&gameplay=2" + (key.isEmpty() ? "" : "&key=" + key);
        socket = http.newWebSocket(new Request.Builder().url(url).build(), new WebSocketListener() {
            @Override public void onMessage(WebSocket webSocket, String text) {
                NativeBridge.onNetworkControl(text);
                try {
                    JSONObject json = new JSONObject(text);
                    if ("welcome".equals(json.optString("type"))) {
                        int playerId = json.getInt("playerId");
                        NativeBridge.configureNetwork(host, playerId, roomCode, host ? "ROOM " + roomCode : "JOINED " + roomCode);
                        status(host ? "ROOM " + roomCode : "JOINED " + roomCode, roomCode, true);
                    } else if ("match_closed".equals(json.optString("type"))) status("HOST LEFT", roomCode, false);
                } catch (Exception ignored) { }
            }
            @Override public void onMessage(WebSocket webSocket, ByteString bytes) { NativeBridge.onNetworkPacket(bytes.toByteArray()); }
            @Override public void onFailure(WebSocket webSocket, Throwable error, Response response) { status("CONNECTION FAILED", roomCode, false); }
            @Override public void onClosed(WebSocket webSocket, int code, String reason) { status("DISCONNECTED", roomCode, false); }
        });
    }

    public void send(byte[] packet) { WebSocket active = socket; if (active != null && packet != null) active.send(ByteString.of(packet)); }
    public void close() { WebSocket active = socket; socket = null; if (active != null) active.close(1000, "leaving"); }
    private void status(String value, String code, boolean connected) { activity.runOnUiThread(() -> listener.onStatus(value, code, connected)); }
    private static String trimSlash(String value) { while (value.endsWith("/")) value = value.substring(0, value.length() - 1); return value; }
}
