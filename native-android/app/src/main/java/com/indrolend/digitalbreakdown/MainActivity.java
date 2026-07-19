package com.indrolend.digitalbreakdown;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
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
import android.widget.TextView;
import android.widget.SeekBar;
import android.graphics.drawable.GradientDrawable;

public final class MainActivity extends Activity {
    private GameView gameView;
    private MultiplayerClient multiplayer;
    private LinearLayout menu;
    private TextView networkStatus;
    private EditText roomCode;
    private float musicVolume=0.70f,sfxVolume=0.55f;
    private boolean musicMuted=false,sfxMuted=false,shadows=true,portal=true,particles=true,fps=false;
    private int graphicsPreset=1;
    private float touchLook=1.0f,controllerLook=1.0f;

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

        gameView = new GameView(this);
        ControlOverlayView controls = new ControlOverlayView(this, gameView);
        multiplayer = new MultiplayerClient(this, BuildConfig.MULTIPLAYER_URL, this::onNetworkStatus);
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
        menu.setOrientation(LinearLayout.VERTICAL); menu.setGravity(Gravity.CENTER); menu.setPadding(48, 32, 48, 32);
        menu.setBackgroundColor(0x30000000);
        showMainMenu();
        FrameLayout.LayoutParams panel = new FrameLayout.LayoutParams(620, -2, Gravity.CENTER); root.addView(menu, panel);
        setContentView(root);

