/*
 * Copyright (C) 2017 Open Source Robotics Foundation
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

#include <gtest/gtest.h>

#include <ignition/common/Filesystem.hh>

#include <ignition/transport/test_config.h>

#include <ignition/transport/Node.hh>
#include <ignition/transport/log/Record.hh>
#include <ignition/transport/log/Log.hh>

#include "ChirpParams.hh"

//////////////////////////////////////////////////
/// \brief Similar to testing::forkAndRun(), except this function specifically
/// calls the INTEGRATION_topicChirp_aux process and passes it arguments to
/// determine how it should chirp out messages over its topics.
/// \param _topics A list of topic names to chirp on
/// \param _chirps The number of messages to chirp out. Each message will count
/// up starting from the value 1 and ending with the value _chirps.
/// \return A handle to the process. This can be used with
/// testing::waitAndCleanupFork().
testing::forkHandlerType BeginChirps(
    const std::vector<std::string> &_topics,
    const int _chirps)
{
  // Set the chirping process name
  const std::string process =
      IGN_TRANSPORT_LOG_BUILD_PATH"/INTEGRATION_topicChirp_aux";

  // Get the partition name by creating a temporary Node and checking its
  // default partition name.
  std::string partitionName =
      ignition::transport::Node().Options().Partition();

  // Argument list:
  // [0]: Executable name
  // [1]: Partition name
  // [2]: Number of chirps
  // [3]-[N]: Each topic name
  // [N+1]: Null terminator, required by execv
  const std::size_t numArgs = 3 + _topics.size() + 1;

  std::vector<std::string> strArgs;
  strArgs.reserve(numArgs-1);
  strArgs.push_back(process);
  strArgs.push_back(partitionName);
  strArgs.push_back(std::to_string(_chirps));
  strArgs.insert(strArgs.end(), _topics.begin(), _topics.end());

#ifdef _MSC_VER
  std::string fullArgs;
  for (std::size_t i = 0; i < strArgs.size(); ++i)
  {
    if (i == 0)
    {
      // Windows prefers quotes around the process name
      fullArgs += "\"";
    }
    else
    {
      fullArgs += " ";
    }

    fullArgs += strArgs[i];

    if (i == 0)
    {
      fullArgs += "\"";
    }
  }

  char * args = new char[fullArgs.size()+1];
  std::strcpy(args, fullArgs.c_str());

  STARTUPINFO info = {sizeof(info)};
  PROCESS_INFORMATION processInfo;

  if (!CreateProcess(process.c_str(), args, nullptr, nullptr,
                     TRUE, 0, nullptr, nullptr, &info, &processInfo))
  {
    std::cerr << "Error running the chirp process ["
              << process << "]\n";
  }

  delete[] args;

  return processInfo;
#else
  // Create a raw char* array to pass to execv
  char * * args = new char*[numArgs];

  // Allocate a char array for each argument and copy the data to it
  for (std::size_t i = 0; i < strArgs.size(); ++i)
  {
    const std::string &arg = strArgs[i];
    args[i] = new char[arg.size()+1];
    std::strcpy(args[i], arg.c_str());
  }

  // The last item in the char array must be a nullptr, according to the
  // documentation of execv
  args[numArgs-1] = nullptr;

  testing::forkHandlerType pid = fork();

  if (pid == 0)
  {
    if (execv(process.c_str(), args) == -1)
    {
      int err = errno;
      std::cerr << "Error running the chirp process [" << err << "]: "
                << strerror(err) << "\n";
    }
  }

  // Clean up the array of arguments
  for (std::size_t i = 0; i < numArgs; ++i)
  {
    char *arg = args[i];
    delete[] arg;
    arg = nullptr;
  }
  delete[] args;
  args = nullptr;

  return pid;
#endif
}

//////////////////////////////////////////////////
/// \brief VerifyMessage is intended to be used by the
/// BeginRecordingXxxxBeforeAdvertisement tests.
/// \param _iter The message iterator that we are currently verifying
/// \param _msgCount The number of messages that we have iterated through so far
/// \param _numTopics The number of topics that we are expecting messages from
/// \param VerifyTopic A boolean function that can verify that the topic name is
/// valid.
/// \return True if the message we are viewing is valid.
void VerifyMessage(const ignition::transport::log::MsgIter &_iter,
                   const long int _msgCount,
                   const long int _numTopics,
                   const std::function<bool(const std::string&)> &VerifyTopic)
{
  using MsgType = ignition::transport::log::test::ChirpMsgType;

  const std::string data = _iter->Data();
  const std::string type = _iter->Type();
  EXPECT_FALSE(data.empty());
  EXPECT_FALSE(type.empty());

  EXPECT_TRUE(VerifyTopic(_iter->Topic()));

  MsgType msg;

  EXPECT_EQ(msg.GetTypeName(), type);

  EXPECT_TRUE(msg.ParseFromString(data));

  // The chirps will count starting from 1 (hence the +1) up to numChirps for
  // each topic. We use integer division because it automatically rounds down,
  // which is what we want.
  const long int chirpValue = _msgCount/_numTopics + 1;
  EXPECT_EQ(chirpValue, msg.data());
}

//////////////////////////////////////////////////
/// \brief Begin recording a set of topics before those topics are advertised
/// or published to.
TEST(recordAndPlayback, BeginRecordingTopicsBeforeAdvertisement)
{
  // Remember to include a leading slash so that the VerifyTopic lambda below
  // will work correctly. ign-transport automatically adds a leading slash to
  // topics that don't specify one.
  std::vector<std::string> topics = {"/foo", "/bar"};

  ignition::transport::log::Recorder recorder;
  for (const std::string &topic : topics)
  {
    recorder.AddTopic(topic);
  }

  const std::string logName = IGN_TRANSPORT_LOG_BUILD_PATH"/test.log";
  ignition::common::removeFile(logName);

  EXPECT_EQ(recorder.Start(logName),
            ignition::transport::log::RecorderError::NO_ERROR);

  const int numChirps = 100;
  testing::forkHandlerType chirper = BeginChirps(topics, numChirps);

  // Wait for the chirping to finish
  testing::waitAndCleanupFork(chirper);

  // Wait to make sure our callbacks are done processing the incoming messages
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Stop recording so we can safely view the log
  recorder.Stop();

  ignition::transport::log::Log log;
  EXPECT_TRUE(log.Open(logName));

  ignition::transport::log::MsgIter iter = log.AllMessages();

  auto VerifyTopic = [&](const std::string &_topic)
  {
    for (const std::string &check : topics)
    {
      if (check == _topic)
      {
        return true;
      }
    }

    std::cout << "Unexpected topic name: " << _topic << std::endl;

    return false;
  };

  long int count = 0;

  while (iter != ignition::transport::log::MsgIter())
  {
    VerifyMessage(iter, count,
                  static_cast<long int>(topics.size()),
                  VerifyTopic);

    ++iter;
    ++count;
  }

  EXPECT_EQ(numChirps*static_cast<int>(topics.size()), count);

  ignition::common::removeFile(logName);
}

//////////////////////////////////////////////////
/// \brief Begin recording a set of topics after those topics have been
/// advertised and published to. Some of the initial messages will be missed,
/// so we only test to see that we received the very last message.
TEST(recordAndPlayback, BeginRecordingTopicsAfterAdvertisement)
{
  std::vector<std::string> topics = {"/foo", "/bar"};

  const std::string logName = IGN_TRANSPORT_LOG_BUILD_PATH"/test.log";
  ignition::common::removeFile(logName);

  ignition::transport::log::Recorder recorder;

  const int delay_ms = ignition::transport::log::test::DelayBetweenChirps_ms;

  // We want to chirp for this many seconds
  const double secondsToChirpFor = 1.5;

  // ... so this is how many chirps we should emit
  const int numChirps = static_cast<int>(
        std::ceil(secondsToChirpFor * 1000.0/static_cast<double>(delay_ms)));

  testing::forkHandlerType chirper = BeginChirps(topics, numChirps);

  const int waitBeforeSubscribing_ms =
      ignition::transport::log::test::DelayBeforePublishing_ms
      + static_cast<int>(0.1*secondsToChirpFor)*1000;

  std::this_thread::sleep_for(
        std::chrono::milliseconds(waitBeforeSubscribing_ms));

  for (const std::string &topic : topics)
  {
    recorder.AddTopic(topic);
  }

  EXPECT_EQ(recorder.Start(logName),
            ignition::transport::log::RecorderError::NO_ERROR);

  // Wait for the chirping to finish
  testing::waitAndCleanupFork(chirper);

  // Wait to make sure our callbacks are done processing the incoming messages
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Stop the recording so we can safely view the log
  recorder.Stop();

  ignition::transport::log::Log log;
  EXPECT_TRUE(log.Open(logName));

  ignition::transport::log::MsgIter iter = log.AllMessages();

  using MsgType = ignition::transport::log::test::ChirpMsgType;
  MsgType msg;

  std::string data;
  std::string type;

  while (iter != ignition::transport::log::MsgIter())
  {
    data = iter->Data();
    type = iter->Type();
    EXPECT_EQ(msg.GetTypeName(), type);
    ++iter;
  }

  EXPECT_TRUE(msg.ParseFromString(data));
  EXPECT_EQ(numChirps, msg.data());

  ignition::common::removeFile(logName);
}

//////////////////////////////////////////////////
void RecordPatternBeforeAdvertisement(const std::regex &_pattern)
{
  std::vector<std::string> topics = {"/foo1", "/foo2", "/bar1", "/bar2"};

  int numMatchingTopics = 0;
  for (const std::string &topic : topics)
  {
    if (std::regex_match(topic, _pattern))
      ++numMatchingTopics;
  }

  EXPECT_NE(0, numMatchingTopics);

  const std::string logName = IGN_TRANSPORT_LOG_BUILD_PATH"/test.log";
  ignition::common::removeFile(logName);

  ignition::transport::log::Recorder recorder;
  recorder.AddTopic(_pattern);

  EXPECT_EQ(recorder.Start(logName),
            ignition::transport::log::RecorderError::NO_ERROR);

  const int numChirps = 100;
  testing::forkHandlerType chirper = BeginChirps(topics, numChirps);

  // Wait for the chirping to finish
  testing::waitAndCleanupFork(chirper);

  // Wait to make sure our callbacks are done processing the incoming messages
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Stop recording so we can safely view the log
  recorder.Stop();

  ignition::transport::log::Log log;
  EXPECT_TRUE(log.Open(logName));

  ignition::transport::log::MsgIter iter = log.AllMessages();

  auto VerifyTopic = [&](const std::string &_topic)
  {
    if(std::regex_match(_topic, _pattern))
      return true;

    std::cout << "Unexpected topic name: " << _topic << std::endl;
    return false;
  };

  long int count = 0;

  while (iter != ignition::transport::log::MsgIter())
  {
    VerifyMessage(iter, count, numMatchingTopics, VerifyTopic);

    ++iter;
    ++count;
  }

  EXPECT_EQ(numChirps*numMatchingTopics, count);

  ignition::common::removeFile(logName);
}

//////////////////////////////////////////////////
TEST(recordAndPlayback, BeginRecordingPatternBeforeAdvertisement)
{
  RecordPatternBeforeAdvertisement(std::regex(".*foo.*"));
}

//////////////////////////////////////////////////
TEST(recordAndPlayback, BeginRecordingAllBeforeAdvertisement)
{
  RecordPatternBeforeAdvertisement(std::regex(".*"));
}

//////////////////////////////////////////////////
int main(int argc, char **argv)
{
  setenv(ignition::transport::log::SchemaLocationEnvVar.c_str(),
         IGN_TRANSPORT_LOG_SQL_PATH, 1);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}