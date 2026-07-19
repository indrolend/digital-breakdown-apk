package com.indrolend.digitalbreakdown;

import android.content.Context;
import android.media.MediaPlayer;
import android.media.AudioAttributes;
import android.media.SoundPool;
import android.util.SparseArray;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public final class NativeBridge {
    private NativeBridge() {}
    private static Context audioContext;
    private static MediaPlayer slurpPlayer;
    private static MediaPlayer musicPlayer;
    private static MediaPlayer gameOverPlayer;
    private static SoundPool soundPool;
    private static final SparseArray<Integer> cueSamples = new SparseArray<>();
    private static final SparseArray<Boolean> loadedSamples = new SparseArray<>();
    private static final SparseArray<Float> pendingSamples = new SparseArray<>();
    private static boolean musicStarted;
    private static boolean gameOverStarted;
    private static MultiplayerClient multiplayerClient;

    public static void setMultiplayerClient(MultiplayerClient client) { multiplayerClient = client; }
    public static void sendNetworkPacket(byte[] packet) {
        MultiplayerClient client = multiplayerClient;
        if (client != null) client.send(packet);
    }

    public static void initializeAudio(Context context) {
        audioContext = context.getApplicationContext();
        AudioAttributes attributes = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build();
        soundPool = new SoundPool.Builder().setMaxStreams(16).setAudioAttributes(attributes).build();
        soundPool.setOnLoadCompleteListener((pool, sampleId, status) -> {
            if (status != 0) return;
            loadedSamples.put(sampleId, true);
            Float pending = pendingSamples.get(sampleId);
            if (pending != null) { pool.play(sampleId, pending, pending, 1, 0, 1.0f); pendingSamples.remove(sampleId); }
        });
        final int[] resources = cueResources();
        for (int cue = 0; cue < resources.length; ++cue) if (cue != 11 && cue != 12) cueSamples.put(cue, soundPool.load(audioContext, resources[cue], 1));
    }

    public static void initializeModels(Context context) {
        File dir = new File(context.getFilesDir(), "pass7-models");
        if (!dir.exists()) dir.mkdirs();
        copyModel(context, R.raw.phone, new File(dir, "phone.dbmesh"));
        copyModel(context, R.raw.flower, new File(dir, "flower.dbmesh"));
        copyModel(context, R.raw.human, new File(dir, "human.dbhuman"));
        setAssetRoot(dir.getAbsolutePath());
    }

    private static void copyModel(Context context, int resource, File output) {
        try (InputStream input = context.getResources().openRawResource(resource); FileOutputStream stream = new FileOutputStream(output)) {
            byte[] buffer = new byte[32768]; int count;
            while ((count = input.read(buffer)) > 0) stream.write(buffer, 0, count);
        } catch (Exception ignored) { }
    }

    public static synchronized void playAudioCue(int cue, float volume) {
        if (audioContext == null) return;
        if (cue == 12) {
            if (slurpPlayer != null) { slurpPlayer.stop(); slurpPlayer.release(); slurpPlayer = null; }
            return;
        }
        final int[] resources = cueResources();
        if (cue < 0 || cue >= resources.length) return;
        if (cue == 11) {
            if (slurpPlayer != null) { slurpPlayer.stop(); slurpPlayer.release(); }
            slurpPlayer = MediaPlayer.create(audioContext, resources[cue]);
            if (slurpPlayer != null) { slurpPlayer.setVolume(volume, volume); slurpPlayer.setLooping(true); slurpPlayer.start(); }
            return;
        }
        Integer sample = cueSamples.get(cue);
        if (soundPool != null && sample != null) {
            if (Boolean.TRUE.equals(loadedSamples.get(sample))) soundPool.play(sample, volume, volume, 1, 0, 1.0f);
            else pendingSamples.put(sample, volume);
        }
    }

    private static int[] cueResources() { return new int[] {
            R.raw.vc_ended,R.raw.vc_invitation,R.raw.connect_power,R.raw.low_power,R.raw.negative_ack,
            R.raw.received_message,R.raw.sent_message,R.raw.phone_attack,R.raw.payment_success,R.raw.payment_failure,
            R.raw.end_call_tone,R.raw.slurp_ringtone,R.raw.slurp_ringtone,R.raw.capture_1,R.raw.capture_2,
            R.raw.capture_3,R.raw.capture_4,R.raw.capture_5
        }; }

    public static synchronized void syncMusic(boolean started, boolean dead) {
        if (audioContext == null) return;
        if (started && !dead) {
            stopGameOver();
            if (!musicStarted) { musicStarted = true; musicPlayer = createNamedPlayer("game_music", true, 0.52f); }
        } else if (musicStarted) { if (musicPlayer != null) { musicPlayer.stop(); musicPlayer.release(); musicPlayer = null; } musicStarted = false; }
        if (dead && !gameOverStarted) { gameOverStarted = true; gameOverPlayer = createNamedPlayer("game_over", false, 0.62f); }
        else if (!dead) stopGameOver();
    }
    private static MediaPlayer createNamedPlayer(String name, boolean loop, float volume) {
        int resource = audioContext.getResources().getIdentifier(name, "raw", audioContext.getPackageName());
        if (resource == 0) return null;
        MediaPlayer player = MediaPlayer.create(audioContext, resource);
        if (player != null) { player.setLooping(loop); player.setVolume(volume, volume); player.start(); }
        return player;
    }
    private static void stopGameOver() {
        if (gameOverPlayer != null) { gameOverPlayer.stop(); gameOverPlayer.release(); gameOverPlayer = null; }
        gameOverStarted = false;
    }

    public static native void onSurfaceCreated();
    public static native void setAssetRoot(String path);
    public static native void onSurfaceChanged(int width, int height);
    public static native void onDrawFrame();
    public static native void onTouch(int action, float x, float y, int pointerCount);
    public static native void onTouchControls(
        float moveX,
        float moveZ,
        float lookDeltaX,
        float lookDeltaY,
        boolean vacuumHeld,
        boolean sprintHeld,
        boolean jumpPressed,
        boolean meleePressed,
        boolean shootPressed,
        boolean cameraTogglePressed
    );
    public static native void onKey(int keyCode, boolean down);
    public static native boolean isIntroActive();
    public static native void startSolo();
    public static native void configureNetwork(boolean host, int playerId, String roomCode, String status);
    public static native void onNetworkControl(String json);
    public static native void onNetworkPacket(byte[] packet);
}