        gameView.post(() -> requestSixtyHertz(gameView.getHolder().getSurface()));
    }

    private static void requestSixtyHertz(Surface surface) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && surface != null && surface.isValid()) {
            surface.setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
        }
    }

    @Override
    protected void onPause() {
        if (gameView != null) gameView.pauseGameLoop();
        super.onPause();
    }

    @Override protected void onDestroy() { if (multiplayer != null) multiplayer.close(); super.onDestroy(); }

    private void onNetworkStatus(String status, String code, boolean connected) {
        if(networkStatus!=null)networkStatus.setText(code.isEmpty() ? status : code);
        if (connected) menu.setVisibility(android.view.View.GONE);
    }
    private void page(String title){menu.removeAllViews();menu.addView(label(title,26));}
    private void showMainMenu(){page("READY");Button solo=button("SOLO");solo.setOnClickListener(v->{multiplayer.close();NativeBridge.startSolo();menu.setVisibility(View.GONE);});menu.addView(solo);Button online=button("ONLINE");online.setOnClickListener(v->showOnline());menu.addView(online);Button settings=button("SETTINGS");settings.setOnClickListener(v->showSettings());menu.addView(settings);}
    private void showOnline(){page("ONLINE");networkStatus=label("",16);menu.addView(networkStatus);Button host=button("HOST");host.setOnClickListener(v->{networkStatus.setText("CREATING");multiplayer.host();});menu.addView(host);Button join=button("JOIN");join.setOnClickListener(v->showJoinCode());menu.addView(join);backButton(this::showMainMenu);}
    private void showJoinCode(){page("ENTER CODE");roomCode=new EditText(this);roomCode.setTextColor(Color.WHITE);roomCode.setTextSize(30);roomCode.setSingleLine(true);roomCode.setAllCaps(true);roomCode.setGravity(Gravity.CENTER);roomCode.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS);roomCode.setLetterSpacing(0.18f);roomCode.setFilters(new InputFilter[]{new InputFilter.LengthFilter(6)});GradientDrawable field=new GradientDrawable();field.setColor(0x180ACDEB);field.setStroke(1,0x888FF7FF);roomCode.setBackground(field);LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(420,82);lp.setMargins(0,22,0,18);menu.addView(roomCode,lp);networkStatus=label("",14);menu.addView(networkStatus);roomCode.addTextChangedListener(new TextWatcher(){public void beforeTextChanged(CharSequence s,int start,int count,int after){}public void onTextChanged(CharSequence s,int start,int before,int count){if(s.length()==6){networkStatus.setText("CONNECTING");multiplayer.join(s.toString());}}public void afterTextChanged(Editable e){}});backButton(this::showOnline);roomCode.requestFocus();roomCode.postDelayed(()->((android.view.inputmethod.InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).showSoftInput(roomCode,android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT),120);}
    private void showSettings(){page("SETTINGS");Button controls=button("CONTROLS");controls.setOnClickListener(v->showControls());menu.addView(controls);Button audio=button("AUDIO");audio.setOnClickListener(v->showAudio());menu.addView(audio);Button graphics=button("GRAPHICS");graphics.setOnClickListener(v->showGraphics());menu.addView(graphics);backButton(this::showMainMenu);}
    private void showControls(){page("CONTROLS");menu.addView(label("TOUCH\nLEFT SIDE  MOVE\nRIGHT DRAG  LOOK\nTAP  LUNGE\nHOLD  VACUUM\n\nCONTROLLER\nA  JUMP   B  LUNGE   X  SHOOT\nR1  VACUUM   R3  CAMERA",14));menu.addView(label("TOUCH LOOK",13));SeekBar touch=controlSlider(touchLook);touch.setOnSeekBarChangeListener(listener(v->{touchLook=0.5f+v*1.25f;gameView.setLookSensitivity(touchLook,controllerLook);}));menu.addView(touch);menu.addView(label("CONTROLLER LOOK",13));SeekBar controller=controlSlider(controllerLook);controller.setOnSeekBarChangeListener(listener(v->{controllerLook=0.5f+v*1.25f;gameView.setLookSensitivity(touchLook,controllerLook);}));menu.addView(controller);Button defaults=button("DEFAULTS");defaults.setOnClickListener(v->{touchLook=controllerLook=1.0f;gameView.setLookSensitivity(1,1);showControls();});menu.addView(defaults);backButton(this::showSettings);}
    private void showAudio(){page("AUDIO");menu.addView(label("MUSIC",15));SeekBar music=slider(musicVolume);music.setOnSeekBarChangeListener(listener(v->{musicVolume=v;applySettings();}));menu.addView(music);Button mm=button(musicMuted?"MUSIC ON":"MUSIC MUTE");mm.setOnClickListener(v->{musicMuted=!musicMuted;applySettings();showAudio();});menu.addView(mm);menu.addView(label("SFX",15));SeekBar sfx=slider(sfxVolume);sfx.setOnSeekBarChangeListener(listener(v->{sfxVolume=v;applySettings();}));menu.addView(sfx);Button sm=button(sfxMuted?"SFX ON":"SFX MUTE");sm.setOnClickListener(v->{sfxMuted=!sfxMuted;applySettings();showAudio();});menu.addView(sm);backButton(this::showSettings);}
    private void showGraphics(){page("GRAPHICS");String[] names={"LEGACY","NORMAL","PRETTY"};Button preset=button(names[graphicsPreset]);preset.setOnClickListener(v->{graphicsPreset=(graphicsPreset+1)%3;if(graphicsPreset==0){shadows=false;portal=false;particles=false;}else{shadows=true;portal=true;particles=true;}applySettings();showGraphics();});menu.addView(preset);Button shadow=button(shadows?"SHADOWS ON":"SHADOWS OFF");shadow.setOnClickListener(v->{shadows=!shadows;applySettings();showGraphics();});menu.addView(shadow);Button particle=button(particles?"PARTICLES ON":"PARTICLES OFF");particle.setOnClickListener(v->{particles=!particles;applySettings();showGraphics();});menu.addView(particle);Button fpsButton=button(fps?"FPS ON":"FPS OFF");fpsButton.setOnClickListener(v->{fps=!fps;applySettings();showGraphics();});menu.addView(fpsButton);backButton(this::showSettings);}
    private void applySettings(){NativeBridge.applyLocalSettings(musicVolume,sfxVolume,musicMuted,sfxMuted,graphicsPreset,shadows,portal,particles,fps);}
    private SeekBar slider(float value){SeekBar bar=new SeekBar(this);bar.setMax(100);bar.setProgress(Math.round(value*100));bar.setLayoutParams(new LinearLayout.LayoutParams(420,-2));return bar;}
    private SeekBar controlSlider(float value){return slider((value-0.5f)/1.25f);}
    private SeekBar.OnSeekBarChangeListener listener(java.util.function.Consumer<Float> update){return new SeekBar.OnSeekBarChangeListener(){public void onProgressChanged(SeekBar bar,int value,boolean user){if(user)update.accept(value/100.0f);}public void onStartTrackingTouch(SeekBar bar){}public void onStopTrackingTouch(SeekBar bar){}};}
    private void backButton(Runnable action){Button back=button("BACK");back.setOnClickListener(v->action.run());menu.addView(back);}
    private TextView label(String text, int size) { TextView view = new TextView(this); view.setText(text); view.setTextColor(0xFFE8FCFF); view.setTextSize(size); view.setGravity(Gravity.CENTER); view.setPadding(8, 10, 8, 10); return view; }
    private Button button(String text) { Button view=new Button(this);view.setText(text);view.setTextColor(Color.WHITE);view.setTextSize(18);view.setAllCaps(false);view.setMinWidth(420);view.setMinHeight(58);GradientDrawable bg=new GradientDrawable();bg.setColor(0x180ACDEB);bg.setStroke(1,0x888FF7FF);bg.setCornerRadius(0);view.setBackground(bg);view.setOnTouchListener((v,e)->{if(e.getActionMasked()==android.view.MotionEvent.ACTION_DOWN){NativeBridge.playMenuCue(true);v.animate().scaleX(0.992f).scaleY(0.992f).alpha(0.86f).setDuration(110).start();}else if(e.getActionMasked()==android.view.MotionEvent.ACTION_UP||e.getActionMasked()==android.view.MotionEvent.ACTION_CANCEL)v.animate().scaleX(1).scaleY(1).alpha(1).setDuration(220).start();return false;});view.setOnFocusChangeListener((v,focused)->{if(focused&&v.hasWindowFocus())NativeBridge.playMenuCue(false);v.animate().scaleX(focused?1.008f:1).scaleY(focused?1.008f:1).setDuration(260).start();});LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(420,-2);lp.setMargins(0,6,0,6);view.setLayoutParams(lp);return view; }

    @Override
    protected void onResume() {
        super.onResume();
        if (gameView != null) gameView.resumeGameLoop();
    }
}
