package com.indrolend.digitalbreakdown;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.os.Bundle;
import android.content.SharedPreferences;
import android.view.Surface;
import android.graphics.Color;
import android.text.InputFilter;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.SeekBar;
import android.widget.ScrollView;
import android.graphics.drawable.GradientDrawable;
import java.net.URLEncoder;
import java.net.URL;
import java.util.Locale;

public final class MainActivity extends Activity {
    private GameView gameView;
    private MultiplayerClient multiplayer;
    private LinearLayout menu;
    private ScrollView menuScroller;
    private TextView networkStatus;
    private EditText roomCode;
    private float musicVolume=0.70f,sfxVolume=0.55f;
    private boolean musicMuted=false,sfxMuted=false,shadows=false,portal=false,particles=false,fps=false;
    private int graphicsPreset=0;
    private float touchLook=1.0f,controllerLook=1.0f;
    private int menuContentWidth;
    private SharedPreferences settingsPrefs;
    private boolean pauseMenuOpen=false;

    static {
        System.loadLibrary("digitalbreakdown");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        enterImmersiveFullscreen();

        gameView = new GameView(this);
        ControlOverlayView controls = new ControlOverlayView(this, gameView);
        multiplayer = new MultiplayerClient(this, BuildConfig.MULTIPLAYER_URL, this::onNetworkStatus);
        settingsPrefs=getSharedPreferences("digital_breakdown_local_settings",MODE_PRIVATE);
        loadLocalSettings();
        gameView.setLookSensitivity(touchLook,controllerLook);
        NativeBridge.setMultiplayerClient(multiplayer);
        FrameLayout root = new FrameLayout(this);
        root.addView(gameView, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));
        root.addView(controls, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));
        menu = new LinearLayout(this);
        final int screenWidth=getResources().getDisplayMetrics().widthPixels,screenHeight=getResources().getDisplayMetrics().heightPixels;
        final int panelWidth=Math.max(dp(260),Math.min(dp(520),screenWidth-dp(24)));
        final int panelHeight=Math.max(dp(220),Math.min(dp(680),screenHeight-dp(24)));
        menuContentWidth=Math.max(dp(220),Math.min(dp(420),panelWidth-dp(32)));
        menu.setOrientation(LinearLayout.VERTICAL); menu.setGravity(Gravity.CENTER); menu.setPadding(dp(16),dp(12),dp(16),dp(12));
        menu.setBackgroundColor(0x30000000);
        showMainMenu();
        menuScroller=new ScrollView(this);menuScroller.setFillViewport(true);menuScroller.setClipToPadding(false);menuScroller.addView(menu,new ScrollView.LayoutParams(ScrollView.LayoutParams.MATCH_PARENT,ScrollView.LayoutParams.WRAP_CONTENT));
        FrameLayout.LayoutParams panel = new FrameLayout.LayoutParams(panelWidth,panelHeight,Gravity.CENTER);root.addView(menuScroller,panel);
        setContentView(root);

        gameView.post(() -> requestSixtyHertz(gameView.getHolder().getSurface()));
        handleJoinIntent(getIntent());
    }

    private static void requestSixtyHertz(Surface surface) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && surface != null && surface.isValid()) {
            surface.setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) enterImmersiveFullscreen();
    }

    private void enterImmersiveFullscreen() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    @Override
    protected void onPause() {
        if (gameView != null) gameView.pauseGameLoop();
        super.onPause();
    }

    @Override protected void onDestroy() { if (multiplayer != null) multiplayer.close(); super.onDestroy(); }

    @Override protected void onNewIntent(Intent intent) { super.onNewIntent(intent); setIntent(intent); handleJoinIntent(intent); }

    private void onNetworkStatus(String status, String code, boolean connected) {
        if(networkStatus!=null)networkStatus.setText(code.isEmpty() ? status : code);
        if (connected) {
            if (multiplayer != null && multiplayer.isHost()) showRoomShare(code);
            else menuScroller.setVisibility(android.view.View.GONE);
        }
    }
    private void page(String title){menu.removeAllViews();if(title!=null&&!title.isEmpty())menu.addView(label(title,26));}
    private void showMainMenu(){pauseMenuOpen=false;page("READY");Button solo=button("SOLO");solo.setOnClickListener(v->{multiplayer.close();NativeBridge.startSolo();menuScroller.setVisibility(View.GONE);});menu.addView(solo);Button online=button("ONLINE");online.setOnClickListener(v->showOnline());menu.addView(online);Button settings=button("SETTINGS");settings.setOnClickListener(v->showSettings());menu.addView(settings);}
    void showPauseMenu(){pauseMenuOpen=true;NativeBridge.setPaused(true);page("PAUSED");Button resume=button("RESUME");resume.setOnClickListener(v->{NativeBridge.setPaused(false);pauseMenuOpen=false;menuScroller.setVisibility(View.GONE);});menu.addView(resume);Button controls=button("CONTROLS");controls.setOnClickListener(v->showControls());menu.addView(controls);Button audio=button("AUDIO");audio.setOnClickListener(v->showAudio());menu.addView(audio);Button graphics=button("GRAPHICS");graphics.setOnClickListener(v->showGraphics());menu.addView(graphics);menuScroller.setVisibility(View.VISIBLE);}
    void resumeFromPauseMenu(){NativeBridge.setPaused(false);pauseMenuOpen=false;menuScroller.setVisibility(View.GONE);}
    private void showOnline(){page("ONLINE");networkStatus=label("",16);menu.addView(networkStatus);Button host=button("HOST");host.setOnClickListener(v->{networkStatus.setText("CREATING");multiplayer.host();});menu.addView(host);Button join=button("JOIN");join.setOnClickListener(v->showJoinCode());menu.addView(join);backButton(this::showMainMenu);}
    private void showJoinCode(){page("JOIN");roomCode=new EditText(this);roomCode.setTextColor(Color.WHITE);roomCode.setTextSize(24);roomCode.setSingleLine(true);roomCode.setAllCaps(false);roomCode.setGravity(Gravity.CENTER);roomCode.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS);roomCode.setLetterSpacing(0.08f);roomCode.setFilters(new InputFilter[]{new InputFilter.LengthFilter(96)});GradientDrawable field=new GradientDrawable();field.setColor(0x180ACDEB);field.setStroke(dp(1),0x888FF7FF);roomCode.setBackground(field);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(menuContentWidth,dp(64));lp.setMargins(0,dp(16),0,dp(12));menu.addView(roomCode,lp);networkStatus=label("",14);menu.addView(networkStatus);roomCode.addTextChangedListener(new TextWatcher(){public void beforeTextChanged(CharSequence s,int start,int count,int after){}public void onTextChanged(CharSequence s,int start,int before,int count){String code=extractRoomCode(s==null?"":s.toString());if(code!=null){networkStatus.setText("CONNECTING");multiplayer.join(code);}}public void afterTextChanged(Editable e){}});backButton(this::showOnline);roomCode.requestFocus();roomCode.postDelayed(()->((android.view.inputmethod.InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).showSoftInput(roomCode,android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT),120);}
    private void showRoomShare(String code){if(code==null||code.isEmpty()){menuScroller.setVisibility(View.GONE);return;}page("ROOM");TextView codeView=label(code.toUpperCase(Locale.US),34);codeView.setLetterSpacing(0.18f);menu.addView(codeView);ImageView qr=new ImageView(this);LinearLayout.LayoutParams qrLp=new LinearLayout.LayoutParams(dp(156),dp(156));qrLp.setMargins(0,dp(6),0,dp(8));qr.setLayoutParams(qrLp);qr.setBackgroundColor(Color.WHITE);menu.addView(qr);loadQr(qr,roomUrl(code));Button copy=button("COPY");copy.setOnClickListener(v->{copyRoom(code);NativeBridge.playMenuCue(true);});menu.addView(copy);Button share=button("SHARE");share.setOnClickListener(v->shareRoom(code));menu.addView(share);Button play=button("PLAY");play.setOnClickListener(v->menuScroller.setVisibility(View.GONE));menu.addView(play);menuScroller.setVisibility(View.VISIBLE);}
    private void showSettings(){page("SETTINGS");Button controls=button("CONTROLS");controls.setOnClickListener(v->showControls());menu.addView(controls);Button audio=button("AUDIO");audio.setOnClickListener(v->showAudio());menu.addView(audio);Button graphics=button("GRAPHICS");graphics.setOnClickListener(v->showGraphics());menu.addView(graphics);backButton(this::showMainMenu);}
    private void showControls(){page("CONTROLS");menu.addView(label("TOUCH LOOK",13));SeekBar touch=controlSlider(touchLook);touch.setOnSeekBarChangeListener(listener(v->{touchLook=0.5f+v*1.25f;gameView.setLookSensitivity(touchLook,controllerLook);saveLocalSettings();}));menu.addView(touch);menu.addView(label("CONTROLLER LOOK",13));SeekBar controller=controlSlider(controllerLook);controller.setOnSeekBarChangeListener(listener(v->{controllerLook=0.5f+v*1.25f;gameView.setLookSensitivity(touchLook,controllerLook);saveLocalSettings();}));menu.addView(controller);Button defaults=button("DEFAULTS");defaults.setOnClickListener(v->{touchLook=controllerLook=1.0f;gameView.setLookSensitivity(1,1);saveLocalSettings();showControls();});menu.addView(defaults);backButton(pauseMenuOpen?this::showPauseMenu:this::showSettings);}
    private void showAudio(){page("AUDIO");menu.addView(label("MUSIC",15));SeekBar music=slider(musicVolume);music.setOnSeekBarChangeListener(listener(v->{musicVolume=v;applySettings();}));menu.addView(music);Button mm=button(musicMuted?"MUSIC ON":"MUSIC MUTE");mm.setOnClickListener(v->{musicMuted=!musicMuted;applySettings();showAudio();});menu.addView(mm);menu.addView(label("SFX",15));SeekBar sfx=slider(sfxVolume);sfx.setOnSeekBarChangeListener(listener(v->{sfxVolume=v;applySettings();}));menu.addView(sfx);Button sm=button(sfxMuted?"SFX ON":"SFX MUTE");sm.setOnClickListener(v->{sfxMuted=!sfxMuted;applySettings();showAudio();});menu.addView(sm);backButton(pauseMenuOpen?this::showPauseMenu:this::showSettings);}
    private void showGraphics(){page("GRAPHICS");String[] names={"LEGACY","NORMAL","PRETTY"};Button preset=button(names[graphicsPreset]);preset.setOnClickListener(v->{graphicsPreset=(graphicsPreset+1)%3;if(graphicsPreset==0){shadows=false;portal=false;particles=false;}else{shadows=true;portal=true;particles=true;}applySettings();showGraphics();});menu.addView(preset);Button shadow=button(shadows?"SHADOWS ON":"SHADOWS OFF");shadow.setOnClickListener(v->{shadows=!shadows;applySettings();showGraphics();});menu.addView(shadow);Button particle=button(particles?"PARTICLES ON":"PARTICLES OFF");particle.setOnClickListener(v->{particles=!particles;applySettings();showGraphics();});menu.addView(particle);Button fpsButton=button(fps?"FPS ON":"FPS OFF");fpsButton.setOnClickListener(v->{fps=!fps;applySettings();showGraphics();});menu.addView(fpsButton);backButton(pauseMenuOpen?this::showPauseMenu:this::showSettings);}
    private void applySettings(){NativeBridge.applyLocalSettings(musicVolume,sfxVolume,musicMuted,sfxMuted,graphicsPreset,shadows,portal,particles,fps);saveLocalSettings();}
    private void loadLocalSettings(){if(settingsPrefs==null)return;musicVolume=settingsPrefs.getFloat("music",musicVolume);sfxVolume=settingsPrefs.getFloat("sfx",sfxVolume);musicMuted=settingsPrefs.getBoolean("musicMuted",musicMuted);sfxMuted=settingsPrefs.getBoolean("sfxMuted",sfxMuted);graphicsPreset=settingsPrefs.getInt("graphicsPreset",graphicsPreset);shadows=settingsPrefs.getBoolean("shadows",shadows);portal=settingsPrefs.getBoolean("portal",portal);particles=settingsPrefs.getBoolean("particles",particles);fps=settingsPrefs.getBoolean("fps",fps);touchLook=settingsPrefs.getFloat("touchLook",touchLook);controllerLook=settingsPrefs.getFloat("controllerLook",controllerLook);if(!settingsPrefs.getBoolean("androidPerfDefaultsV3",false)){graphicsPreset=0;shadows=false;portal=false;particles=false;settingsPrefs.edit().putBoolean("androidPerfDefaultsV2",true).putBoolean("androidPerfDefaultsV3",true).apply();}applySettings();}
    private void saveLocalSettings(){if(settingsPrefs==null)return;settingsPrefs.edit().putFloat("music",musicVolume).putFloat("sfx",sfxVolume).putBoolean("musicMuted",musicMuted).putBoolean("sfxMuted",sfxMuted).putInt("graphicsPreset",graphicsPreset).putBoolean("shadows",shadows).putBoolean("portal",portal).putBoolean("particles",particles).putBoolean("fps",fps).putFloat("touchLook",touchLook).putFloat("controllerLook",controllerLook).apply();}
    private SeekBar slider(float value){SeekBar bar=new SeekBar(this);bar.setMax(100);bar.setProgress(Math.round(value*100));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(menuContentWidth,-2);lp.gravity=Gravity.CENTER_HORIZONTAL;bar.setLayoutParams(lp);return bar;}
    private SeekBar controlSlider(float value){return slider((value-0.5f)/1.25f);}
    private SeekBar.OnSeekBarChangeListener listener(java.util.function.Consumer<Float> update){return new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar bar,int value,boolean user){if(user)update.accept(value/100.0f);}public void onStartTrackingTouch(SeekBar bar){}public void onStopTrackingTouch(SeekBar bar){}};}
    private void backButton(Runnable action){Button back=button("BACK");back.setOnClickListener(v->action.run());menu.addView(back);}
    private TextView label(String text, int size) { TextView view = new TextView(this); view.setText(text); view.setTextColor(0xFFE8FCFF); view.setTextSize(size); view.setGravity(Gravity.CENTER); view.setPadding(dp(6),dp(7),dp(6),dp(7));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(menuContentWidth,-2);lp.gravity=Gravity.CENTER_HORIZONTAL;view.setLayoutParams(lp); return view; }
    private Button button(String text) { Button view=new Button(this);view.setText(text);view.setTextColor(Color.WHITE);view.setTextSize(18);view.setAllCaps(false);view.setGravity(Gravity.CENTER);view.setMinWidth(0);view.setMinimumWidth(0);view.setMinHeight(dp(48));GradientDrawable bg=new GradientDrawable();bg.setColor(0x180ACDEB);bg.setStroke(dp(1),0x888FF7FF);bg.setCornerRadius(0);view.setBackground(bg);view.setOnTouchListener((v,e)->{if(e.getActionMasked()==android.view.MotionEvent.ACTION_DOWN){NativeBridge.playMenuCue(true);v.animate().scaleX(0.992f).scaleY(0.992f).alpha(0.86f).setDuration(110).start();}else if(e.getActionMasked()==android.view.MotionEvent.ACTION_UP||e.getActionMasked()==android.view.MotionEvent.ACTION_CANCEL)v.animate().scaleX(1).scaleY(1).alpha(1).setDuration(220).start();return false;});view.setOnFocusChangeListener((v,focused)->{if(focused&&v.hasWindowFocus())NativeBridge.playMenuCue(false);v.animate().scaleX(focused?1.008f:1).scaleY(focused?1.008f:1).setDuration(260).start();});LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(menuContentWidth,dp(48));lp.gravity=Gravity.CENTER_HORIZONTAL;lp.setMargins(0,dp(4),0,dp(4));view.setLayoutParams(lp);return view; }
    private int dp(float value){return Math.max(1,Math.round(value*getResources().getDisplayMetrics().density));}

    private void handleJoinIntent(Intent intent){String code=extractRoomCode(intent==null||intent.getData()==null?null:intent.getData().toString());if(code!=null){menuScroller.setVisibility(View.VISIBLE);showOnline();networkStatus.setText("CONNECTING");multiplayer.join(code);}}
    private static String extractRoomCode(String value){if(value==null)return null;String compact=value.toUpperCase(Locale.US).replaceAll("[^A-Z0-9]","");for(int i=compact.length()-6;i>=0;--i){String code=compact.substring(i,i+6);if(code.matches("[A-Z0-9]{6}"))return code;}return null;}
    private static String roomUrl(String code){return "digitalbreakdown://join/"+code.toUpperCase(Locale.US);}
    private String roomShareText(String code){return "Digital Breakdown room "+code.toUpperCase(Locale.US)+"\n"+roomUrl(code);}
    private void copyRoom(String code){ClipboardManager clipboard=(ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);if(clipboard!=null)clipboard.setPrimaryClip(ClipData.newPlainText("Digital Breakdown room",roomShareText(code)));}
    private void shareRoom(String code){Intent share=new Intent(Intent.ACTION_SEND);share.setType("text/plain");share.putExtra(Intent.EXTRA_TEXT,roomShareText(code));startActivity(Intent.createChooser(share,"Share room"));}
    private void loadQr(ImageView image,String data){new Thread(()->{try{String encoded=URLEncoder.encode(data,"UTF-8");URL url=new URL("https://api.qrserver.com/v1/create-qr-code/?size=220x220&margin=8&data="+encoded);final android.graphics.Bitmap bitmap=BitmapFactory.decodeStream(url.openStream());if(bitmap!=null)runOnUiThread(()->image.setImageBitmap(bitmap));}catch(Exception ignored){}}).start();}

    @Override
    protected void onResume() {
        super.onResume();
        if (gameView != null) gameView.resumeGameLoop();
    }
}
