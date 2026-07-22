package com.indrolend.digitalbreakdown;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.MediaPlayer;
import android.media.PlaybackParams;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.media.SoundPool;
import android.media.audiofx.Equalizer;
import android.util.SparseArray;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public final class NativeBridge {
    private NativeBridge() {}
    private static Context audioContext;
    private static MediaPlayer slurpPlayer;
    private static MediaPlayer musicPlayer;
    private static MediaPlayer menuMusicPlayer;
    private static MediaPlayer tvRoomPlayer;
    private static MediaPlayer gameOverPlayer;
    private static SoundPool soundPool;
    private static final SparseArray<Integer> cueSamples = new SparseArray<>();
    private static final SparseArray<Boolean> loadedSamples = new SparseArray<>();
    private static final SparseArray<Float> pendingSamples = new SparseArray<>();
    private static boolean musicStarted;
    private static boolean menuMusicStarted;
    private static boolean gameOverStarted;
    private static Equalizer musicEqualizer;
    private static float menuFilterAmount;
    private static float menuCuePulse;
    private static float menuCueBend;
    private static float tvRoomMix;
    private static long tvRoomPitchUpdateNs;
    private static float rewardDuck;
    private static float localMusicLevel = 0.70f;
    private static float localSfxLevel = 0.55f;
    private static final AudioTrack[] menuVoices = new AudioTrack[2];
    private static int menuVariation;
    private static MultiplayerClient multiplayerClient;
    private static SharedPreferences progressionPreferences;

    public static synchronized void initializeProgression(Context context) {
        progressionPreferences=context.getApplicationContext().getSharedPreferences("digital_breakdown_progression",Context.MODE_PRIVATE);
        setPersistentProgression(progressionPreferences.getLong("tokens",0),progressionPreferences.getInt("shot",0),progressionPreferences.getInt("lunge",0),progressionPreferences.getInt("attack",0));
    }

    public static synchronized void saveProgression(long revision,long tokens,int shot,int lunge,int attack) {
        if(progressionPreferences==null)return;
        progressionPreferences.edit().putLong("revision",revision).putLong("tokens",Math.max(0,tokens)).putInt("shot",shot).putInt("lunge",lunge).putInt("attack",attack).apply();
    }

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
        for(int i=0;i<2;++i){short[] pcm=makeMenuPluck(i==0?220.0f:293.66f);try{AudioTrack track=new AudioTrack.Builder().setAudioAttributes(attributes).setAudioFormat(new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(22050).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build()).setBufferSizeInBytes(pcm.length*2).setTransferMode(AudioTrack.MODE_STATIC).build();track.write(pcm,0,pcm.length);menuVoices[i]=track;}catch(Exception ignored){menuVoices[i]=null;}}
        final int[] resources = cueResources();
        for (int cue = 0; cue < resources.length; ++cue) if (cue != 11 && cue != 12) cueSamples.put(cue, soundPool.load(audioContext, resources[cue], 1));
    }

    public static void initializeModels(Context context) {
        File dir = new File(context.getFilesDir(), "pass7-models");
        if (!dir.exists()) dir.mkdirs();
        copyModel(context, R.raw.phone, new File(dir, "phone.dbmesh"));
        copyModel(context, R.raw.flower, new File(dir, "flower.dbmesh"));
        copyModel(context, R.raw.human, new File(dir, "human.dbhuman"));
        File tvDir = new File(dir, "tv-gifs");
        if (!tvDir.exists()) tvDir.mkdirs();
        for (int index = 1; index <= 15; ++index) {
            String name = String.format(java.util.Locale.US, "tv%02d.dbgif", index);
            copyAsset(context, name, new File(tvDir, name));
        }
        setAssetRoot(dir.getAbsolutePath());
    }

    private static void copyAsset(Context context, String asset, File output) {
        try (InputStream input = context.getAssets().open(asset); FileOutputStream stream = new FileOutputStream(output)) {
            byte[] buffer = new byte[32768]; int count;
            while ((count = input.read(buffer)) > 0) stream.write(buffer, 0, count);
        } catch (Exception ignored) { }
    }

    private static void copyModel(Context context, int resource, File output) {
        try (InputStream input = context.getResources().openRawResource(resource); FileOutputStream stream = new FileOutputStream(output)) {
            byte[] buffer = new byte[32768]; int count;
            while ((count = input.read(buffer)) > 0) stream.write(buffer, 0, count);
        } catch (Exception ignored) { }
    }

    public static synchronized void playAudioCue(int cue, float volume) {
        if (audioContext == null) return;
        if(cue==20)rewardDuck=Math.max(rewardDuck,0.18f);else if(cue==21)rewardDuck=Math.max(rewardDuck,0.09f);
        volume *= localSfxLevel;
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
            R.raw.capture_3,R.raw.capture_4,R.raw.capture_5,R.raw.headshot,R.raw.headshot_critical,
            R.raw.reward_woah,R.raw.reward_nice
        }; }

    public static synchronized void syncMusic(boolean started, boolean dead, boolean menuFiltered, boolean inTvRoom, float headshotCrush, float phoneProximity) {
        if (audioContext == null) return;
        if (!started && !dead) {
            if (!menuMusicStarted) { menuMusicStarted = true; menuMusicPlayer = createNamedPlayer("menu_music", true, 0.0f); }
            if (menuMusicPlayer != null) {
                menuCuePulse = Math.max(0.0f, menuCuePulse - 0.018f);
                menuCueBend *= 0.92f;
                float t = System.nanoTime() * 0.000000001f;
                float breath = 0.5f + 0.5f * (float)Math.sin(t * 0.42f);
                float volume = (0.34f + breath * 0.045f + menuCuePulse * 0.075f) * localMusicLevel;
                menuMusicPlayer.setVolume(volume, volume);
                try { menuMusicPlayer.setPlaybackParams(new PlaybackParams().allowDefaults().setSpeed(1.0f).setPitch(1.0f + (float)Math.sin(t * 0.17f) * 0.0025f + menuCueBend + menuCuePulse * 0.0035f)); } catch(Exception ignored) {}
            }
        } else stopMenuMusic();
        if (started && !dead) {
            stopGameOver();
            if (!musicStarted) { musicStarted = true; musicPlayer = createNamedPlayer("game_music", true, 0.52f); tvRoomPlayer=createNamedPlayer("tv_room_pad",true,0.0f);initializeMusicEqualizer(); }
            updateMusicFilter(menuFiltered, headshotCrush);
            rewardDuck=Math.max(0.0f,rewardDuck-0.006f);float duck=1.0f-rewardDuck;tvRoomMix+=((inTvRoom?1.0f:0.0f)-tvRoomMix)*0.035f;if(tvRoomPlayer!=null){float pad=0.48f*localMusicLevel*tvRoomMix*duck;tvRoomPlayer.setVolume(pad,pad);long now=System.nanoTime();if(now-tvRoomPitchUpdateNs>50000000L){tvRoomPitchUpdateNs=now;float t=now*0.000000001f,p=Math.max(0.0f,Math.min(1.0f,phoneProximity)),pitch=1.0f+p*(-0.012f+(float)Math.sin(t*2.7f)*0.018f+(float)Math.sin(t*0.61f)*0.010f);try{tvRoomPlayer.setPlaybackParams(new PlaybackParams().allowDefaults().setSpeed(1.0f).setPitch(pitch));}catch(Exception ignored){}}}if(musicPlayer!=null){float base=(0.52f-Math.max(0.0f,Math.min(1.0f,headshotCrush))*0.018f)*localMusicLevel*(1.0f-tvRoomMix)*duck;musicPlayer.setVolume(base,base);}
        } else if (musicStarted) { releaseMusicEqualizer(); if (musicPlayer != null) { musicPlayer.stop(); musicPlayer.release(); musicPlayer = null; }if(tvRoomPlayer!=null){tvRoomPlayer.stop();tvRoomPlayer.release();tvRoomPlayer=null;}tvRoomMix=0.0f;musicStarted = false; }
        if (dead && !gameOverStarted) { gameOverStarted = true; gameOverPlayer = createNamedPlayer("game_over", false, 0.62f * localMusicLevel); }
        else if (!dead) stopGameOver();
    }

    private static short[] makeMenuPluck(float frequency){final int rate=22050,frames=2867,reflection=640,bottomOut=154;short[] result=new short[frames];float[] work=new float[frames];int noise=0x53A91;for(int i=0;i<frames;++i){float t=(float)i/rate,phase=t*frequency;noise=noise*1664525+1013904223;float grain=((noise>>>24)&255)/127.5f-1.0f;float contact=grain*(float)Math.exp(-t*125.0f),body=((float)Math.sin(phase*6.2831853f)*0.62f+(float)Math.sin(phase*12.5663706f)*0.16f)*(float)Math.exp(-t*34.0f);float dry=contact*0.24f+body;if(i>=bottomOut){float bt=(float)(i-bottomOut)/rate;dry+=work[i-bottomOut]*0.18f*(float)Math.exp(-bt*70.0f);}dry=Math.round(Math.tanh(dry*1.18f)*48.0f)/48.0f;work[i]=dry*0.18f+(i>=reflection?work[i-reflection]*0.13f:0.0f);result[i]=(short)Math.max(-32767,Math.min(32767,Math.round(work[i]*32767.0f)));}return result;}

    public static synchronized void playMenuCue(boolean confirm){if(localSfxLevel<=0)return;menuCuePulse=Math.max(menuCuePulse,confirm?0.34f:0.22f);menuCueBend=confirm?0.010f:-0.006f;final float[] ratios={0.94f,1.0f,1.035f,0.975f,1.07f,0.92f,1.015f};int variation=menuVariation++;for(AudioTrack track:menuVoices)if(track!=null)try{track.pause();track.flush();track.stop();}catch(Exception ignored){}AudioTrack voice=menuVoices[confirm?1:0];if(voice==null)return;try{voice.setPlaybackHeadPosition(0);voice.setPlaybackRate(Math.round(22050*ratios[(variation+(confirm?2:0))%ratios.length]));voice.setVolume(localSfxLevel*(confirm?0.76f:0.54f)*(0.96f+(variation%3)*0.018f));voice.play();}catch(Exception ignored){}}
    private static void initializeMusicEqualizer() { try { if(musicPlayer==null)return;musicEqualizer=new Equalizer(0,musicPlayer.getAudioSessionId());musicEqualizer.setEnabled(true); } catch(Exception ignored){musicEqualizer=null;} }
    private static void updateMusicFilter(boolean filtered, float headshotCrush) { menuFilterAmount+=((filtered?1.0f:0.0f)-menuFilterAmount)*0.085f;float crush=Math.max(0.0f,Math.min(1.0f,headshotCrush));float step=(System.nanoTime()/16000000L)%3L==0L?1.0f:0.0f;if(musicPlayer!=null){float volume=(0.52f-crush*(0.010f+step*0.018f))*localMusicLevel;musicPlayer.setVolume(volume,volume);}if(musicEqualizer==null)return;try{short bands=musicEqualizer.getNumberOfBands();short minimum=musicEqualizer.getBandLevelRange()[0];for(short band=0;band<bands;++band){float high=bands<=1?1.0f:(float)band/(float)(bands-1);float shaped=high*high;float attenuation=menuFilterAmount*shaped+crush*step*0.08f*high;musicEqualizer.setBandLevel(band,(short)(minimum*Math.min(1.0f,attenuation)));}}catch(Exception ignored){} }
    public static synchronized void applyLocalSettings(float music,float sfx,boolean musicMuted,boolean sfxMuted,int preset,boolean shadows,boolean portal,boolean particles,boolean fps){localMusicLevel=musicMuted?0.0f:Math.max(0.0f,Math.min(1.0f,music));localSfxLevel=sfxMuted?0.0f:Math.max(0.0f,Math.min(1.0f,sfx));setLocalSettings(music,sfx,musicMuted,sfxMuted,preset,shadows,portal,particles,fps);}
    private static void releaseMusicEqualizer(){if(musicEqualizer!=null){try{musicEqualizer.setEnabled(false);musicEqualizer.release();}catch(Exception ignored){}musicEqualizer=null;}menuFilterAmount=0.0f;}
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
    private static void stopMenuMusic() {
        if (menuMusicPlayer != null) { menuMusicPlayer.stop(); menuMusicPlayer.release(); menuMusicPlayer = null; }
        menuMusicStarted = false;
        menuCuePulse = 0.0f;
        menuCueBend = 0.0f;
    }

    public static native void onSurfaceCreated();
    public static native void setPersistentProgression(long tokens, int shot, int lunge, int attack);
    public static native void setAssetRoot(String path);
    public static native void onSurfaceChanged(int width, int height);
    public static native void onDrawFrame();
    public static native void onTouch(int action, float x, float y, int pointerCount);
    public static native void onWiggle(float axis);
    public static native void onCommSignal(int signal);
    public static native boolean isGrabbed();
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
    public static native void restart();
    public static native boolean isIntroActive();
    public static native boolean isStarted();
    public static native int getMenuMode();
    public static native void setPaused(boolean paused);
    public static native void chooseUpgrade(int track, boolean permanent);
    private static native void setLocalSettings(float music,float sfx,boolean musicMuted,boolean sfxMuted,int preset,boolean shadows,boolean portal,boolean particles,boolean fps);
    public static native void startSolo();
    public static native void configureNetwork(boolean host, int playerId, String roomCode, String status);
    public static native void onNetworkControl(String json);
    public static native void onNetworkPacket(byte[] packet);
}
