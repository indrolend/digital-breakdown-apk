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

        final float joystickRadius = gameView.joystickRadius();
        final float joystickX = gameView.joystickCenterX();
        final float joystickY = gameView.joystickCenterY();
        drawCircleControl(canvas, joystickX, joystickY, joystickRadius, "");

        final float knobRadius = joystickRadius * 0.43f;
        final float knobX = joystickX + gameView.getMoveX() * joystickRadius * 0.62f;
        final float knobY = joystickY - gameView.getMoveZ() * joystickRadius * 0.62f;
        fillPaint.setColor(Color.argb(150, 205, 243, 255));
        canvas.drawCircle(knobX, knobY, knobRadius, fillPaint);

        final float r = gameView.buttonRadius();
        drawCircleControl(canvas, gameView.actionCenterX(), gameView.actionCenterY(), r * 1.10f,
            gameView.isVacuumHeld() ? "VAC" : "HIT");
        drawCircleControl(canvas, gameView.jumpCenterX(), gameView.jumpCenterY(), r, "JUMP");
        drawCircleControl(canvas, gameView.shootCenterX(), gameView.shootCenterY(), r, "SOUL");
        drawCircleControl(canvas, gameView.cameraCenterX(), gameView.cameraCenterY(), r, "CAM");

        textPaint.setTextSize(r * 0.27f);
        textPaint.setColor(Color.argb(190, 220, 245, 255));
        canvas.drawText("tap", gameView.actionCenterX(), gameView.actionCenterY() + r * 0.43f, textPaint);
        canvas.drawText("hold + slide", gameView.actionCenterX(), gameView.actionCenterY() + r * 0.72f, textPaint);

        postInvalidateOnAnimation();
    }

    private void drawCircleControl(Canvas canvas, float cx, float cy, float radius, String label) {
        fillPaint.setColor(Color.argb(72, 8, 20, 28));
        strokePaint.setColor(Color.argb(185, 150, 225, 245));
        canvas.drawCircle(cx, cy, radius, fillPaint);
        canvas.drawCircle(cx, cy, radius, strokePaint);
        if (!label.isEmpty()) {
            textPaint.setTextSize(radius * 0.31f);
            textPaint.setColor(Color.argb(225, 235, 250, 255));
            final Paint.FontMetrics fm = textPaint.getFontMetrics();
            final float baseline = cy - (fm.ascent + fm.descent) * 0.5f;
            canvas.drawText(label, cx, baseline, textPaint);
        }
    }

    private float dp(float value) {
        return value * getResources().getDisplayMetrics().density;
    }
}