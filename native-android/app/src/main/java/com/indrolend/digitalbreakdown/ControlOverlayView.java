package com.indrolend.digitalbreakdown;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

public final class ControlOverlayView extends View {
    private final GameView gameView;
    private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint strokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    public ControlOverlayView(Context context, GameView gameView) {
        super(context);
        this.gameView = gameView;
        setFocusable(false);
        setBackgroundColor(Color.TRANSPARENT);
        fillPaint.setStyle(Paint.Style.FILL);
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(dp(2.0f));
        textPaint.setTextAlign(Paint.Align.CENTER);
        textPaint.setTypeface(android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.NORMAL));
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final boolean handled = gameView.processTouchEvent(event);
        invalidate();
        return handled;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        final int menuMode = NativeBridge.getMenuMode();
        if (!NativeBridge.isStarted() || NativeBridge.isIntroActive() || menuMode != 0) {
            return;
        }

        final float r = gameView.buttonRadius();
        if (gameView.isMoveActive()) {
            final float joystickRadius = gameView.joystickRadius();
            final float joystickX = gameView.joystickCenterX();
            final float joystickY = gameView.joystickCenterY();
            drawCircleControl(canvas, joystickX, joystickY, joystickRadius, "");

            final float knobRadius = joystickRadius * 0.43f;
            final float knobX = joystickX + gameView.getMoveX() * joystickRadius * 0.62f;
            final float knobY = joystickY - gameView.getMoveZ() * joystickRadius * 0.62f;
            fillPaint.setColor(Color.argb(150, 120, 213, 225));
            canvas.drawCircle(knobX, knobY, knobRadius, fillPaint);
        }

        if (gameView.isActionActive()) {
            drawCircleControl(canvas, gameView.actionCenterX(), gameView.actionCenterY(), r * 1.02f,
                gameView.isVacuumHeld() ? "VAC" : "HIT");
        }
        drawOptionsControl(canvas);
    }

    private void drawCircleControl(Canvas canvas, float cx, float cy, float radius, String label) {
        fillPaint.setColor(Color.argb(48, 24, 12, 28));
        strokePaint.setColor(Color.argb(150, 120, 213, 225));
        canvas.drawCircle(cx, cy, radius, fillPaint);
        canvas.drawCircle(cx, cy, radius, strokePaint);
        if (!label.isEmpty()) {
            textPaint.setTextSize(radius * 0.31f);
            textPaint.setColor(Color.argb(190, 235, 250, 255));
            final Paint.FontMetrics fm = textPaint.getFontMetrics();
            final float baseline = cy - (fm.ascent + fm.descent) * 0.5f;
            canvas.drawText(label, cx, baseline, textPaint);
        }
    }

    private void drawOptionsControl(Canvas canvas) {
        final float cx = gameView.optionsCenterX();
        final float cy = gameView.optionsCenterY();
        final float radius = gameView.optionsRadius();
        drawCircleControl(canvas, cx, cy, radius, "");
        strokePaint.setStyle(Paint.Style.STROKE);
        strokePaint.setStrokeWidth(dp(1.6f));
        strokePaint.setColor(Color.argb(145, 120, 213, 225));
        final float w = radius * 0.58f;
        for (int i = -1; i <= 1; ++i) {
            final float y = cy + i * radius * 0.28f;
            canvas.drawLine(cx - w * 0.5f, y, cx + w * 0.5f, y, strokePaint);
        }
    }

    private float dp(float value) {
        return value * getResources().getDisplayMetrics().density;
    }
}
