/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "ignition/transport/log/Playback.hh"
#include "gtest/gtest.h"

using namespace ignition;
using namespace ignition::transport;

//////////////////////////////////////////////////
TEST(Playback, PlaybackEmptyDatabase)
{
  log::Playback playback(":memory:");
  EXPECT_EQ(log::PlaybackError::FAILED_TO_OPEN, playback.Start());
  EXPECT_EQ(log::PlaybackError::FAILED_TO_OPEN, playback.AddTopic("/foo/bar"));
  EXPECT_EQ(log::PlaybackError::FAILED_TO_OPEN,
    static_cast<log::PlaybackError>(playback.AddTopic(std::regex(".*"))));
  EXPECT_EQ(log::PlaybackError::FAILED_TO_OPEN, playback.Start());
  EXPECT_EQ(log::PlaybackError::FAILED_TO_OPEN, playback.Stop());
  playback.WaitUntilFinished();
}


//////////////////////////////////////////////////
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}