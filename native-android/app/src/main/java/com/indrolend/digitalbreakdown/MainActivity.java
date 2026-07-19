package com.indrolend.digitalbreakdown;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
<<<<<<< Updated upstream
import android.view.Surface;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
=======
import android.graphics.Color;
import android.text.InputFilter;
import android.view.Gravity;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
>>>>>>> Stashed changes

public final class MainActivity extends Activity {
    private GameView gameView;
    private MultiplayerClient multiplayer;
    private LinearLayout menu;
    private TextView networkStatus;

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
<<<<<<< Updated upstream
        ControlOverlayView controls = new ControlOverlayView(this, gameView);
        FrameLayout root = new FrameLayout(this);
        root.addView(gameView, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));
        root.addView(controls, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));
        setContentView(root);

        gameView.post(() -> requestSixtyHertz(gameView.getHolder().getSurface()));
    }

    private static void requestSixtyHertz(Surface surface) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && surface != null && surface.isValid()) {
            surface.setFrameRate(60.0f, Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
        }
=======
        multiplayer = new MultiplayerClient(this, BuildConfig.MULTIPLAYER_URL, this::onNetworkStatus);
        NativeBridge.setMultiplayerClient(multiplayer);

        FrameLayout root = new FrameLayout(this);
        root.addView(gameView, new FrameLayout.LayoutParams(-1, -1));
        menu = new LinearLayout(this);
        menu.setOrientation(LinearLayout.VERTICAL); menu.setGravity(Gravity.CENTER); menu.setPadding(48, 32, 48, 32);
        menu.setBackgroundColor(0xDD061419);
        TextView title = label("DIGITAL BREAKDOWN", 28); menu.addView(title);
        networkStatus = label("SOLO OR ONLINE CO-OP", 16); menu.addView(networkStatus);
        Button solo = button("SOLO"); solo.setOnClickListener(v -> { multiplayer.close(); NativeBridge.startSolo(); menu.setVisibility(android.view.View.GONE); }); menu.addView(solo);
        Button host = button("HOST ONLINE ROOM"); host.setOnClickListener(v -> multiplayer.host()); menu.addView(host);
        EditText code = new EditText(this); code.setHint("ROOM CODE"); code.setTextColor(Color.WHITE); code.setHintTextColor(0xFF72DDE4); code.setSingleLine(true); code.setAllCaps(true); code.setFilters(new InputFilter[]{new InputFilter.LengthFilter(6)}); menu.addView(code, new LinearLayout.LayoutParams(420, -2));
        Button join = button("JOIN ROOM"); join.setOnClickListener(v -> multiplayer.join(code.getText().toString())); menu.addView(join);
        FrameLayout.LayoutParams panel = new FrameLayout.LayoutParams(620, -2, Gravity.CENTER); root.addView(menu, panel);
        setContentView(root);
>>>>>>> Stashed changes
    }

    @Override
    protected void onPause() {
        if (gameView != null) gameView.pauseGameLoop();
        super.onPause();
    }

    @Override protected void onDestroy() { if (multiplayer != null) multiplayer.close(); super.onDestroy(); }

    private void onNetworkStatus(String status, String code, boolean connected) {
        networkStatus.setText(code.isEmpty() ? status : status + "\nCODE: " + code);
        if (connected) menu.setVisibility(android.view.View.GONE);
    }
    private TextView label(String text, int size) { TextView view = new TextView(this); view.setText(text); view.setTextColor(0xFF72DDE4); view.setTextSize(size); view.setGravity(Gravity.CENTER); view.setPadding(8, 8, 8, 8); return view; }
    private Button button(String text) { Button view = new Button(this); view.setText(text); view.setAllCaps(false); return view; }

    @Override
    protected void onResume() {
        super.onResume();
        if (gameView != null) gameView.resumeGameLoop();
    }
}