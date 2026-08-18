#include <gtest/gtest.h>
#include "audio/AudioRingBuffer.h"

using namespace rtvcc;

TEST(AudioRingBufferTest, BasicPushPop) {
    AudioRingBuffer buffer(1024);
    AudioFrame frame{};
    frame.data[0] = 1.0f;
    frame.data[1] = 2.0f;

    EXPECT_TRUE(buffer.push(frame));
    AudioFrame popped;
    EXPECT_TRUE(buffer.pop(popped));
    EXPECT_FLOAT_EQ(popped.data[0], 1.0f);
    EXPECT_FLOAT_EQ(popped.data[1], 2.0f);
}

TEST(AudioRingBufferTest, EmptyPopReturnsFalse) {
    AudioRingBuffer buffer(1024);
    AudioFrame frame;
    EXPECT_FALSE(buffer.pop(frame));
}

TEST(AudioRingBufferTest, FullPushReturnsFalse) {
    AudioRingBuffer buffer(4);
    AudioFrame frame{};

    EXPECT_TRUE(buffer.push(frame));
    EXPECT_TRUE(buffer.push(frame));
    EXPECT_TRUE(buffer.push(frame));
    EXPECT_FALSE(buffer.push(frame));
}

TEST(AudioRingBufferTest, Wraparound) {
    AudioRingBuffer buffer(8);
    AudioFrame frame{};

    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 7; ++i) {
            frame.data[0] = static_cast<float>(cycle * 10 + i);
            EXPECT_TRUE(buffer.push(frame));
        }
        for (int i = 0; i < 7; ++i) {
            EXPECT_TRUE(buffer.pop(frame));
            EXPECT_FLOAT_EQ(frame.data[0], static_cast<float>(cycle * 10 + i));
        }
    }
}

TEST(AudioRingBufferTest, BulkPushPop) {
    AudioRingBuffer buffer(1024);
    std::vector<AudioFrame> frames(100);
    for (size_t i = 0; i < frames.size(); ++i) {
        frames[i].data[0] = static_cast<float>(i);
    }

    size_t pushed = buffer.push(frames.data(), frames.size());
    EXPECT_EQ(pushed, frames.size());

    std::vector<AudioFrame> popped_frames(100);
    size_t popped = buffer.pop(popped_frames.data(), popped_frames.size());
    EXPECT_EQ(popped, frames.size());

    for (size_t i = 0; i < frames.size(); ++i) {
        EXPECT_FLOAT_EQ(popped_frames[i].data[0], static_cast<float>(i));
    }
}

TEST(AudioRingBufferTest, PrepareWriteRead) {
    AudioRingBuffer buffer(1024);
    AudioFrame frame{};
    frame.data[0] = 42.0f;

    auto [chunk1, chunk2] = buffer.prepareWrite(1);
    EXPECT_EQ(chunk1, 1);
    EXPECT_EQ(chunk2, 0);
    *buffer.getWritePtr(0) = frame;
    buffer.commitWrite(1);

    auto [rchunk1, rchunk2] = buffer.prepareRead(1);
    EXPECT_EQ(rchunk1, 1);
    EXPECT_EQ(rchunk2, 0);
    const AudioFrame* read_frame = buffer.getReadPtr(0);
    EXPECT_FLOAT_EQ(read_frame->data[0], 42.0f);
    buffer.commitRead(1);
}

TEST(AudioRingBufferTest, Clear) {
    AudioRingBuffer buffer(1024);
    AudioFrame frame{};
    buffer.push(frame);
    buffer.clear();
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.pop(frame));
}

TEST(AudioRingBufferTest, SizeAndCapacity) {
    AudioRingBuffer buffer(1024);
    EXPECT_EQ(buffer.capacity(), 1023);
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_TRUE(buffer.empty());

    AudioFrame frame{};
    buffer.push(frame);
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer.available(), 1022);
    EXPECT_FALSE(buffer.full());

    for (int i = 0; i < 1022; ++i) buffer.push(frame);
    EXPECT_TRUE(buffer.full());
}