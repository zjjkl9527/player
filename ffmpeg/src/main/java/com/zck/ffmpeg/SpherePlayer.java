package com.zck.ffmpeg;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;

public class SpherePlayer  {
    private final SphereSurfaceView mSphereSurfaceView;
    private final ArrayBlockingQueue<byte[]> mVideoFrameQueue = new ArrayBlockingQueue<byte[]>(20);
    private final AtomicBoolean pollFlag = new AtomicBoolean(false);
    private final AtomicBoolean bufferFlag= new AtomicBoolean(false);
    private final AtomicBoolean decodeFlag= new AtomicBoolean(false);
    private final AtomicBoolean gyroFlag= new AtomicBoolean(true);
    private final AtomicBoolean isPlayingFlag=new AtomicBoolean(false);
    private final AtomicBoolean pressedFlag = new AtomicBoolean(false);
    private final AtomicBoolean gyroEnable = new AtomicBoolean(false);
    private PollRTMPFrameThread mPollRTMPFrameThread;
    private VideoBufferThread mVideoBufferThread;
    private VideoDecoderThread mVideoDecoderThread;
    private MediaCodec mVideoDecoder;
    private Demuxer mDemuxer;
    private String mUrl;
    private float mStartX;
    private float mStartY;
    private float mLastDistance;
    private float mPreviousXs, mPreviousYs;
    private float mTimeStamp;
    private final float[] mAngle = new float[3];
    private static final double NS2S = 1.0 / 1000000000.0;
    private EventListener mEventListener;




    public SpherePlayer(SphereSurfaceView sphereSurfaceView) {
        mSphereSurfaceView = sphereSurfaceView;
    }

    public void setEventListener(EventListener eventListener){
        mEventListener=eventListener;
    }
    public interface EventListener {
        void UrlError();
        void pollError();
        void SurfaceViewError();
        void DecodeError();
    }


    public void play(String url) {
        if(mEventListener==null) return;
        if(mSphereSurfaceView==null){
            mEventListener.SurfaceViewError();
            return;
        }
        if(url==null){
            mEventListener.UrlError();
            return;
        }
        mUrl=url;
        mDemuxer=new Demuxer();
        initMediaCodec();
        onTouchEvent();
        mPollRTMPFrameThread = new PollRTMPFrameThread();
        pollFlag.set(true);
        mPollRTMPFrameThread.start();

        mVideoBufferThread = new VideoBufferThread();
        bufferFlag.set(true);
        mVideoBufferThread.start();

        mVideoDecoderThread = new VideoDecoderThread();
        decodeFlag.set(true);
        mVideoDecoderThread.start();
    }


    public void stop() {
        isPlayingFlag.set(false);
        if (mPollRTMPFrameThread != null) pollFlag.set(false);
        if (mVideoDecoderThread != null) bufferFlag.set(false);
        if (mVideoBufferThread != null) decodeFlag.set(false);
        if(!gyroFlag.get()) gyroEnable.set(false);
        if(mDemuxer!=null) mDemuxer=null;
        mVideoDecoder.stop();
        mVideoDecoder.flush();
        mVideoDecoder.release();
        mVideoFrameQueue.clear();
    }

    public boolean isPlaying(){
        return isPlayingFlag.get();
    }

    public void setGyroEnable(boolean b,Context context){
        if(gyroFlag.get()){
            gyroscope(context);
            gyroFlag.set(false);
        }
        gyroEnable.set(b);
    }

    public boolean isGyroEnable(){
        return gyroEnable.get();
    }

