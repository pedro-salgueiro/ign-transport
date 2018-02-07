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

#ifndef IGNITION_TRANSPORT_LOG_MSGITERPRIVATE_HH_
#define IGNITION_TRANSPORT_LOG_MSGITERPRIVATE_HH_

#include <vector>

#include "ignition/transport/log/Message.hh"
#include "ignition/transport/log/SqlStatement.hh"
#include "src/raii-sqlite3.hh"


using namespace ignition::transport;
using namespace ignition::transport::log;

class ignition::transport::log::MsgIterPrivate
{
  /// \brief constructor
  public: MsgIterPrivate();

  /// \brief constructor
  public: MsgIterPrivate(const std::shared_ptr<raii_sqlite3::Database> &_db,
      const std::shared_ptr<std::vector<SqlStatement>> &_statements);

  /// \brief destructor
  public: ~MsgIterPrivate();

  /// \brief Executes the statement once
  public: void StepStatement();

  /// \brief Prepares the next statement to be executed
  /// \return true if the statement was sucessfully prepared
  public: bool PrepareNextStatement();

  /// \brief a statement that is being stepped
  public: std::unique_ptr<raii_sqlite3::Statement> statement;

  /// \brief which statement is the msg iterator iterating on
  public: std::size_t statementIndex = 0;

  /// \brief The database this iterator is getting its data from
  public: std::shared_ptr<raii_sqlite3::Database> db;

  /// \brief statements used to get messages from the database
  public: std::shared_ptr<std::vector<SqlStatement>> statements;

  /// \brief the message this iterator is at
  public: std::unique_ptr<Message> message;
};

#endif