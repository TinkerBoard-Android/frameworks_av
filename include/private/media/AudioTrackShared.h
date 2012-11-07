/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIO_TRACK_SHARED_H
#define ANDROID_AUDIO_TRACK_SHARED_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/threads.h>

namespace android {

// ----------------------------------------------------------------------------

// Maximum cumulated timeout milliseconds before restarting audioflinger thread
#define MAX_STARTUP_TIMEOUT_MS  3000    // Longer timeout period at startup to cope with A2DP
                                        // init time
#define MAX_RUN_TIMEOUT_MS      1000
#define WAIT_PERIOD_MS          10
// AudioTrack no longer uses this, it is for AudioRecord only:
#define RESTORE_TIMEOUT_MS      5000    // Maximum waiting time for a track to be restored

#define CBLK_UNDERRUN   0x01 // set: underrun (out) or overrrun (in), clear: no underrun or overrun
#define CBLK_FORCEREADY 0x02 // set: track is considered ready immediately by AudioFlinger,
                             // clear: track is ready when buffer full
#define CBLK_INVALID    0x04 // track buffer invalidated by AudioFlinger, need to re-create
#define CBLK_DISABLED   0x08 // track disabled by AudioFlinger due to underrun, need to re-start
// AudioTrack no longer uses these, they are for AudioRecord only:
#define CBLK_RESTORING  0x10 // track is being restored after invalidation by AudioFlinger
#define CBLK_RESTORED   0x20 // track has been restored after invalidation by AudioFlinger

// Important: do not add any virtual methods, including ~
struct audio_track_cblk_t
{

    // The data members are grouped so that members accessed frequently and in the same context
    // are in the same line of data cache.
                Mutex       lock;           // sizeof(int)
                Condition   cv;             // sizeof(int)

                // next 4 are offsets within "buffers"
    volatile    uint32_t    user;
    volatile    uint32_t    server;
                uint32_t    userBase;
                uint32_t    serverBase;

                // if there is a shared buffer, "buffers" is the value of pointer() for the shared
                // buffer, otherwise "buffers" points immediately after the control block
                void*       buffers;
                uint32_t    frameCount;

                // Cache line boundary

                uint32_t    loopStart;
                uint32_t    loopEnd;        // read-only for server, read/write for client
                int         loopCount;      // read/write for client

                // Channel volumes are fixed point U4.12, so 0x1000 means 1.0.
                // Left channel is in [0:15], right channel is in [16:31].
                // Always read and write the combined pair atomically.
                // For AudioTrack only, not used by AudioRecord.
private:
                uint32_t    mVolumeLR;
public:

                uint32_t    sampleRate;

                // NOTE: audio_track_cblk_t::frameSize is not equal to AudioTrack::frameSize() for
                // 8 bit PCM data: in this case,  mCblk->frameSize is based on a sample size of
                // 16 bit because data is converted to 16 bit before being stored in buffer

                // read-only for client, server writes once at initialization and is then read-only
                uint8_t     frameSize;       // would normally be size_t, but 8 bits is plenty
                uint8_t     mName;           // normal tracks: track name, fast tracks: track index

                // used by client only
                uint16_t    bufferTimeoutMs; // Maximum cumulated timeout before restarting
                                             // audioflinger

                uint16_t    waitTimeMs;      // Cumulated wait time, used by client only
private:
                // client write-only, server read-only
                uint16_t    mSendLevel;      // Fixed point U4.12 so 0x1000 means 1.0
public:
    volatile    int32_t     flags;

                // Cache line boundary (32 bytes)

                // Since the control block is always located in shared memory, this constructor
                // is only used for placement new().  It is never used for regular new() or stack.
                            audio_track_cblk_t();

                // called by client only, where client includes regular
                // AudioTrack and AudioFlinger::PlaybackThread::OutputTrack
                uint32_t    stepUserIn(uint32_t frameCount) { return stepUser(frameCount, false); }
                uint32_t    stepUserOut(uint32_t frameCount) { return stepUser(frameCount, true); }

                bool        stepServer(uint32_t frameCount, bool isOut);

                void*       buffer(uint32_t offset) const;
                uint32_t    framesAvailableIn() { return framesAvailable(false); }
                uint32_t    framesAvailableOut() { return framesAvailable(true); }
                uint32_t    framesAvailableIn_l() { return framesAvailable_l(false); }
                uint32_t    framesAvailableOut_l() { return framesAvailable_l(true); }
                uint32_t    framesReadyIn() { return framesReady(false); }
                uint32_t    framesReadyOut() { return framesReady(true); }

                bool        tryLock();

                // No barriers on the following operations, so the ordering of loads/stores
                // with respect to other parameters is UNPREDICTABLE. That's considered safe.

                // for AudioTrack client only, caller must limit to 0.0 <= sendLevel <= 1.0
                void        setSendLevel(float sendLevel) {
                    mSendLevel = uint16_t(sendLevel * 0x1000);
                }

                // for AudioFlinger only; the return value must be validated by the caller
                uint16_t    getSendLevel_U4_12() const {
                    return mSendLevel;
                }

                // for AudioTrack client only, caller must limit to 0 <= volumeLR <= 0x10001000
                void        setVolumeLR(uint32_t volumeLR) {
                    mVolumeLR = volumeLR;
                }

                // for AudioFlinger only; the return value must be validated by the caller
                uint32_t    getVolumeLR() const {
                    return mVolumeLR;
                }

private:
                // isOut == true means AudioTrack, isOut == false means AudioRecord
                uint32_t    stepUser(uint32_t frameCount, bool isOut);
                uint32_t    framesAvailable(bool isOut);
                uint32_t    framesAvailable_l(bool isOut);
                uint32_t    framesReady(bool isOut);
};


// ----------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_AUDIO_TRACK_SHARED_H