    private void gyroscope(Context context){
        SensorEventListener mSensorListener = new SensorEventListener() {
            final Configuration mConfiguration=context.getResources().getConfiguration();
            final int ori=mConfiguration.orientation;
            @Override
            public void onSensorChanged(SensorEvent event) {
                if(gyroEnable.get()){
                    if (event.sensor.getType() == Sensor.TYPE_GYROSCOPE) {
                        if (mTimeStamp != 0) {
                            final double dT = (event.timestamp - mTimeStamp) * NS2S;
                            mAngle[0] += event.values[0] * dT;
                            mAngle[1] += event.values[1] * dT;
                            float angleX = (float) Math.toDegrees(mAngle[0]);
                            float angleY = (float) Math.toDegrees(mAngle[1]);
                            float dx = angleX - mPreviousXs;
                            float dy = angleY - mPreviousYs;
                            if(ori==1){
                                mSphereSurfaceView.getSphereRender().rotateX(dx);
                                mSphereSurfaceView.getSphereRender().rotateY(-dy);
                            }
                            if(ori==2){
                                mSphereSurfaceView.getSphereRender().rotateX(-dy);
                                mSphereSurfaceView.getSphereRender().rotateY(-dx);
                            }
                            mSphereSurfaceView.requestRender();

                            mPreviousYs = angleY;
                            mPreviousXs = angleX;
                        }
                        mTimeStamp = event.timestamp;
                    }
                }
            }

            @Override
            public void onAccuracyChanged(Sensor sensor, int accuracy) {
            }
        };
        SensorManager mSensorMgr = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        Sensor mGyroscopeSensor = mSensorMgr.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        mSensorMgr.registerListener(mSensorListener, mGyroscopeSensor, 0, 0);
    }

