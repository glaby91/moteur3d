package org.csewarp.androidprobe;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.util.Log;
import android.view.View;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import org.qtproject.qt.android.bindings.QtActivity;

/** HUD composed by Android, independently of the native Vulkan scene.
 * No screenshot, PixelCopy, CPU readback or second 3D renderer.
 * Touch passes through this non-interactive overlay to the Qt window.
 */
public class WarpActivity extends QtActivity {
    private WarpHud hud;
    public void updateWarpHud(String title, String metrics, boolean visible,
            boolean menu, boolean crouched, int scale, int msaa, int sensitivity,
            float dx, float dy) {
        runOnUiThread(() -> {
            if (hud == null || hud.getParent() == null) {
                hud = new WarpHud();
                addContentView(hud, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
                hud.bringToFront();
            }
            hud.title = title; hud.metrics = metrics;
            hud.visible = visible; hud.menu = menu; hud.crouched = crouched;
            hud.scale = scale; hud.msaa = msaa; hud.sensitivity = sensitivity;
            hud.dx = dx; hud.dy = dy;
            hud.invalidate();
        });
    }

    private final class WarpHud extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final int accent = Color.rgb(103, 222, 207);
        private final int ink = Color.rgb(243, 248, 250);
        private final int panel = Color.argb(226, 14, 24, 33);
        String title = "CSE WARP", metrics = "";
        boolean visible = true, menu = false, crouched = false;
        int scale = 100, msaa = 1, sensitivity = 1;
        float dx = 0, dy = 0;
        boolean announced = false, lastMenu = false, lastVisible = true;
        WarpHud() {
            super(WarpActivity.this);
            setClickable(false); setLongClickable(false); setFocusable(false);
            setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            setBackgroundColor(Color.TRANSPARENT);
        }
        @Override public boolean onTouchEvent(MotionEvent event) { return false; }
        private float x(float value) { return value * getWidth(); }
        private float y(float value) { return value * getHeight(); }
        private float unit() { return Math.min(getWidth(), getHeight()); }
        private void box(Canvas c,float a,float b,float d,float e,int fill,int stroke) {
            RectF r = new RectF(x(a),y(b),x(d),y(e));
            paint.setStyle(Paint.Style.FILL); paint.setColor(fill);
            c.drawRoundRect(r,unit()*.016f,unit()*.016f,paint);
            if (stroke != 0) {
                paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(unit()*.002f);
                paint.setColor(stroke);
                c.drawRoundRect(r,unit()*.016f,unit()*.016f,paint);
                paint.setStyle(Paint.Style.FILL);
            }
        }
        private void label(Canvas c,String text,float cx,float cy,float size,int color,boolean center,boolean bold) {
            paint.setStyle(Paint.Style.FILL); paint.setColor(color);
            paint.setTypeface(Typeface.create("sans-serif",bold?Typeface.BOLD:Typeface.NORMAL));
            paint.setTextSize(unit()*size); paint.setTextAlign(center?Paint.Align.CENTER:Paint.Align.LEFT);
            c.drawText(text,x(cx),y(cy)-(paint.ascent()+paint.descent())/2,paint);
        }
        private void button(Canvas c,float a,float b,float d,float e,String text,boolean selected) {
            box(c,a,b,d,e,selected?Color.argb(230,24,77,77):panel,accent);
            label(c,text,(a+d)/2,(b+e)/2,.026f,ink,true,true);
        }
        @Override protected void onDraw(Canvas c) {
            super.onDraw(c);
            if (getWidth()<=0 || getHeight()<=0) return;
            if (visible) {
                box(c,.018f,.018f,.70f,.12f,panel,0);
                label(c,title,.034f,.048f,.027f,accent,false,true);
                label(c,metrics,.034f,.092f,.023f,ink,false,false);
            }
            button(c,.736f,.02f,.856f,.112f,visible?"HUD ON":"HUD OFF",visible);
            button(c,.865f,.02f,.982f,.112f,menu?"FERMER":"MENU",menu);
            if (menu) {
                paint.setStyle(Paint.Style.FILL); paint.setColor(Color.argb(110,0,0,0));
                c.drawRect(0,y(.14f),getWidth(),getHeight(),paint);
                box(c,.285f,.145f,.715f,.98f,panel,accent);
                label(c,"PARAMÈTRES",.5f,.19f,.034f,accent,true,true);
                button(c,.30f,.23f,.70f,.37f,"Rendu : "+scale+" %",false);
                button(c,.30f,.39f,.70f,.53f,"Antialiasing : x"+msaa,false);
                button(c,.30f,.55f,.70f,.68f,"Caméra suivante",false);
                button(c,.30f,.70f,.70f,.82f,"Sensibilité : "+sensitivity,false);
                button(c,.30f,.84f,.70f,.96f,"REPRENDRE",true);
            } else if (visible) {
                final float cx=x(.14f), cy=y(.80f), r=unit()*.115f;
                paint.setStyle(Paint.Style.FILL); paint.setColor(Color.argb(150,14,24,33));
                c.drawCircle(cx,cy,r,paint);
                paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(unit()*.003f);paint.setColor(accent);
                c.drawCircle(cx,cy,r,paint);
                paint.setStyle(Paint.Style.FILL);paint.setColor(Color.argb(215,103,222,207));
                c.drawCircle(cx+dx*r*.67f,cy+dy*r*.67f,r*.30f,paint);
                label(c,"MARCHE",.14f,.95f,.022f,ink,true,true);
                label(c,"Glisser pour regarder",.68f,.68f,.022f,ink,true,false);
                button(c,.805f,.75f,.896f,.95f,"ACC.",crouched);
                button(c,.907f,.75f,.982f,.95f,"SAUT",false);
                paint.setColor(Color.argb(190,255,255,255)); paint.setStrokeWidth(unit()*.002f);
                float mx=x(.5f),my=y(.5f),s=unit()*.011f;
                c.drawLine(mx-s,my,mx+s,my,paint);c.drawLine(mx,my-s,mx,my+s,paint);
            }
            // Actual Canvas draw, not just a C++ request. Used in emulator QA.
            if (!announced) {
                announced=true;
                Log.i("CSE_WARP_HUD","CSE_ANDROID PASS HUD_DRAW "+getWidth()+" "+getHeight());
            }
            if (lastMenu!=menu) {
                lastMenu=menu;
                Log.i("CSE_WARP_HUD",menu?"CSE_ANDROID PASS MENU_VISIBLE":"CSE_ANDROID PASS MENU_CLOSED");
            }
            if (lastVisible!=visible) {
                lastVisible=visible;
                Log.i("CSE_WARP_HUD",visible?"CSE_ANDROID PASS HUD_SHOWN":"CSE_ANDROID PASS HUD_HIDDEN");
            }
        }
    }
}