    @SuppressLint("ClickableViewAccessibility")
    private void onTouchEvent(){
        mSphereSurfaceView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getPointerCount()) {
                    case 1:
                        switch (event.getAction() & MotionEvent.ACTION_MASK) {
                            case MotionEvent.ACTION_DOWN:
                                mStartX = event.getX();
                                mStartY = event.getY();
                                pressedFlag.set(true);
                                break;

                            case MotionEvent.ACTION_MOVE:
                                if (!pressedFlag.get()) break;

                                float x = event.getX();
                                float y = event.getY();
                                float deltaX = mStartX - x;
                                float deltaY = mStartY - y;

                                mStartX = x;
                                mStartY = y;

                                mSphereSurfaceView.getSphereRender().rotateY(0.05f * deltaX);
                                mSphereSurfaceView.getSphereRender().rotateX(-0.05f * deltaY);
                                mSphereSurfaceView.requestRender();
                                break;
                            case MotionEvent.ACTION_UP:
                                break;
                        }

                        break;

                    case 2:
                        switch (event.getAction() & MotionEvent.ACTION_MASK) {
                            case MotionEvent.ACTION_POINTER_DOWN: {
                                pressedFlag.set(false);

                                float deltaX = event.getX(0) - event.getX(1);
                                float deltaY = event.getY(0) - event.getY(1);
                                mLastDistance = (float) Math.pow((deltaX * deltaX + deltaY * deltaY), 0.5);
                            }
                            break;

                            case MotionEvent.ACTION_MOVE: {
                                float deltaX = event.getX(0) - event.getX(1);
                                float deltaY = event.getY(0) - event.getY(1);
                                float distance = (float) Math.pow((deltaX * deltaX + deltaY * deltaY), 0.5);

                                float deltaDistance = distance - mLastDistance;
                                mLastDistance = distance;

                                mSphereSurfaceView.getSphereRender().zoom(-0.05f * deltaDistance);
                                mSphereSurfaceView.requestRender();
                            }
                            break;
                            case MotionEvent.ACTION_UP:
                                break;
                        }
                        break;

                    default:
                        break;
                }
                return true;
            }
        });
    }




    private void initMediaCodec(){
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                mVideoDecoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
            }else {
                mVideoDecoder=MediaCodec.createDecoderByType("video/avc");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        MediaFormat videoFormat = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            videoFormat = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 1920, 1080);
        }else {
            videoFormat = MediaFormat.createVideoFormat("video/avc", 1920, 1080);
        }
        mVideoDecoder.configure(videoFormat,mSphereSurfaceView.getSphereRender().getSurface(),null,0);
        mVideoDecoder.start();
    }





    private class PollRTMPFrameThread extends Thread {
        @Override
        public void run() {
            startPollRTMPFrame(mUrl);
         }
    }
    private void startPollRTMPFrame(String url){
        try {
            mDemuxer.open(url, new Demuxer.Callback() {
                @Override
                public void onVideo(byte[] frame, long pts) {
                    if (mVideoFrameQueue.remainingCapacity() > 0) {
                        mVideoFrameQueue.offer(frame);
                    }
                }
                @Override
                public void onAudio(byte[] frame, long pts) {
                }
            });
        } catch (IOException e) {
            e.printStackTrace();
            mEventListener.UrlError();
            stop();
        }
        while (true) {
            if(pollFlag.get()){
                try {
                    mDemuxer.flush();
                } catch (IOException e) {
                    e.printStackTrace();
                    mEventListener.pollError();
                    stop();
                }
            }else return;
        }
    }


    private class VideoBufferThread extends Thread {
        @Override
        public void run() {
            while (true) {
                if (bufferFlag.get()) {
                    try {
                        int inIndex = mVideoDecoder.dequeueInputBuffer(33333);
                        if (inIndex < 0) continue;
                        byte[] data = mVideoFrameQueue.take();
                        mVideoFrameQueue.remove(data);

                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                            ByteBuffer inputBuffer = mVideoDecoder.getInputBuffer(inIndex);
                            inputBuffer.put(data, 0, data.length);
                        } else {
                            ByteBuffer[] inputBuffers = mVideoDecoder.getInputBuffers();
                            ByteBuffer inputBuffer = inputBuffers[inIndex];
                            inputBuffer.put(data, 0, data.length);
                        }
                        mVideoDecoder.queueInputBuffer(inIndex, 0, data.length, 0, 0);
                    } catch (Exception e) {
                        e.printStackTrace();
                        mEventListener.DecodeError();
                        SpherePlayer.this.stop();
                    }
                } else {
                    return;
                }
            }
        }
    }



    private class VideoDecoderThread extends Thread {
        @Override
        public void run() {
            while (true) {
                if (decodeFlag.get()) {
                    try {
                        long startTime = System.currentTimeMillis();
                        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
                        int outIndex = mVideoDecoder.dequeueOutputBuffer(info, 0);
                        if (outIndex < 0) continue;
                        mVideoDecoder.releaseOutputBuffer(outIndex, true);
                        mSphereSurfaceView.requestRender();
                        isPlayingFlag.set(pollFlag.get()&&bufferFlag.get()&&decodeFlag.get());

                        long endTime = System.currentTimeMillis();
                        long delta = endTime - startTime;

                        if (delta < 33) {
                            try {
                                Thread.sleep(33 - delta);
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                        }
                    } catch (Exception e) {
                        e.printStackTrace();
                        mEventListener.DecodeError();
                        SpherePlayer.this.stop();
                    }
                } else {
                    return;
                }
            }
        }
    }


    private static class Demuxer {

        public Demuxer(){
            mPointer = createDemuxer();
        }

        public void open(String url, com.zck.ffmpeg.SpherePlayer.Demuxer.Callback callback) throws IOException {
            int ret =openInput(mPointer, url, callback);
            if(ret < 0) throw new IOException(getErrorString(ret));
        }

        public void flush() throws IOException{
            int ret = flush(mPointer);
            if(ret < 0) throw new IOException(getErrorString(ret));
        }

        @Override
        public void finalize() throws Throwable {
            super.finalize();
            destroyDemuxer(mPointer);
        }

        private final long mPointer;

        public interface Callback{
            void onVideo(byte[] frame, long pts);

            void onAudio(byte[] frame, long pts);
        }

        static {
            System.loadLibrary("native-lib");
        }

        private native long createDemuxer();

        private native void destroyDemuxer(long pointer);

        private native int openInput(long pointer, String url, com.zck.ffmpeg.SpherePlayer.Demuxer.Callback callback);

        private native int flush(long pointer);

        private native String getErrorString(int value);

    }


}


